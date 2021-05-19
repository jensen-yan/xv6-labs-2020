#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    char *argv2[MAXARG];
    char *cmd;
    if(argc == 1){
        // 默认echo
        cmd = "echo";
    }else{
        cmd = argv[1];
    }
    argv2[0] = "echo";
    argv2[1] = "hello";
    argv2[2] = 0;

    int pid = fork();
    if(pid == 0){
        exec(cmd, argv2);
    }else{
        wait(0);
    }
    exit(0);
}
