#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// copy from user/grep.c
// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}


// copy and changed from user/ls.c
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1] = {0};
  memset(buf, 0, DIRSIZ+1); // 每次清零一下!
  char *p;

  // Find first character after last slash. 从后向前获取当前文件名
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  // memset(buf+strlen(p), ' ', DIRSIZ-strlen(p)); // buf后面加上一串' ', 总长度为14
  // 直接返回当前路径下的文件名
  return buf;
}

void find(char* path, char *re)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){ // 打开路径的这个文件(夹)
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){ // 获取文件状态
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:  // 对于文件: 输出名字/路径, 状态(1为文件夹, 2为文件, 3为设备), inode, 大小
    // printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    // 文件名字和寻找rc相同, 找到就输出路径
    if(match(re, fmtname(path))){
        printf("%s\n", path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");  // 最大支持14级路径, 最长512的buf
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';   // 当前路径最后加上/, p指向下一个0
    while(read(fd, &de, sizeof(de)) == sizeof(de)){ // 获取文件夹中所有文件, 依次存入到de中
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);  // 把文件名拷贝到buf后面去, 会覆盖上一次的, buf一般是./ls 这种
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){   // 根据路径buf, 获取status 存入st中
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
    //   printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);  // 根据st输出对应的文件信息
    // 在当前路径递归寻找re, 找到就输出全路径

      char* lstname = fmtname(buf);
      if(strcmp(lstname, ".") == 0 || strcmp(lstname, "..") == 0){
          continue;
      }
      // 如果当前文件是目录, 进入目录来递归查找
      // 如果当前文件是文件, 进入文件递归也会查找成功的
      // else if(match(re, fmtname(buf))){
      //   printf("%s\n", buf);
      // }
      else{
          find(buf, re);
      }
      // if(match(re, fmtname(buf+2))){
      //   printf("%s\n", buf);
      // }
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
    if(argc < 2){
        printf("USE: find path expression\n");
    }else{
        find(argv[1], argv[2]);
    }
    exit(0);
}
