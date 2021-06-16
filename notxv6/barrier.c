#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;  // 一个互斥锁
  pthread_cond_t barrier_cond;    // 一个条件变量
  int nthread;      // Number of threads that have reached this round of the barrier 到达屏障的线程数目
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  // 阻塞直到所有线程都调用了barrier(), round++
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;   // 新来个线程执行到这里, 加加

  if(bstate.nthread == nthread){  // 最后一个线程更新状态
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);   // 唤醒其他进程
  }else{
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);   // 前面线程阻塞在这里
  }

  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();  // 同步后, 同步调用barrier()
    usleep(random() % 100); // 随机睡眠
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();   // 初始化barrier

  for(i = 0; i < nthread; i++) {  // 每个线程执行thread
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
