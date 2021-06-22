// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;   // 全局的, 暂时只用到全局锁和30个buf

struct bucket{
  struct spinlock lock;
  struct buf head;
} buckets[NBUCKET];    // 13个hash桶, 一个锁+ 链表头

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++)
  {
    struct bucket *buk = &buckets[i];   // 指向这个桶
    initlock(&buk->lock, "bcache.bucket");
    buk->head.prev = &buk->head;  // 初始化这个桶的头节点
    buk->head.next = &buk->head;
    if(i == 0){   // 先把全部buf存入桶0中
      for(b = bcache.buf; b < bcache.buf+NBUF; b++){
        b->next = buk->head.next;
        b->prev = &buk->head;
        initsleeplock(&b->lock, "buffer");
        buk->head.next->prev = b;
        buk->head.next = b;
      }
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);  // 有全局大锁

  const uint key = blockno % NBUCKET;   // hash key
  // Is the block already cached?
  // block是否被缓存过, 直接在对应桶中找
  acquire(&buckets[key].lock);
  for(b = buckets[key].head.next; b != &buckets[key].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buckets[key].lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 没被缓存过, 找这个桶中的空闲的. 如果没有, 找桶0, 没有, 找桶1向后找
  for(b = buckets[key].head.prev; b != &buckets[key].head; b = b->prev){
    if(b->refcnt == 0){
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&buckets[key].lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // release(&buckets[key].lock);   // 继续持有key的锁!
  // 这个桶没找到, 从0桶开始向后面找, 如果找到, 要放到key桶中!
  for (int i = 0; i < NBUCKET; i++)
  {
    if(i == key) continue;
    acquire(&buckets[i].lock);  // 这个桶重新加锁
    for(b = buckets[i].head.prev; b != &buckets[i].head; b = b->prev){
      if(b->refcnt == 0){
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 从i桶搬到key桶中
        struct buf *b_pre = b->prev;
        b->prev->next = b->next;
        b->next->prev = b_pre;
        b->next = buckets[key].head.next;
        b->prev = &buckets[key].head;
        buckets[key].head.next->prev = b;
        buckets[key].head.next = b;
        
        release(&buckets[i].lock);    // 顺序释放3个锁
        release(&buckets[key].lock);  
        // release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&buckets[i].lock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = b->blockno % NBUCKET;
  acquire(&buckets[key].lock);
  b->refcnt--;
  // refcnt为0, 不做事情
  release(&buckets[key].lock);
}

void
bpin(struct buf *b) {
  uint key = b->blockno % NBUCKET;
  acquire(&buckets[key].lock);
  b->refcnt++;
  release(&buckets[key].lock);
}

void
bunpin(struct buf *b) {
  uint key = b->blockno % NBUCKET;
  acquire(&buckets[key].lock);
  b->refcnt--;
  release(&buckets[key].lock);
}


