# Context switch
#
#   void swtch(struct context *old, struct context *new);
# 把当前上下文存入old中, 从new中恢复出上下文. 上下文应该存储在数据段中
# Save current registers in old. Load from new.	


.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        
        ret
        # pc = 新的ra, 切换到新的上下文代码处, sp指向新的栈
        # eg1: 对于sched调用, ra会返回到scheduler调用地方, 继续运行调度线程, sp指向调度线程的栈
        # eg2: 对于scheduler调用, ra返回到sched调用处, 继续运行新线程, sp指向新线程内核栈

	
