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
    std::atomic<Slot*> next; // 原子指针，为后续无锁操作做准备
};

class MemoryPool
{
public:
    // 构造函数：初始化块大小，默认为4KB，这里可修改
    MemoryPool(size_t BlockSize = 4096);
    // 析构函数：负责释放向系统申请的所有内存块
    ~MemoryPool();
    
    // 初始化函数：设置该内存池管理的槽大小（如8字节、16字节...），此外还将其他成员设置为nullptr
    void init(size_t);

    // 对外分配接口
    void* allocate();
    // 对外释放接口，无需提供释放的大小，因为同一个内存池分配和释放的都是SlotSize_大小
    void deallocate(void*);

private:
    // 核心内部接口：当当前内存块用尽时，申请新的大块内存，不同返回申请的块指针
    void allocateNewBlock();
    // 辅助函数：计算指针对齐所需的填充字节数，C/C++ 保证 char* 的步长是 1 字节，指针算术单位就是字节。
    size_t padPointer(char* p, size_t align);

    // 无锁链表操作（将在步骤2重点实现，目前保留声明）
    /*
        参数 Slot* → 普通指针，表示“一块待回收的内存块”，生命周期已结束，没人再并发的碰它。
        成员 std::atomic<Slot*> freeList_ → 共享栈顶，可能被多线程并发读写，所以必须原子。
    */
    void pushFreeList(Slot* slot);
    Slot* popFreeList();

private:
    size_t                 BlockSize_;  // 向系统申请的单个大内存块的大小（如4096字节）
    size_t                 SlotSize_;   // 该池提供的每个小对象的实际大小

    Slot*               firstBlock_; // 管理所有向系统申请的大块内存（链表头）
    Slot*               curSlot_;    // 指向当前内存块中，下一个可分配的空闲位置
    Slot*               lastSlot_;   // 当前内存块的末尾边界
    
    std::atomic<Slot*>  freeList_;   // 归还回来的空闲对象链表（无锁栈）
    std::mutex          mutexForBlock_; // 互斥锁：仅用于 allocateNewBlock 这种低频的大块申请操作
};

class HashBucket
{
public:
    // 初始化所有规格的内存池，HashBucket 本身没有任何成员变量，全静态接口消除实例化开销
    static void initMemoryPool();
    // 获取指定索引的内存池实例
    static MemoryPool& getMemoryPool(int index);

    // 核心路由函数：根据大小分配内存
    static void* useMemory(size_t size)
    {
        if (size <= 0)
            return nullptr;
        
        // 超过最大规格（512字节）的对象，内存池不接管，直接走系统申请，注意operator前缀
        if (size > MAX_SLOT_SIZE) 
            return operator new(size);

        // TODO：【映射算法】计算 size 对应的内存池索引
        // 1. 目标：将大小映射到 0 ~ (MEMORY_POOL_NUM-1) 的范围内
        // 2. 规则：按 SLOT_BASE_SIZE (8字节) 对齐
        //    例如：1-8字节 -> index 0; 9-16字节 -> index 1
        // 3. 算法公式：((size + 7) / 8) - 1
        //    (size + 7) / 8 实现了向上取整（Ceil）的效果
        return getMemoryPool(((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE) - 1).allocate();
    }

    // 核心路由函数：根据大小释放内存，与内存池的deallocate不同，需提供size
    static void freeMemory(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        if (size > MAX_SLOT_SIZE)
        {
            operator delete(ptr);
            return;
        }

        // 同样的映射算法找到对应的池进行回收，注意利用内存池的接口去进行操作而非使用内存池的内部结构
        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    // 模板封装：分配内存并构造对象
    template<typename T, typename... Args> 
    friend T* newElement(Args&&... args);
    
    // 模板封装：析构对象并释放内存
    template<typename T>
    friend void deleteElement(T* p);
};

// TODO：【模板封装】实现类似 new T(args...) 的功能
// 1. 计算 T 类型的大小 sizeof(T)
// 2. 调用 HashBucket::useMemory 申请原始内存
// 3. 使用 Placement New 在申请的内存上构造对象
// 4. 利用 std::forward 实现参数的完美转发
template<typename T, typename... Args>
T* newElement(Args&&... args)
{
    T* p = nullptr;
    // 申请内存
    if((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        // 在地址 p 上构造对象 T，传入参数 args
        new (p) T(std::forward<Args>(args)...);

    return p;
}

// TODO：【模板封装】实现类似 delete p 的功能
// 1. 检查指针有效性
// 2. 显式调用对象的析构函数 ~T() (注意：这只清理资源，不释放内存)
// 3. 调用 HashBucket::freeMemory 归还内存块
template<typename T>
void deleteElement(T* p)
{
    if (p)
    {
        // 显式调用析构函数
        p->~T();
        // 内存回收
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

} // namespace Kama_memoryPool