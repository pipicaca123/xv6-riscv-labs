#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(2, "Usage: user sleep needs one parameter...\n");
    }
    int secs = atoi(argv[1]);
    sleep(secs);
    exit(0);
}