// Long-term locks for processes
struct sleeplock {  // 睡眠锁= 自旋锁lk + locked变量
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

