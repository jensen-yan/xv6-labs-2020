#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0) // 通过argint获取第0个参数int
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask = 0;   // 传入的系统调用掩码
  if(argint(0, &mask) < 0)
    return -1;
  // printf("mask %d\n", mask);
  // 传递掩码到proc中
  // 让fork跟踪掩码, 从父进程复制到子进程?
  // 因为之后用exec调用grep, grep调用read, 就是子进程了
  struct proc *p = myproc();
  memset(p->masks, 0, sizeof(p->masks));
  int i = 0;
  while(mask > 0){
    if(mask % 2){
      p->masks[i] = '1';
    }else{
      p->masks[i] = '0';
    }
    i++;
    mask >>= 1;
  }
  // printf("masks = %s\n", p->masks);
  return 0;
}

uint64 
sys_sysinfo(void){
  // 获取结构体
  uint64 addr;  // 指向sysinfo结构体
  struct sysinfo info;
  if(argaddr(0, &addr) < 0)
    return -1;
  // 获取可用内存量
  info.freemem = freemem_size();
  // 获取进程数
  info.nproc = proc_num();
  // 返回到用户空间
  struct proc *myp = myproc();
  // 把info内容传递出去给addr地址
  if(copyout(myp->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;

  return 0;
}