#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char *argv[]){
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);
    uint8 dummy_byte = 0;

    int pid;
    if(fork() > 0){ // parent
        wait((int *) 0);
        pid = getpid();
        write(p1[0], &dummy_byte, 1);

        read(p2[1], &dummy_byte, 1);
        fprintf(1, "%d: received pong\n", pid);
    }else{ // child
        pid = getpid();
        read(p1[1], &dummy_byte, 1);
        fprintf(1, "%d: received ping\n", pid);
        write(p2[0], &dummy_byte, 1);
    }

    exit(0);
}