#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;    // 数据块个数
  int block[LOGSIZE];   // 长为30的扇区号数组, 每个对应日志的数据块 
};

struct log {
  struct spinlock lock;   // 只读syscall可以并发, 对写不能并发
  int start;    // 起始地址
  int size;     // log大小
  int outstanding; // how many FS sys calls are executing. 有多少次fs的系统调用正在被执行, 未完成
  int committing;  // in commit(), please wait. 正在commit, 等待
  int dev;
  struct logheader lh;
};
struct log log;   // 全局的日志log结构, 日志起始块, 内存, 磁盘各一份

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;   // 把superblock信息更新到log中, 默认2
  log.size = sb->nlog;  // 默认30
  log.dev = dev;
  recover_from_log();   // 从上次log中恢复文件系统, 如果没崩溃, 不管
}

// Copy committed blocks from log to their home location
// 查log: 把提交了的块恢复到磁盘对应位置去
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block 读log数据块
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst 读目的地, log头指向的数据块
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst 把block移动到目的地
    bwrite(dbuf);  // write dst to disk 写入磁盘中
    if(recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
// 从磁盘中读取log头块, 放入内存中全局log头
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);  // 默认从2号块读取
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
// 把内存的log头(日志起始块)写入磁盘
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;   // 内存中的log头更新到buf中, 再写入磁盘中
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log 把log清空
}

// called at the start of each FS system call.
// 每次fs 的syscall会先begin_op. 等到独占日志使用权后返回(log.lock解锁后)
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){   // 正在commit中 / 用完了log空间, sleep等待commit
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
// fs syscall 最后调用end_op(). 如果是最后一个未完成的操作, commit
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;   // 和begin_op成对出现(可能两次begin_op后两次end_op), 后一次为0 commit
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    // 可能log空间满了, begin_op的进程孩子睡着, 唤醒它
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block 日志的数据块, 3,4,5等
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block 读取日志头block指针,指向的修改后的数据块, 33,56等, 读出的是缓存块, 不是磁盘的!
    memmove(to->data, from->data, BSIZE);   // 把当前缓存过的磁盘块新内容(更新还没写入磁盘), 拷贝到日志的数据块中
    bwrite(to);  // write the log, 日志数据块写入磁盘上
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {   // 有日志数据块时候, 更新到磁盘
    write_log();     // Write modified blocks from cache to log 对修改块, 从bcache写入磁盘log 数据块中. 
    write_head();    // Write header to disk -- the real commit 把内存的日志头写入磁盘, 写一个特殊提交记录到磁盘中, 代表完成的操作
    install_trans(0); // Now install writes to home locations   把日志数据块写入磁盘的data block中
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log      删除磁盘的日志文件(log.lh.n = 0了)
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[] 读bcache, 修改, 写入log, 释放bcache
//   log_write(bp)
//   brelse(bp)
// 调用者修改了bcache数据, 记录块号, refcnt++, commit/write_log 才把真正修改写入磁盘中
// 把块中新内容记录到日志中(好像没记录新内容?), 把块的扇区号记录到内存log中
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion, 找到记录要更新的扇区号的块 对应的 日志记录数据块
      break;
  }
  log.lh.block[i] = b->blockno; // 如果找不到, 记录更新块的扇区号到log中新的数据块, log.h.n++
  if (i == log.lh.n) {  // Add new block to log? 新加一块到log中
    bpin(b);    // pin住buf, refcnt++, 提交前这个块被修改过一次, 不被替换
    log.lh.n++;
  }
  release(&log.lock);
}

