#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main()
{
    // 两个进程, 一对管道, 传递一个byte
    // 父进程 写1byte 给 parent_fd[1] 来发送
    // 子进程 阅读 parent_fd[0] 来接受
    // 子 收到后, 写1byte 给 child_fd[1] , 然后父进程读取

    // 使用pipe创建管道, fork创建孩子, read, write 读写管道
    
    int parent_fd[2] = {0, 0};
    int child_fd[2] = {0, 0};
    char buf[20];
    pipe(parent_fd);
    pipe(child_fd); // 建立管道

    int pid = fork();
    if(pid > 0){
        // parent
        close(parent_fd[0]);
        write(parent_fd[1], "ping", 4);
        close(child_fd[1]);
        read(child_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
    }else if(pid == 0){
        // child
        close(parent_fd[1]);   // 关闭标准输入
        read(parent_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        close(child_fd[0]);
        write(child_fd[1], "pong", sizeof(buf));
    }else{
        printf("fork error\n");
    }

    exit(0);
}
