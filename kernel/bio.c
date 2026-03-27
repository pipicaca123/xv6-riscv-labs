// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 7

// using integration of hash table + LRU to avoid collision
struct hashtable_t {
  struct buf buckets[NBUCKET];
  struct spinlock lock[NBUCKET];
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct hashtable_t buf_hashtable;
} bcache;

void init_bufhashtable() {
  for (int i = 0; i < NBUCKET; i++) {
    char bucket_lockname[16];
    struct buf *bcache_bucket = &bcache.buf_hashtable.buckets[i];
    memset(bcache_bucket, 0, sizeof(*bcache_bucket));
    bcache_bucket->next = bcache_bucket; // init sentinel point
    bcache_bucket->prev = bcache_bucket; // init sentinel point
    snprintf(bucket_lockname, sizeof(bucket_lockname), "bcachebucket%d", i);
    initlock(&bcache.buf_hashtable.lock[i], bucket_lockname);
  }
  for (int i = 0; i < NBUF; i++) {
    struct buf *bcache_bucket = &bcache.buf_hashtable.buckets[i % NBUCKET];
    bcache.buf[i].next = bcache_bucket->next;
    bcache.buf[i].prev = bcache_bucket;
    bcache_bucket->next->prev = &bcache.buf[i];
    bcache_bucket->next = &bcache.buf[i];
  }
}

uint8 hash_func(uint blockno) { return blockno % NBUCKET; }

struct buf *hash_insert(uint blockno, uint dev) {
  uint h = hash_func(blockno);
  for (int i = 0; i < NBUCKET; i++, h = (h + 1) % NBUCKET) {
    struct buf *bucket = &bcache.buf_hashtable.buckets[h];
    struct spinlock *bucketlock = &bcache.buf_hashtable.lock[h];
    struct buf *sentinel = bucket;
    acquire(bucketlock);
    while (bucket->next != sentinel) {
      bucket = bucket->next;
      if (bucket->dev == dev && bucket->blockno == blockno) {
        // printf("hash_insert: find re-insert buf (ref=%d)\n", bucket->refcnt);
        release(bucketlock);
        return bucket;
      } else if (bucket->refcnt == 0) { // this buf is no one using, eviction.
        release(bucketlock);
        return bucket;
      }
    }
    release(bucketlock);
    // if not found, search nearest bucket and try to insert it
  }
  return (struct buf *)0;
}

struct buf *hash_search(uint blockno, uint dev) {
  uint h = hash_func(blockno);
  for (int i = 0; i < NBUCKET; i++, h = (h + 1) % NBUCKET) {
    struct buf *bucket = &bcache.buf_hashtable.buckets[h];
    struct spinlock *bucketlock = &bcache.buf_hashtable.lock[h];
    struct buf *sentinel = bucket;
    acquire(bucketlock);
    while (bucket->next != sentinel) {
      bucket = bucket->next;
      if (bucket->dev == dev && bucket->blockno == blockno) {
        release(bucketlock);
        return bucket;
      }
    }
    release(bucketlock);
  }
  return (struct buf *)0; // no buf find, no cached
}
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers

  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
  }
  init_bufhashtable();
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  b = hash_search(blockno, dev);
  if (b) { // cached
    b->refcnt++;
    acquiresleep(&b->lock);
    return b;
  }
  acquire(&bcache.lock); // avoid multithread insert same block simutanously
  b = hash_insert(blockno, dev);
  if (!b)
    panic("bget: no buffers");
  if (b->refcnt != 0) {
    // another thread has register this buf dev-blockno already
    b->refcnt++;
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


