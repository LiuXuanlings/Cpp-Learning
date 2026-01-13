## 步骤1：内存池基础框架与块管理
### 步骤总览
本步骤是「Kama-memoryPool」的基石，重点构建 `MemoryPool` 类的核心骨架。核心目标是实现内存池的「大块内存申请」与「小对象切分」机制。通过本步骤，你将掌握如何向系统申请大块连续内存（Block），并通过指针运算与内存对齐算法，将其划分为可供分配的 Slot 游标。
此步骤完成后，内存池将具备基本的内存“进货”能力，为后续的 Slot 分配和无锁队列管理打下基础。

### include/目录
#### MemoryPool.h
##### 改造后代码
```cpp
#pragma once 

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace Kama_memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

/* 
 * 内存槽结构体
 * 注意：Slot的大小并不代表实际分配的内存大小。
 * 实际分配大小是 MemoryPool::SlotSize_ 决定的。
 * 这个结构体仅用于在空闲链表中存储下一个节点的指针。
 */
struct Slot 
{
    std::atomic<Slot*> next; // 原子指针，为后续无锁操作做准备。std::atomic<T> 与裸 T 完全同大小
};

class MemoryPool
{
public:
    // 构造函数：初始化块大小，默认为4KB
    MemoryPool(size_t BlockSize = 4096);
    // 析构函数：负责释放向系统申请的所有内存块
    ~MemoryPool();
    
    // 初始化函数：设置该内存池管理的槽大小（如8字节、16字节...）
    void init(size_t);

    // 对外分配接口
    void* allocate();
    // 对外释放接口
    void deallocate(void*);

private:
    // 核心内部接口：当当前内存块用尽时，申请新的大块内存
    void allocateNewBlock();
    // 辅助函数：计算指针对齐所需的填充字节数
    size_t padPointer(char* p, size_t align);

    // 无锁链表操作（将在步骤2重点实现，目前保留声明）
    bool pushFreeList(Slot* slot);
    Slot* popFreeList();

private:
    int                 BlockSize_;  // 向系统申请的单个大内存块的大小（如4096字节）
    int                 SlotSize_;   // 该池提供的每个小对象的实际大小
    
    Slot*               firstBlock_; // 管理所有向系统申请的大块内存（链表头）
    Slot*               curSlot_;    // 指向当前内存块中，下一个可分配的空闲位置
    Slot*               lastSlot_;   // 当前内存块的末尾边界
    
    std::atomic<Slot*>  freeList_;   // 归还回来的空闲对象链表（无锁栈，链表只是节点的连接方式；栈是节点的逻辑顺序）
    std::mutex          mutexForBlock_; // 互斥锁：仅用于 allocateNewBlock 这种低频的大块申请操作
};

} // namespace Kama_memoryPool
```
##### 模块讲解
- **功能定位**：定义了内存池的蓝图，包括核心数据成员（如 `curSlot_` 游标指针）和接口。`Slot` 结构体利用 `union` 思想（虽然这里显式定义为结构体），在对象空闲时存储 `next` 指针，复用内存空间。
- **语法要点**：
  - **`std::atomic<Slot*> next`**：虽然本步骤暂不涉及并发操作，但预定义原子类型是为了适配后续的 CAS 无锁编程，保证指针读写的原子性。
  - **`reinterpret_cast`**：在内存池中，我们需要频繁地将 `void*` 或 `char*` 转换为 `Slot*`，这属于低级指针操作，需确保转换后的地址合法且对齐。
- **设计思路**：
  - **单链表管理 Block**：`firstBlock_` 指向最新的大块内存，每个 Block 的头部存放指向上一个 Block 的指针。这样析构时可以遍历链表一次性释放所有 Block，防止内存泄漏。
  - **游标分配法**：`curSlot_` 和 `lastSlot_` 构成了最基本的线性分配（Bump Pointer Allocation），分配开销极低，只有在当前 Block 用完时才调用较重的 `allocateNewBlock`。

### src/目录
#### MemoryPool.cpp
##### 改造后代码
```cpp
#include "../include/MemoryPool.h"

namespace Kama_memoryPool 
{
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_ (BlockSize)
    , SlotSize_ (0)
    , firstBlock_ (nullptr)
    , curSlot_ (nullptr)
    , freeList_ (nullptr)
    , lastSlot_ (nullptr)
{}

MemoryPool::~MemoryPool()
{
    // TODO：【资源清理】遍历并释放所有向系统申请的 Block
    // 1. 从 firstBlock_ 开始遍历链表
    // 2. 在删除当前节点前，需先保存 next 指针防止断链
    // 3. 使用 operator delete 释放内存（需转换为 void*）
    Slot* cur = firstBlock_;
    while (cur)
    {
        //Slot* next = cur->next; // ❌ cur->next 是 std::atomic<Slot*>
        Slot* next = cur->next.load(std::memory_order_relaxed); 

        // 等同于 free(reinterpret_cast<void*>(firstBlock_));
        // 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t size)
{
    assert(size > 0);
    SlotSize_ = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate()
{
    // 优先使用空闲链表中的内存槽（步骤2重点）
    Slot* slot = popFreeList();
    if (slot != nullptr)
        return slot;

    Slot* temp;
    {   
        // 加锁保护 Block 的申请和游标移动
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_)
        {
            // 当前内存块已无内存槽可用，开辟一块新的内存
            allocateNewBlock();
        }
    
        temp = curSlot_;
        // 移动游标，指向下一个可用位置
        // 注意：这里进行指针运算，SlotSize_ / sizeof(Slot) 是为了适配 Slot* 类型的步长
        // 但更严谨的做法是转为 char* 运算。v1版本此处逻辑简化，假定对齐。
        curSlot_ += SlotSize_ / sizeof(Slot);
    }
    
    return temp; 
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    pushFreeList(slot);
}

void MemoryPool::allocateNewBlock()
{   
    // TODO：【核心分配逻辑】向系统申请新的大块内存并初始化
    // 1. 使用 operator new 申请 BlockSize_ 大小的内存
    // 2. 建立 Block 链表：将新块的 next 指向旧的 firstBlock_，更新 firstBlock_
    // 3. 计算数据区起始地址（跳过 Block 头部存放指针的空间）
    // 4. 调用 padPointer 计算对齐填充量
    // 5. 初始化 curSlot_（数据区起点+填充）和 lastSlot_（边界标记）
    
    // 申请内存
    void* newBlock = operator new(BlockSize_);
    
    // 链表头插法：新块 -> 旧块
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    // 计算数据体开始位置：块首地址 + 指针大小（用来存next）
    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    
    // 计算对齐
    size_t paddingSize = padPointer(body, SlotSize_); 
    
    // 设置游标
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // 设置边界：Block末尾 - Slot大小 + 1（确保最后一个位置能被用上）
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);
}

// 让指针对齐到槽大小的倍数位置
size_t MemoryPool::padPointer(char* p, size_t align)
{
    // TODO：【内存对齐算法】计算指针 p 距离下一个 align 倍数地址的偏移量
    // 1. 将指针 p 转为 size_t 获取数值
    // 2. 计算模运算结果 rem = addr % align
    // 3. 若 rem 为 0，则不需要填充；否则填充 align - rem
    size_t rem = (reinterpret_cast<size_t>(p) % align);
    return rem == 0 ? 0 : (align - rem);
}

bool MemoryPool::pushFreeList(Slot* slot)
{
    // 暂未实现，步骤2完善
    return false; 
}

Slot* MemoryPool::popFreeList()
{
    // 暂未实现，步骤2完善
    return nullptr; 
}

} // namespace Kama_memoryPool
```
##### 模块讲解
- **功能定位**：`MemoryPool.cpp` 实现了内存池的物理管理。重点在于如何将一块 raw memory（原始内存）转换成结构化的数据结构。
- **语法要点**：
  - **`operator new`**：不同于 `new` 表达式（分配+构造），`operator new` 仅负责分配原始内存（类似 malloc），这正是内存池底层需要的。
  - **指针算术**：代码中大量出现 `reinterpret_cast<char*>`。必须转为 `char*`（1字节步长）来进行精确的字节级偏移计算，直接对 `void*` 进行加减在标准C++中是非法的（虽然GCC支持）。
  - **`lock_guard`**：RAII 风格的锁管理，确保在 `allocateNewBlock` 抛出异常或函数返回时自动解锁，防止死锁。
- **设计思路**：
  - **Block 头部元数据**：每个申请的大块内存（Block）的前 `sizeof(Slot*)` 字节被挪用，存储指向上一个 Block 的指针。这是一种侵入式链表设计，避免了额外的容器开销。
  - **内存对齐**：CPU 访问未对齐的内存（如在奇数地址读取 int）会导致性能大幅下降甚至崩溃（某些架构）。`padPointer` 确保每个分配出去的 Slot 起始地址都是 `SlotSize_`（通常是8的倍数）的整数倍。
- **填空思考方向**：
  - **allocateNewBlock**：思考如何把几个孤立的变量（新申请的内存、旧的链表头、游标指针）串联起来。关键是理解内存布局：`[NextBlockPtr | Padding | Slot 1 | Slot 2 | ... | End]`。
  - **padPointer**：经典的对齐算法。如果当前地址是 13，对齐要求是 8，模8余5，说明多出了5字节，需要补 `8-5=3` 字节，使地址变为 16。

### tests/目录
#### Step1_Test.cpp
由于 v1 原有的 `UnitTest.cpp` 依赖 `HashBucket`（尚未实现），我们需要创建一个步骤1专属的测试文件来验证基础的 Block 分配功能。

##### 完整测试代码
```cpp
#include <iostream>
#include <cassert>
#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 步骤1专属测试：验证 Block 分配与游标移动
void TestBlockAllocation()
{
    std::cout << "[Test] Block Allocation & Alignment..." << std::endl;
    
    // 1. 创建一个内存池，Block大小设为 1024 字节
    MemoryPool pool(1024);
    // 2. 初始化槽大小为 8 字节
    pool.init(8);

    // 3. 连续分配 10 个对象
    // 由于此时 FreeList 为空，应触发 allocateNewBlock
    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    
    assert(p1 != nullptr);
    assert(p2 != nullptr);

    // 4. 验证地址连续性（因为是顺序从 Block 切分的）
    // p2 应该在 p1 之后，且相差 8 字节（未考虑复杂的 Padding 情况下，理想状态）
    size_t addr1 = reinterpret_cast<size_t>(p1);
    size_t addr2 = reinterpret_cast<size_t>(p2);
    
    std::cout << "Addr1: " << addr1 << ", Addr2: " << addr2 << std::endl;
    assert(addr2 - addr1 == 8);

    // 5. 验证对齐
    assert(addr1 % 8 == 0);
    assert(addr2 % 8 == 0);

    std::cout << "[Pass] Block Allocation basic logic works." << std::endl;
}

int main()
{
    TestBlockAllocation();
    return 0;
}
```

### 步骤1测试指导
- **测试目标**：验证 `allocateNewBlock` 能正确向系统申请内存，且 `curSlot_` 游标能按 `SlotSize` 步长正确移动，产生的地址符合对齐要求。
- **测试命令**：
  ```bash
  # 编译命令
  g++ -o step1_test src/MemoryPool.cpp tests/Step1_Test.cpp -I include/ -std=c++11 -pthread
  
  # 运行命令
  ./step1_test
  ```
- **通过标准**：
  - 编译无报错。
  - 运行输出 `[Pass] Block Allocation basic logic works.`。
  - 且 `Addr2` 与 `Addr1` 的差值严格等于 8（因为槽大小设为8）。
- **常见错误**：
  1. **Segmentation Fault**：在 `allocateNewBlock` 中，若 `reinterpret_cast` 转换后的地址计算错误（例如忘记转为 `char*` 就直接加减），会导致越界访问。
     - *解决方法*：检查指针运算前是否已强制转换为 `char*` 或 `size_t`。
  2. **地址未对齐**：`padPointer` 实现错误（如直接返回 `align - rem` 而忽略 `rem == 0` 的情况），导致内存浪费或不对齐。
     - *解决方法*：确保当 `rem == 0` 时返回 0，不要进行减法运算。ß

    

**`+1` 是为了确保最后一个有效槽位能被用上。**

---

### **边界检查机制**
```cpp
if(curSlot_ >= lastSlot_) {  // 当前游标 >= 上界 → 无空间
    allocateNewBlock();
}
temp = curSlot_;
curSlot_ += SlotSize_ / sizeof(Slot);  // 移动游标
```

`lastSlot_` 是**警戒线**：当 `curSlot_` 到达或越过它时，表示当前 Block 已耗尽。

---

### **为什么必须 `+1`？**

假设 Block 从地址 **1000** 开始，`BlockSize_ = 100`，`SlotSize_ = 10`：

| 地址 | 1000 | 1010 | 1020 | ... | 1090 | **1100** |
|------|------|------|------|-----|------|----------|
| 槽位 | 槽0  | 槽1  | 槽2  | ... | **槽9（最后一个）** | Block 结束 |

- **最后一个有效槽**的起始地址是 `1090 = 1000 + 100 - 10`
- **警戒线**必须设在 `1091`，才能让 `curSlot_ = 1090` 时通过检查（`1090 >= 1091` 为 **false**，表示还有空间）

如果 `lastSlot_ = 1000 + 100 - 10 = 1090`（**不加 1**）：
- 当 `curSlot_ = 1090` 时，检查 `1090 >= 1090` 为 **true**
- 会**误判**为无空间，提前分配新 Block，**浪费**最后一个槽位

---

### **一句话总结**
`+1` 把 `lastSlot_` 从“最后一个槽的地址”微调为“最后一个槽的地址**+1**”，让边界检查从**闭区间**变为**开区间**，从而正确包含最后一个可用槽。