#include "kernel/types.h"
#include "user/user.h"
#include <stdint.h>

#define MAX_DATA 35


static void gen_buf(int *buf, int buf_len, int *msg, int *msg_len){
    *msg_len = 0;
    for(int i=0; i<buf_len;i++){
        if(buf[i] % buf[0] != 0){
            msg[(*msg_len)++] = buf[i];
        }
    }
}

int main(int argc, char *argv[]){
    int buf[MAX_DATA - 1], buf_len = sizeof(buf) / sizeof(buf[0]);
    int msg[MAX_DATA - 1], msg_len;
    for(int i=2, j=0; i< MAX_DATA+1; i++,j++)
        buf[j] = i;
    int seed = 2;

    while(buf_len >= 1){
        int p_fd[2];
        pipe(p_fd);
        printf("prime %d\n", seed);
        gen_buf(buf, buf_len, msg,&msg_len);
        buf_len = msg_len;
        write(p_fd[1], msg, msg_len  * sizeof(int));
        close(p_fd[1]);
        if(fork()>0){
            wait((int *) 0);
            close(p_fd[0]);
            break;
        }else{
            read(p_fd[0], buf, buf_len  * sizeof(int));
            close(p_fd[0]);
            seed = buf[0];
        }

    }
    exit(0);
}