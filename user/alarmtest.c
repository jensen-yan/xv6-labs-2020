//
// test program for the alarm lab.
// you can modify this file for testing,
// but please make sure your kernel
// modifications pass the original
// versions of these tests.
//

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

void test0();
void test1();
void test2();
void periodic();
void slow_handler();

int
main(int argc, char *argv[])
{
  test0();
  test1();
  test2();
  exit(0);
}

volatile static int count;  // 全局变量

void
periodic()
{
  count = count + 1;
  printf("alarm!\n"); // 变量加1并打印
  sigreturn();  // 返回到发生中断的地方
}

// tests whether the kernel calls
// the alarm handler even a single time.
// 测试是否内核调用了alarm, 即便一次也要测试
void
test0()
{
  int i;
  printf("test0 start\n");
  count = 0;
  sigalarm(2, periodic);  // 处理函数为periodic, 两次中断后进入处理函数?
  for(i = 0; i < 1000*500000; i++){ // 循环5亿次
    if((i % 1000000) == 0)
      write(2, ".", 1);   // 每一百万打印一个点
    if(count > 0) // count+1之后就跳出去
      break;
  }
  sigalarm(0, 0); // 内核停止生成周期性的alarm调用, 设置为ticks=0后, 不会去调用handler处理函数了
  if(count > 0){
    printf("test0 passed\n");
  } else {
    printf("\ntest0 failed: the kernel never called the alarm handler\n");
  }
}

void __attribute__ ((noinline)) foo(int i, int *j) {
  if((i % 2500000) == 0) {
    write(2, ".", 1);
  }
  *j += 1;
}

//
// tests that the kernel calls the handler multiple times.
//
// tests that, when the handler returns, it returns to
// the point in the program where the timer interrupt
// occurred, with all registers holding the same values they
// held when the interrupt occurred.
// 测试, 但处理函数返回是, 回到中断发生的地方, 所有寄存器保持不变
void
test1()
{
  int i;
  int j;

  printf("test1 start\n");
  count = 0;
  j = 0;
  sigalarm(2, periodic);  // 设置ticks=2, 到时间调用handler
  for(i = 0; i < 500000000; i++){
    if(count >= 10) // 总共用户中断函数处理10次
      break;
    foo(i, &j); // j++, i = j始终成立
  }
  if(count < 10){
    printf("\ntest1 failed: too few calls to the handler\n");
  } else if(i != j){
    // the loop should have called foo() i times, and foo() should
    // have incremented j once per call, so j should equal i.
    // once possible source of errors is that the handler may
    // return somewhere other than where the timer interrupt
    // occurred; another is that that registers may not be
    // restored correctly, causing i or j or the address ofj
    // to get an incorrect value.
    // 如果中断处理函数没有正确返回中断发生处, 或者寄存器恢复错了, 失败
    printf("\ntest1 failed: foo() executed fewer times than it was called\n");
  } else {
    printf("test1 passed\n");
  }
}

//
// tests that kernel does not allow reentrant alarm calls.
// 测试内核不允许重复调用handler, 如果没放好
void
test2()
{
  int i;
  int pid;
  int status;

  printf("test2 start\n");
  if ((pid = fork()) < 0) {
    printf("test2: fork failed\n");
  }
  if (pid == 0) {
    count = 0;
    sigalarm(2, slow_handler);  // 子进程slow_handler
    for(i = 0; i < 1000*500000; i++){
      if((i % 1000000) == 0)
        write(2, ".", 1);
      if(count > 0) // handler会让count++ 跳出循环
        break;
    }
    if (count == 0) {
      printf("\ntest2 failed: alarm not called\n");
      exit(1);
    }
    exit(0);
  }
  wait(&status);  // 父进程等子进程结束, 把返回状态写入status中
  if (status == 0) {
    printf("test2 passed\n");
  }
}

void
slow_handler()
{
  count++;
  printf("alarm!\n");
  if (count > 1) {  // 子进程不能count++两次
    printf("test2 failed: alarm handler called more than once\n");
    exit(1);
  }
  for (int i = 0; i < 1000*500000; i++) {
    asm volatile("nop"); // avoid compiler optimizing away loop
    // 等长时间, 在没有return前, 又有ticks++=2了, 再次进入handler函数处理, 让count=2错误
  }
  sigalarm(0, 0);   // 停止调用handler
  sigreturn();
}
