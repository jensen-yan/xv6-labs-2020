struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode 内存中的inode, 是磁盘dinode的拷贝(内核有C指针指向时候才拷贝)
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number 应该是内存中的逻辑inum, 和磁盘上扇区号区别!
  int ref;            // Reference count 记录有多少个内核C指针指向inode, iget, iput会申请和释放inode指针
  struct sleeplock lock; // protects everything below here 睡眠锁保护下面的所有内容
  int valid;          // inode has been read from disk? inode是否是从磁盘中读出的备份?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
