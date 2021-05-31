
进程调度这一块还没有弄得很明白
ls.c 中最后exit(0) 只是释放文件资源, 变成僵尸态, 还需要等待父进程(initcode = shell) 来调用wait来释放进程资源
exit最后会进入sched()函数, 执行swtch(&p->context, &mycpu()->context);
这里的mycpu-> context到底是什么呢, 似乎返回到scheduler()函数那里, 然后页表切换回全局的内核页表, 在内核栈中运行
这里的内核栈应该是shell进程的内核栈吧, 何时调用wait?
scheduler 的进程切换前后是否应该切换页表, 还没想太清楚

bug1:
ls 后 panic: freewalk: leaf
调用freewalk前要释放所有叶节点, 之前没解除完关系

bug2:
第二次ls, panic: remap
是mappages中映射页表可能: 应该写到新进程页表, 却写入了全局页表中
原因: kalloc申请内核栈写入全局页表中, 出现了重复映射, 因为原来的内核栈页被释放了? 又写到同一个地方?
原来内核栈proc[2] 那里写入了一个内核栈映射, 现在进程结束了, 但是内核栈映射却没有删除, 导致重复映射了
要不内核栈映射就不做了? 但是这里出现kvmpa: 在全局页表中对3ffffffdf10地址的映射*pte的valid=0
好吧, 直接修改kvmpa中的地方, 需要修改成调用当前内核页表!
注意需要先定义spinlock, 然后才是proc.h, 声明顺序居然也有要求, 服了

终于完成任务2了, 确实有难度, 继续加油, 我很棒!