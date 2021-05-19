#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

    // atoi 获取sleep时间
    if(argc != 2){
        printf("please input sleep time(int) \n");
        printf("USE: sleep 10\n");
        exit(0);
    }
    int time = atoi(argv[1]);
    // syscall sleep()
    sleep(time);

    // exit()
    exit(0);
}
