#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
// #define NKEYS 100000
#define NKEYS 10000

struct entry {
  int key;
  int value;
  struct entry *next;
};  // 键值对的链表
struct entry *table[NBUCKET]; // 分成5个桶, 每个通指向一个链表
int keys[NKEYS];
int nthread = 1;

double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;   // 新建节点e, 指向链表, 头指向e
}

static 
void put(int key, int value)
{
  int i = key % NBUCKET;  // 分成5个桶

  // is the key already present? 先看key是否存在
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key. key存在就覆盖value, 好吧
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);  // 把键值对存入桶中
  }
}

static struct entry*
get(int key)
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;   // 顺序查找链表直到找到对应key节点, 返回
  }

  return e;
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number 线程号码
  int b = NKEYS/nthread;

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);  // 每个线程只put对应部分的key(随机生成的)
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);  // 获取线程数目
  tha = malloc(sizeof(pthread_t) * nthread);  // 存放线程号的数组空间, tha指向它
  srandom(0);
  assert(NKEYS % nthread == 0); // 1,2,4,10 要被10万整除才行
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();   // 随机初始化10万个key
  }

  //
  // first the puts 先puts
  //
  t0 = now(); // 获取时间, s为单位
  for(int i = 0; i < nthread; i++) {  // 返回线程号存入tha[i], null, 执行函数指针, 参数i为执行函数参数. 成功返回0
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {  // 主线程等待其他线程结束, 返回值存入value
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
