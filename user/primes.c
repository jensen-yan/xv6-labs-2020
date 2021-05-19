#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void send_primes(int fd[2], int infos[], int infoslen){
    close(fd[0]);
    int info = 0;
    for (int i = 0; i < infoslen; i++)
    {
        info = infos[i];
        write(fd[1], &info, sizeof(info));
    }
    
}

void process(int fd[2]){
    int child_fd[2];
    pipe(child_fd);

    int infos[34];
    int infos_i = 0;

    int p = 0;
    close(fd[1]);
    int len = read(fd[0], &p, sizeof(p));
    if(len == 0 || p == 0) exit(0);
    printf("prime %d\n", p);

    int n = 0;
    while (len != 0)
    {   
        len = read(fd[0], &n, sizeof(n));
        if( n % p != 0){
            infos[infos_i] = n;
            infos_i++;
        }
    }
    
    int pid = fork();
    if(pid == 0){
        process(child_fd);
    }else{
        send_primes(child_fd, infos, infos_i);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    int fd[2];
    pipe(fd);

    int pid = fork();
    if(pid == 0){
        process(fd);
    }else{
        int nums[34];
        for (int i = 0; i < 34; i++)
        {
            nums[i] = i+2;
        }
        send_primes(fd, nums, 34);
    }
    exit(0);
}
