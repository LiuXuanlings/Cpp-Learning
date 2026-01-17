## 步骤2：无锁空闲链表（CAS核心实现）
### 步骤总览
本步骤是高性能内存池的核心所在。在步骤1能够向系统“进货”的基础上，本步骤将实现内存的“回收与复用”。
为了在多线程高频申请/释放场景下避免锁竞争带来的性能抖动，我们将采用 **Treiber Stack（无锁栈）** 算法管理空闲链表。你将深入操作 `std::atomic`，利用 **CAS（Compare-And-Swap）** 原语实现线程安全的 `push`（归还）和 `pop`（获取）操作。
实现后，内存池将具备 O(1) 复杂度的分配与释放能力，且支持多线程并发访问。

### include/目录
#### MemoryPool.h
##### 改造后代码
```cpp
/* 本文件在步骤2保持步骤1的结构，无需修改接口定义。
   核心关注点是 Slot 结构体中的 atomic 指针，它为 src 中的无锁实现提供支持。
   
   struct Slot {
       std::atomic<Slot*> next; // 原子指针，支撑无锁栈
   };
*/
```
##### 模块讲解
- **功能定位**：虽然代码未变，但理解 `Slot` 的设计至关重要。它是一个**侵入式节点**，当内存块被用户使用时，这块内存存储用户数据；当被归还时，这块内存被强制转换为 `Slot`，利用其头部存储 `next` 指针链接到 `freeList_` 中。
- **场景/面试关联**：
  - **面试题**：为什么内存池中不需要额外的 `Node` 结构体来存链表节点？
  - **答案**：利用**侵入式数据结构**（Intrusive Data Structure），复用空闲内存本身存储链表指针，节省内存开销（Overhead）。

### src/目录
#### MemoryPool.cpp
##### 改造后代码
```cpp
#include "../include/MemoryPool.h"

namespace Kama_memoryPool 
{

// ... (构造函数、析构函数、init同步骤1，此处省略) ...

void* MemoryPool::allocate()
{
    // TODO：【复用逻辑】优先尝试从无锁空闲链表中获取
    // 1. 调用 popFreeList() 获取可用的 Slot
    // 2. 如果获取成功（非空），直接返回该 Slot 指针
    // 3. 如果失败（链表为空），则进入下方的 Block 分配逻辑
    Slot* slot = popFreeList();
    if (slot != nullptr)
        return slot;

    // ... (下方 Block 分配逻辑保持步骤1内容) ...
    Slot* temp;
    {   
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_)
        {
            allocateNewBlock();
        }
        temp = curSlot_;
        curSlot_ += SlotSize_ / sizeof(Slot);
    }
    return temp; 
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;

    // 将用户指针强制转换为 Slot*，以便将其挂回链表
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    pushFreeList(slot);
}

// ... (allocateNewBlock, padPointer 同步骤1，此处省略) ...

// 实现无锁入队操作（头插法）
bool MemoryPool::pushFreeList(Slot* slot)
{
    // TODO：【无锁进栈】使用 CAS 循环将节点插入链表头部
    // 1. 构造一个死循环 (while(true))，因为 CAS 可能会失败需重试
    // 2. load：获取当前 freeList_ 的头节点 oldHead (需用 relaxed 序，提高性能)
    // 3. store：将新节点 slot->next 指向 oldHead (暂未发布的私有操作，relaxed 即可)
    // 4. CAS：调用 compare_exchange_weak 尝试将 freeList_ 更新为 slot
    //    - 成功条件：freeList_ 仍等于 oldHead
    //    - 内存序：成功时需 release (保证 slot->next 的写入对其他线程可见)
    //    - 失败时：自动更新 oldHead 为最新的 freeList_，循环重试
    
    while (true)
    {
        // 获取当前头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        
        // 将新节点的 next 指向当前头节点
        slot->next.store(oldHead, std::memory_order_relaxed);

        // 尝试将新节点设置为头节点
        // oldHead 是期望值，slot 是新值
        if (freeList_.compare_exchange_weak(oldHead, slot,
         std::memory_order_release, std::memory_order_relaxed))
        {
            return true;
        }
        // 失败：说明在此期间另一个线程修改了 freeList_，oldHead 被自动更新为最新值
        // CAS 失败则 continue 重试
    }
}

// 实现无锁出队操作
Slot* MemoryPool::popFreeList()
{
    // TODO：【无锁出栈】使用 CAS 循环从链表头部取出一个节点
    // 1. 构造死循环重试机制
    // 2. load：获取当前头节点 oldHead (需用 acquire 序，确保看到 push 时的写入)
    // 3. 判空：如果 oldHead 为 nullptr，说明池中无可用内存，返回 nullptr
    // 4. 读取 newHead：尝试读取 oldHead->next 作为新的头节点
    //    - 注意：需考虑异常安全或指针有效性（原项目代码在此处使用了 try-catch）
    // 5. CAS：调用 compare_exchange_weak 尝试将 freeList_ 从 oldHead 更新为 newHead
    //    - 内存序：成功 acquire，失败 relaxed
    
    while (true)
    {
        Slot* oldHead = freeList_.load(std::memory_order_acquire);
        if (oldHead == nullptr)
            return nullptr; // 队列为空

        // 在访问 newHead 之前再次验证 oldHead 的有效性
        Slot* newHead = nullptr;
        try
        {
            // 读取下一个节点，准备将其提升为头节点
            newHead = oldHead->next.load(std::memory_order_relaxed);
        }
        catch(...)
        {
            // 极端防御：如果在读取过程中发生异常（如内存被意外回收），重试
            continue;
        }
        
        // 尝试更新头结点：将 freeList_ 指向 oldHead->next
        if (freeList_.compare_exchange_weak(oldHead, newHead,
         std::memory_order_acquire, std::memory_order_relaxed))
        {
            return oldHead; // 返回拿到的旧头节点供使用
        }
        // 失败：说明被其他线程抢先 pop 或 push 了，重试
    }
}

} // namespace Kama_memoryPool
```
##### 模块讲解
- **功能定位**：实现了内存分配的“快速通道”。大多数分配请求应通过 `popFreeList` 满足，只有在空闲链表耗尽时才去动用慢速的 `allocateNewBlock`（加锁操作）。
- **语法要点**：
  - **`compare_exchange_weak`**：CAS 的核心。相比 strong 版本，它允许在某些平台上“假失败”（Spurious Failure），但在循环中效率更高。参数：`(期望值, 新值, 成功序, 失败序)`。
  - **内存序（Memory Order）**：
    - `release` (在 push 时)：确保我把节点挂上去之前，我对节点内容的写入（如 `next` 指针）对其他线程可见。
    - `acquire` (在 pop 时)：确保我拿到节点后，能看到其他线程 release 之前对该节点的所有写入。
    - `relaxed`：无同步要求，仅保证原子性，用于失败重试或无需同步的数据读取。
- **设计思路**：
  - **无锁栈（Treiber Stack）**：采用头插法（LIFO）。因为最近释放的内存通常还在 CPU 缓存（L1/L2 Cache）中，立即复用它能大幅减少缓存未命中（Cache Miss），提升性能。
  - **乐观锁思想**：假设冲突不会发生，先尝试操作；如果发现冲突（CAS失败），则重试。这在高并发且锁竞争不极其激烈的场景下，比互斥锁（Mutex）快得多。
- **填空思考方向**：
  - **Loop-CAS 模式**：这是无锁编程的标准范式。`do { 准备新值 } while (!CAS(旧值, 新值));`。
  - **ABA 问题**：代码未引入版本号解决 ABA，在v1 版本中简化处理，聚焦于基础 CAS 逻辑。

### tests/目录
#### Step2_Test.cpp
创建一个新的测试文件，重点验证“分配-释放-再分配”的复用逻辑以及多线程下的安全性。

##### 完整测试代码
```cpp
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <set>
#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 1. 复用性测试：验证释放的内存是否真的回到了链表并被再次分配
void TestReuse()
{
    std::cout << "[Test] Memory Reuse Logic..." << std::endl;
    MemoryPool pool(4096);
    pool.init(8);

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();

    // 记录 p1 的地址
    size_t addr1 = reinterpret_cast<size_t>(p1);
    
    // 释放 p1，此时 p1 应该成为 freeList_ 的头节点
    pool.deallocate(p1);

    // 再次分配，根据 LIFO 原则，应该立刻拿到 p1
    void* p3 = pool.allocate();
    size_t addr3 = reinterpret_cast<size_t>(p3);

    assert(addr1 == addr3); // 核心校验：地址必须相同
    
    pool.deallocate(p2);
    pool.deallocate(p3);
    std::cout << "[Pass] Memory reused successfully (LIFO confirmed)." << std::endl;
}

// 2. 多线程并发测试：验证无锁操作是否有数据竞争导致崩溃或丢数据
void TestConcurrency()
{
    std::cout << "[Test] Multi-threaded Allocate/Deallocate..." << std::endl;
    MemoryPool pool(4096); // 较小的 Block 触发频繁竞争
    pool.init(8);

    const int THREAD_COUNT = 8;
    const int OPS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;

    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j)
            {
                void* p = pool.allocate();
                // 简单的负载模拟
                *reinterpret_cast<int*>(p) = j; 
                pool.deallocate(p);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }
    std::cout << "[Pass] Multi-threaded operations finished without crash." << std::endl;
}

int main()
{
    TestReuse();
    TestConcurrency();
    return 0;
}
```

### 步骤2测试指导
- **测试目标**：
  1. 验证 `deallocate` 后的内存能通过 `popFreeList` 被再次分配（复用性）。
  2. 验证多线程同时进行 `push` 和 `pop` 操作时，程序稳定不崩溃（线程安全）。
- **测试命令**：
  ```bash
  # 编译命令（必须加 -pthread）
  g++ -o step2_test src/MemoryPool.cpp tests/Step2_Test.cpp -I include/ -std=c++11 -pthread
  
  # 运行命令
  ./step2_test
  ```
- **通过标准**：
  - 输出 `[Pass] Memory reused successfully (LIFO confirmed).`：证明无锁链表逻辑生效，且遵循栈的后进先出原则。
  - 输出 `[Pass] Multi-threaded operations finished without crash.`：证明 CAS 逻辑正确处理了并发冲突。
- **常见错误**：
  1. **死循环（CAS 始终失败）**：如果在 `pushFreeList` 中忘记在循环内重新加载 `oldHead`（例如把 `oldHead` 定义在 while 外），一旦第一次 CAS 失败，`oldHead` 永远是旧值，导致死循环。
     - *解决方法*：确保 `oldHead` 的获取或更新逻辑在 `while` 循环内部，或者利用 `compare_exchange` 失败自动更新期望值的特性。
  2. **链表断裂**：在 `pop` 时，如果没有正确读取 `oldHead->next` 就进行了交换，或者多线程下 `oldHead` 被其他线程修改了但当前线程仍使用旧的 `next` 指针。
     - *解决方法*：严格遵循 `load` -> `CAS` 的顺序，并理解 `compare_exchange_weak` 的原子性保障了“我看到的 oldHead 和当前的 freeList_ 一致时才交换”。



------------------------------------------------
一、先把“无锁”俩字翻译成大白话
1. 无锁 ≠ 无同步  
   我们只是把“操作系统帮你睡觉的锁（mutex）”换成了“CPU 原语（CAS）”。  
   线程永远不会因为拿不到锁而挂起，它只会**忙等重试**，所以叫“lock-free”。

2. 为什么敢忙等？  
   CAS 是“一条指令完成 读-改-写”，硬件保证原子性。  
   只要冲突概率不高，重试几次就能成功，比切线程上下文快得多。

------------------------------------------------
二、内存序（memory_order）到底在同步什么？

1. memory_order_relaxed  
   只保证“原子性”，不保证顺序。  
   典型用法：计数器自增、循环里的重试读取。

2. memory_order_acquire  
   我拿到值以后，**能看到**别人 release 之前写进去的所有数据。  
   消费端（pop）用：  
   ```cpp
   Node* new_head = head.load(acquire);
   // 从此刻起，new_head->data 一定是写线程写完整的
   ```

3. memory_order_release  
   我把值写出去之前，**先 flush** 我前面所有写操作。  
   生产端（push）用：  
   ```cpp
   new_node->next = old_head;        // 1. 先写好next
   head.store(new_node, release);    // 2. 再“发布”出去
   ```
   别人拿到 new_node 时，1 一定完成。



下面把这段“无锁栈（Treiber Stack）”实现中提到的四个关键词——  
1. compare_exchange_weak 的语义与性能  
2. 内存序（memory_order）到底在同步什么  
3. 为什么 LIFO+缓存友好  
4. 乐观锁、Loop-CAS 范式
——全部拆成“能落地的白话”，看完就能自己写、自己调、自己排坑。  

------------------------------------------------  
1. compare_exchange_weak：允许“假失败”的 CAS  
------------------------------------------------  
函数签名（C++20 起）  
bool atomic<T>::compare_exchange_weak(  
    T& expected, T desired,  
    memory_order succ, memory_order fail) noexcept;  

逻辑伪码  
if (this->load() == expected) {  
    this->store(desired);          // 成功路径  
    return true;  
} else {  
    expected = this->load();       // 失败路径：把最新值写回 expected  
    return false;  
}  

关键区别 weak vs strong  
- weak：即使 *this == expected，也可能返回 false（“spurious failure”）。  
    Spurious failure 是为了性能而主动允许的抽象：
    硬件可以失败，库不必补偿，用户循环重试即可。
- strong：内部自动重试，直到“确实不相等”才返回 false。  
    代价是多一条循环指令，极端并发下能差 5~15%。  

------------------------------------------------  
2. 内存序：到底在拦哪条指令？  
------------------------------------------------  
硬件视角：内存序只干一件事——**阻止编译器和 CPU 把访存指令重排到“栅栏”另一侧**。  
C++ 把栅栏拆成 6 种，无锁栈里只出现 3 种：  

a) memory_order_release（push 成功时）  
   保证 **本次线程** 对 “要挂上去的节点” 的任何写（如 next 指针、数据payload）  
   都不能被重排到“CAS 成功”之后。  
   换句话说：别的线程一旦通过 acquire 看到我更新 head，就一定能看到我之前对节点的所有写。  

b) memory_order_acquire（pop 成功时）  
   与 release 配对。  
   保证 **本次线程** 在“读到新 head”之后的任何读，都不能被重排到“读 head”之前。  
   于是：  
   node = old_head;          // acquire 读  
   data = node->data;        // 这行不会被提前  

c) memory_order_relaxed  
   只保证“原子性”，不拦重排。  
   典型用法：  
   - 失败重试的循环计数器、debug 计数器；  
   - 只拿来读快照，不做同步。  

------------------------------------------------  
3. 为什么 Treiber 栈是 LIFO + 缓存友好  
------------------------------------------------  
- 最近释放的节点大概率还在当前 CPU 的 L1/L2 里，  
  立即重新 malloc / push 只会触发一次热缓存写，  
  相比 FIFO 队列把冷缓存行搬来搬去，miss 率更低。  

------------------------------------------------  
4. 乐观锁、Loop-CAS、ABA 问题  
------------------------------------------------  
4.1 乐观锁思想  
   先不加锁、直接改；冲突检测靠硬件 CAS；  
   冲突概率 < 10% 时，比 mutex 快一个量级；  

4.2 Loop-CAS 范式（背下来）  
   do {  
       old = atomic.load(mo_acquire);     // ① 快照  
       new = calculate(old);              // ② 构造新值  
   } while (!atomic.compare_exchange_weak(  
               old, new,                   // ③ 提交  
               mo_release, mo_relaxed));  

   三步缺一不可：  
   - 快照必须在循环内重取，否则无法感知别的线程已提交；  
   - calculate 不能修改共享状态（只能读 old）；  
   - CAS 失败自动把最新值写回 old，下一次循环继续。  

------------------------------------------------  
一句话总结  
------------------------------------------------  
compare_exchange_weak + acquire/release 是“硬件级乐观锁”的骨架；  
Treiber 栈借 LIFO 把最热的数据留在 CPU 缓存；  
记住 “快照-构造-CAS” 三步循环，就能写出第一版无锁结构，再逐步加版本号、Hazard Pointer 直到生产可用。