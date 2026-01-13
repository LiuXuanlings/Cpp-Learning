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
    // TODO：【复用逻辑】优先尝试从无锁空闲链表中获取
    // 1. 调用 popFreeList() 获取可用的 Slot
    // 2. 如果获取成功（非空），直接返回该 Slot 指针
    // 3. 如果失败（链表为空），则进入下方的 Block 分配逻辑
    Slot * temp = popFreeList();
    if(temp!=nullptr) return reinterpret_cast<void*>(temp);

    {
        // 加锁保护 Block 的申请和游标移动
        std::lock_guard<std::mutex>lock(mutexForBlock_);
        if(curSlot_>=lastSlot_){
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

    // 将用户指针强制转换为 Slot*，以便将其挂回链表
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
    while(true){
        // 获取当前头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        // 将新节点的 next 指向当前头节点
        slot->next.store(oldHead,std::memory_order_relaxed);
        // 尝试将新节点设置为头节点
        // oldHead 是期望值，slot 是新值
        if(freeList_.compare_exchange_weak(oldHead,slot,std::memory_order_release,std::memory_order_relaxed)){
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
        // 在访问 newHead 之前再次验证 oldHead 的有效性
        if(oldHead == nullptr)
            return nullptr;

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
        if(freeList_.compare_exchange_weak(oldHead,newHead,std::memory_order_acquire,std::memory_order_relaxed)){
            return oldHead;
        }
        // 失败：说明被其他线程抢先 pop 或 push 了，重试
    }
}

void HashBucket::initMemoryPool()
{
    // TODO：【初始化逻辑】初始化 64 个不同规格的内存池
    // 1. 遍历 0 到 MEMORY_POOL_NUM-1
    // 2. 计算每个池子对应的 Slot 大小：(i + 1) * SLOT_BASE_SIZE
    //    即：idx 0 -> 8B, idx 1 -> 16B ... idx 63 -> 512B
    // 3. 调用 MemoryPool::init 完成每个实例的初始化
    for(int i = 0; i < MEMORY_POOL_NUM; ++i){
        getMemoryPool(i).init((i+1)*SLOT_BASE_SIZE);
    }
}   

MemoryPool& HashBucket::getMemoryPool(int index)
{
    // TODO：【静态单例数组】定义并返回静态内存池数组
    // 1. 定义 static MemoryPool 数组，大小为 MEMORY_POOL_NUM
    //    使用 static 局部变量实现“Meyers Singleton”，确保线程安全的延迟初始化
    // 2. 返回数组中指定 index 的引用
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}

} // namespace Kama_memoryPool