#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "user/user.h"

void test0();
void test1();

#define SZ 4096
char buf[SZ];

int
main(int argc, char *argv[])
{
  test0();
  test1();
  exit(0);
}

void
createfile(char *file, int nblock)
{
  int fd;
  char buf[BSIZE];
  int i;
  
  fd = open(file, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("createfile %s failed\n", file);
    exit(-1);
  }
  for(i = 0; i < nblock; i++) {
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf("write %s failed\n", file);
      exit(-1);
    }
  }
  close(fd);
}

void
readfile(char *file, int nbytes, int inc)
{
  char buf[BSIZE];
  int fd;
  int i;

  if(inc > BSIZE) {
    printf("readfile: inc too large\n");
    exit(-1);
  }
  if ((fd = open(file, O_RDONLY)) < 0) {
    printf("readfile open %s failed\n", file);
    exit(-1);
  }
  for (i = 0; i < nbytes; i += inc) {
    if(read(fd, buf, inc) != inc) {
      printf("read %s failed for block %d (%d)\n", file, i, nbytes);
      exit(-1);
    }
  }
  close(fd);
}

// 获取统计数据, print=1打印buf内容
int ntas(int print)
{
  int n;
  char *c;

  if (statistics(buf, SZ) <= 0) {
    fprintf(2, "ntas: no stats\n");
  }
  c = strchr(buf, '=');
  n = atoi(c+2);
  if(print)
    printf("%s", buf);
  return n;
}

void
test0()
{
  char file[2];
  char dir[2];
  enum { N = 10, NCHILD = 3 };  // 3个孩子, 10个数
  int m, n;

  dir[0] = '0';   // 一个目录0, 一个文件F
  dir[1] = '\0';
  file[0] = 'F';
  file[1] = '\0';

  printf("start test0\n");
  for(int i = 0; i < NCHILD; i++){
    dir[0] = '0' + i;
    mkdir(dir);
    if (chdir(dir) < 0) {   // 创建目录, 改变当前目录到dir = "i"
      printf("chdir failed\n");
      exit(1);
    }
    unlink(file);   // 删除文件
    createfile(file, N);  // 创建大小为10block的文件
    if (chdir("..") < 0) {  // 进入上个目录
      printf("chdir failed\n");
      exit(1);
    }
  }
  m = ntas(0);
  for(int i = 0; i < NCHILD; i++){
    dir[0] = '0' + i;
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid == 0){
      if (chdir(dir) < 0) {   // 3个孩子进入3个目录
        printf("chdir failed\n");
        exit(1);
      }

      readfile(file, N*BSIZE, 1); // 竞争的从file文件中读数据

      exit(0);
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }
  printf("test0 results:\n");
  n = ntas(1);
  if (n-m < 500)  // 竞争数不要太大就好
    printf("test0: OK\n");
  else
    printf("test0: FAIL\n");
}

void test1()
{
  char file[3];
  enum { N = 100, BIG=100, NCHILD=2 };  // 两个孩子
  
  printf("start test1\n");
  file[0] = 'B';
  file[2] = '\0';
  for(int i = 0; i < NCHILD; i++){
    file[1] = '0' + i;
    unlink(file);
    if (i == 0) {
      createfile(file, BIG);  // 创建大文件B0, 小文件B1
    } else {
      createfile(file, 1);
    }
  }
  for(int i = 0; i < NCHILD; i++){
    file[1] = '0' + i;
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid == 0){
      if (i==0) {
        for (i = 0; i < N; i++) {
          readfile(file, BIG*BSIZE, BSIZE);   // 一个孩子读大文件, 另个读小文件
        }
        unlink(file);
        exit(0);
      } else {
        for (i = 0; i < N; i++) {
          readfile(file, 1, BSIZE);
        }
        unlink(file);
      }
      exit(0);
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }
  printf("test1 OK\n");
}
