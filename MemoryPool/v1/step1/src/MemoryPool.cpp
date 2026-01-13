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
    while(cur!=nullptr){
        //Slot* next = cur->next; // ❌ cur->next 是 std::atomic<Slot*>
        Slot* next = cur->next.load(std::memory_order_relaxed); 

        // 等同于 free(reinterpret_cast<void*>(firstBlock_));
        // 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
        operator delete(reinterpret_cast<void*> (cur));
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
    Slot * Block = reinterpret_cast<Slot*> (operator new (BlockSize_));
    
    // 链表头插法：新块 -> 旧块
    Block->next = firstBlock_;
    firstBlock_ = Block;
    // 计算数据体开始位置：块首地址 + 指针大小（用来存next）
    char* dataAddr = reinterpret_cast<char*>(Block) + sizeof(Slot*);

    // 计算对齐
    size_t padSize = padPointer(dataAddr,SlotSize_);
    
    // 设置游标
    curSlot_ = reinterpret_cast<Slot*> (dataAddr + padSize);

    // 设置边界：Block末尾 - Slot大小 + 1（确保最后一个位置能被用上
）
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(Block) + BlockSize_ -SlotSize_+1);

}

// 让指针对齐到槽大小的倍数位置
size_t MemoryPool::padPointer(char* p, size_t align)
{
    // TODO：【内存对齐算法】计算指针 p 距离下一个 align 倍数地址的偏移量
    // 1. 将指针 p 转为 size_t 获取数值
    // 2. 计算模运算结果 rem = addr % align
    // 3. 若 rem 为 0，则不需要填充；否则填充 align - rem
    size_t rem = (reinterpret_cast<size_t>(p) % align);
    return (rem==0 ? 0:align - rem);
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