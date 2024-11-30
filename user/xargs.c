/**
 * TODO: extend to (echo 1 ; echo 2) | xargs -n 1 echo, see in doc.
 */
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_FS_READ_SIZE 512 // is this good?

char tmp_buf[MAX_FS_READ_SIZE];
int main(int argc, char *argv[]){
    if(argc > MAXARG){
        fprintf(2, "get argument out of legal range, argc_cnt=%d\n", argc);
        exit(1);
    }
    char std_rd_buf[MAX_FS_READ_SIZE], *std_rd_ptr;
    char *argv_exec[MAXARG], *argv_buf[MAXARG];
    int exec_cnt = 0;

    std_rd_ptr = std_rd_buf;

    int cnt_flg;
    char *tmp_rd_ptr = std_rd_ptr;
    do{
        cnt_flg = read(0, tmp_rd_ptr, MAX_FS_READ_SIZE);
        tmp_rd_ptr += cnt_flg;
    }while(cnt_flg > 0);

    for(int idx = 0; idx < strlen(std_rd_ptr); idx++){
        if(std_rd_ptr[idx] == '\n'){
            std_rd_ptr[idx] = '\0';
            argv_buf[exec_cnt++] = std_rd_ptr;
            std_rd_ptr += (++idx);
            idx = 0;
        }
    }

    argv_exec[0] = argv[1];
    argv_exec[1] = argv[2];
    argv_exec[3] = 0;
    for(int i=0;i<exec_cnt;i++){
        if(fork() > 0){
            wait((int *) 0);
        }else{
            argv_exec[2] = argv_buf[i];
            exec(argv_exec[0], argv_exec);
        }
    }

    exit(0);
}