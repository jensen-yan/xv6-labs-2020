// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
// mkfs 计算 superblock并构建一个初始的文件系统
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
  // bit map 默认一块吗? data block的起始号 = bmapstart + bmapSize
};

#define FSMAGIC 0x10203040

#define NDIRECT 12    // 最大12个直接块
#define NINDIRECT (BSIZE / sizeof(uint))    // 256个间接块
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure 磁盘上的inode数据结构
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system 指向这个inode节点的目录项数目
  uint size;            // Size of file (bytes) 文件的字节数
  uint addrs[NDIRECT+1];   // Data block addresses 文件存放在数据块上的位置数组
};

// Inodes per block. 每块有很多个dinode结构
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

