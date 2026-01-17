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

/*
析构 MemoryPool 时不需要释放 freeList_，这是因为 freeList_ 中的所有节点在物理上都属于 firstBlock_ 所指向的大块内存。
所有权关系：
firstBlock_ 链表管理的是所有向系统申请的 原始大块内存（Blocks）。
freeList_ 链表管理的是从这些大块内存中切分出来的 小槽位（Slots）。


需要转换成 void* 之后才调用 operator delete
A. 避免调用析构函数
如果你写 delete cur;（其中 cur 是 Slot*）：

编译器会认为你正在销毁一个 Slot 对象。
它会先调用 Slot 的析构函数 ~Slot()。

B. 匹配内存申请时的行为
在 allocateNewBlock 中，我们使用的是：

code
C++
operator new(BlockSize_) // 申请的是原始字节流
根据 C++ 标准，通过 operator new 申请的原始内存，应当通过 operator delete 配合 void* 来归还。这是一种对称的底层操作。

C. 防止销毁不完整的对象
在内存池的 Block 中，内存可能处于以下几种状态：

存储着 Slot 结构（在空闲链表中时）。
存储着用户对象 T（被分配出去后）。
仅仅是填充字节（Padding）。
当 MemoryPool 析构时，我们并不关心这一块块内存里现在装的是什么。通过 void* 释放，我们告诉编译器：“我只想要回这 BlockSize_ 个字节的内存，不需要你帮我做任何逻辑处理（比如调用某个对象的析构函数）。”
*/

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
    //BlockSize_ 已在构造函数中初始化
    SlotSize_ = std::max(size, sizeof(Slot)); // 确保每个槽位至少能放下 Slot 结构
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}


//返回 void* 指针，而不是返回Slot*, 因为 allocate 的接口对外是通用的，不能暴露内部实现细节 Slot 结构
void* MemoryPool::allocate()
{
    // TODO：【复用逻辑】优先尝试从无锁空闲链表中获取
    // 1. 调用 popFreeList() 获取可用的 Slot
    // 2. 如果获取成功（非空），直接返回该 Slot 指针
    // 3. 如果失败（链表为空），则进入下方的 Block 分配逻辑
    Slot * temp = popFreeList();
    if(temp!=nullptr) return reinterpret_cast<void*>(temp);

    // 为内存分配和游标移动加锁，防止多个线程同时进入此临界区！！
    {
        // 加锁保护 Block 的申请和游标移动
        std::lock_guard<std::mutex>lock(mutexForBlock_);
        if(curSlot_>=lastSlot_){
            // 当前内存块已无内存槽可用，开辟一块新的内存
            allocateNewBlock();
        }
        temp = curSlot_;
        // 移动游标，指向下一个可用位置
        // 注意：这里进行指针运算，原本采用SlotSize_ / sizeof(Slot) 
        // C++ 中指针加法是以 sizeof(T) 为单位的。
        // 如果 SlotSize_ 是 24 字节，sizeof(Slot) 是 8 字节，那么 +3 是对的。
        // 但如果 SlotSize_ 不是 sizeof(Slot) 的整数倍，这种写法会产生错误的偏移。转换为 char* 操作字节是最安全的。
        curSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<char*>(curSlot_) + SlotSize_);
    } 
    return reinterpret_cast<void*>(temp);
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;

    // 将用户指针强制转换为 Slot*，以便将其挂回链表
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    // 需要转换为 Slot* 后再入栈,因为 freeList_ 管理的就是 Slot* 链表
    pushFreeList(slot);
}

void MemoryPool::allocateNewBlock()
{   
    // 1. 申请原始大内存块
    void* newBlock = operator new(BlockSize_);
    
    // 2. 建立 Block 链表：
    // 每个 Block 的开头 8 字节（sizeof(Slot*)）用于存储指向前一个 Block 的指针，
    // 这样在销毁 MemoryPool 时可以顺着链表释放所有申请过的 Block。
    // 注意是store，不是直接赋值, 也不是load
    reinterpret_cast<Slot*>(newBlock)->next.store(firstBlock_, std::memory_order_relaxed);
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    // 3. 计算数据区的物理起点
    // 跳过开头的管理指针。注意：此时 dataAddr 的偏移量是 8。
    // static_cast（逻辑还原）：用于 C++ 标准定义过的合法类型路径。将 void* 转换为 char* 属于语义上的“类型还原”，编译器会进行路径检查，更安全、更规范。
    // reinterpret_cast（二进制重解释）：用于物理层面的硬转。它不关心逻辑，只是简单地将比特位强行解释为另一种类型（如整数转指针），通常用于处理完全无关的类型。
    char* dataAddr = static_cast<char*>(newBlock) + sizeof(Slot*);

    // 4. 【对齐策略思考记录】
    // 思考 A：为什么要对齐？
    //    operator new 保证 16 字节对齐，但跳过 8 字节指针后，dataAddr 变成了 8 字节对齐。
    //
    // 思考 B：按 SlotSize_ 对齐还是按固定值对齐？
    //    - 若按 SlotSize_ 对齐：绝对安全但浪费严重。例如 SlotSize_=512，dataAddr=8，
    //      padPointer 会跳过 504 字节凑齐 512，导致 Block 开头浪费 12% 的空间。
    //    - 若按 16 字节对齐（alignof(std::max_align_t)）：
    //      这是 64 位系统的标准对齐要求。即便 SlotSize_=512，也只需跳过 8 字节凑齐 16 字节对齐即可。
    //      这样可以最大化利用 Block 空间，且对于 99% 的 C++ 对象是安全的。
    //
    // 结论：采用 16 字节作为基础对齐步长。
    size_t alignment = 16; 
    size_t padSize = padPointer(dataAddr, alignment);
    
    // 设置当前可分配的第一个槽位
    curSlot_ = reinterpret_cast<Slot*>(dataAddr + padSize);

    // 5. 【边界计算与 512 字节规格的“宿命”】
    // 思考：4096 / 512 = 8，为什么总是只能分出 7 个？
    //    因为 BlockSize(4096) - 管理头(8字节) = 4088 字节。
    //    4088 / 512 = 7.98，在物理空间上绝对不足以放下第 8 个槽位。
    //    这个“浪费”是由于 BlockSize 设定的太死导致的，无法通过对齐算法找回。
    //    但对于其他规格（如 500 字节），按 16 字节对齐能比按 500 字节对齐多存出一个 Slot。
    
    // lastSlot_ 标记最后一个能完整容纳 SlotSize_ 的位置
    // lastSlot_ 的计算公式为 Block起始 + BlockSize - SlotSize + 1。
    // 这个 +1 的设计是为了配合 allocate() 中的 if (curSlot_ >= lastSlot_) 判断。
    // 它确保了只要 curSlot_ 还没到达 lastSlot_，当前位置就一定能切出一个完整的 SlotSize_ 给用户使用，不会发生越界写。
    lastSlot_ = reinterpret_cast<Slot*>(static_cast<char*>(newBlock) + BlockSize_ - SlotSize_ + 1);
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

/* 
 * 无锁链表风险对比：
 * 1. Push：操作的是【私有节点】，指针指向的内存由当前线程绝对控制，无并发销毁风险。
 * 2. Pop：操作的是【共享节点】，在读取 next 指针时，该节点可能已在其他线程中被弹出、
 *    甚至随所属大块内存一同被归还系统，存在典型的“Hazard Pointer”访问风险。
 */

// 实现无锁入队操作（头插法）
void MemoryPool::pushFreeList(Slot* slot)
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
        // 在 push 操作中，初始的 load 只是一个“猜测（Hint）”!!!
        // 真正的同步逻辑是由下方的 compare_exchange_weak 完成的。
        // 使用 relaxed 可以减少不必要的内存屏障开销。
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        // 将新节点的 next 指向当前头节点
        // 【安全性分析】
        // push 操作无需 try-catch：此时 slot 指针由当前线程独占（刚从用户处归还），
        // 在 CAS 成功发布到 freeList_ 前，其他线程无法感知该地址。
        // 因此，写入 slot->next 属于安全的私有操作，不存在并发销毁导致的非法访问。
        slot->next.store(oldHead,std::memory_order_relaxed);
        // 尝试将新节点设置为头节点
        // oldHead 是期望值，slot 是新值
        // 由FreeList_ 调用 compare_exchange_weak！！
        if(freeList_.compare_exchange_weak(oldHead,slot,std::memory_order_release,std::memory_order_relaxed)){
            return;
        };
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
        /* 
        * 【核心安全点：Pop vs Push】
        * 不同于 push 操作只修改当前线程拥有的私有节点，pop 操作需要解引用共享节点：oldHead.ptr->next。
        * 
        * 在通用的无锁栈中，这是一个极度危险的区域：若 oldHead 指向的内存被其他线程弹出并立即释放给系统，
        * 此处解引用会导致段错误（Segmentation Fault）。
        *
        * 【本实现安全性证明】
        * 在本 MemoryPool 设计中，Block 内存块仅在 MemoryPool 析构时释放。
        * 只要 MemoryPool 对象生命周期未结束，所有 Slot 的物理地址在运行时始终有效。
        * 即使 oldHead 被其他线程抢先弹出并重新分配（ABA），解引用操作依然是物理安全的。
        */

        // 读取下一个节点，准备将其提升为头节点
        Slot* newHead = oldHead->next.load(std::memory_order_relaxed);
        
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