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
    
    std::atomic<Slot*>  freeList_;   // 归还回来的空闲对象链表（无锁栈）
    std::mutex          mutexForBlock_; // 互斥锁：仅用于 allocateNewBlock 这种低频的大块申请操作
};

} // namespace Kama_memoryPool