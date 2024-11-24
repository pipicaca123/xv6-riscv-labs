#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define MAX_PATH_LENG 512

void find(char *path, char *filename){
    char buf[MAX_PATH_LENG], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path,O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
  }

    switch (st.type) {
    case T_DIR:
        if(strlen(path)+1+1+sizeof(DIRSIZ) > sizeof(buf)){
            // path + '\0' + '\' + dirent.name[DIRSIZE]
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(path);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0) // see in 8.11 Code:directory layer
                continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if(st.type == T_DIR){
            if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            find(buf, filename);
        }
        else{
            if(strcmp(filename, de.name) == 0){
                printf("%s\n", buf);
            }
        }
            
        }

    break;
    default:
        fprintf(1, "find: you might pass file/device name here!\n");
    break;
    }

    close(fd);
    return;
}

int main(int argc, char *argv[]){
    if(argc < 3){
        fprintf(2,"find should carry path arg. & file arg.\n");
        exit(1);
    }
        find(argv[1],argv[2]);
    exit(0);
}