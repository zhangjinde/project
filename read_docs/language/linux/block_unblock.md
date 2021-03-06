### 阻塞、非阻塞
1. 阻塞与非阻塞是设备访问的两种方式。驱动程序需要提供阻塞（等待队列，中断）和非阻塞方式（轮询，异步通知）访问设备。（设备？设备与fd区别）
2. 阻塞是指没有获得资源则挂起进程，直到获得资源为止。被挂起的进程进入休眠状态，被调度器的运行队列移走，直到等待条件被满足。
3. 非阻塞是不能进行设备操作时不挂起，或放弃，或反复查询，直到可以进行操作为止。
    1) 阻塞不是低效率，如果设备驱动不阻塞， 用户想获取设备资源只能不断查询，消耗CPU资源，阻塞访问时，不能获取资源的进程将进入休眠，将CPU资源让给其他资源。
    2) 阻塞的进程会进入休眠状态，因此，必须确保有一个地方能唤醒休眠的进程。唤醒进程的地方最大可能发生在中断里面，因为硬件资源获得的同时往往伴随着一个中断。

### 同步异步


### 同步操作与机制

#### atomic 原子操作
所谓原子操作，就是该操作绝不会在执行完毕前被任何其他任务或事件打断，原子操作需要硬件的支持，因此是架构相关的，其API和原子类型的定义都定义在内核源码树的include/asm/atomic.h文件中。

原子操作主要用于实现资源计数，很多引用计数(refcnt)就是通过原子操作实现的。

#### Spanlock
自旋锁与互斥锁有点类似，只是自旋锁不会引起调用者睡眠，如果自旋锁已经被别的执行单元保持，调用者就一直循环在那里看是否该自旋锁的保持者已经释放了锁。


信号量和读写信号量适合于保持时间较长的情况，它们会导致调用者睡眠，因此只能在进程上下文使用（_trylock的变种能够在中断上下文使用），而自旋锁适合于保持时间非常短的情况，它可以在任何上下文使用。

#### 互斥锁
互斥锁主要用于实现内核中的互斥访问功能。内核互斥锁是在原子API之上实现的。
Linux内核的信号量在概念和原理上与用户态的SystemV的IPC机制信号量是一样的，但是它绝不可能在内核之外使用，因此它与SystemV的IPC机制信号量毫不相干。

#### seqlock 顺序锁
