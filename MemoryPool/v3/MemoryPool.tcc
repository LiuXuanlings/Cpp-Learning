#ifndef MEMORY_POOL_TCC
#define MEMORY_POOL_TCC

#include "MemoryPool.h"

template<typename T,size_t BlockSize>
//属于类的 “内部类型”，所以必须用 类名::类型名 的方式指明。
//在模板里访问 “依赖于模板参数的类内部类型” 时，必须用 typename 告诉编译器：MemoryPool<T, BlockSize>::size_type 是一个类型，而不是类的静态成员变量 / 函数
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::padPointer(data_pointer_ p, size_type align)
const noexcept //const和noexcept修饰符顺序无所谓
{
    uintptr_t result = reinterpret_cast<uintptr_t>(p);
    return (align - (result % align)) % align;
}

template<typename T,size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool() noexcept
    : currentBlock_(nullptr),
      currentSlot_(nullptr),
      lastSlot_(nullptr),
      freeSlots_(nullptr)
{
}

template<typename T,size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool& other) 
noexcept :
    MemoryPool() //委托构造函数，调用默认构造函数初始化成员变量
{
    // 空实现，保持独立内存池
}

template<typename T,size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(MemoryPool&& other) noexcept
    : currentBlock_(other.currentBlock_),
      currentSlot_(other.currentSlot_),
      lastSlot_(other.lastSlot_),
      freeSlots_(other.freeSlots_)
{
    // 将 other 的指针成员置空，防止其析构时释放内存
    other.currentBlock_ = nullptr;
    other.currentSlot_ = nullptr;
    other.lastSlot_ = nullptr;  
    other.freeSlots_ = nullptr;
}       

template <typename T, size_t BlockSize>
template<class U>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool<U, BlockSize>& memoryPool)
noexcept :
MemoryPool()
{}

template<typename T,size_t BlockSize>
MemoryPool<T, BlockSize>&
MemoryPool<T, BlockSize>::operator=(MemoryPool&& other) noexcept
{
    if (this != &other) {
        // 释放当前内存池的资源
        this->~MemoryPool();

        // 移动其他内存池的资源
        currentBlock_ = other.currentBlock_;
        currentSlot_ = other.currentSlot_;
        lastSlot_ = other.lastSlot_;
        freeSlots_ = other.freeSlots_;

        // 将 other 的指针成员置空，防止其析构时释放内存
        other.currentBlock_ = nullptr;
        other.currentSlot_ = nullptr;
        other.lastSlot_ = nullptr;  
        other.freeSlots_ = nullptr;
    }
    return *this;
}

template<typename T,size_t BlockSize>
MemoryPool<T, BlockSize>::~MemoryPool() noexcept
{
    slot_pointer_ curr = currentBlock_;
    while (curr != nullptr) {
        slot_pointer_ prev = curr->next;
        ::operator delete(reinterpret_cast<void *>(curr));//释放空间 转为 void* 不需要调用析构函数
        curr = prev;
    }
}

template<typename T,size_t BlockSize>
void MemoryPool<T, BlockSize>::allocateBlock()
{
    // 分配一块新的内存块
    data_pointer_ newBlock = reinterpret_cast<data_pointer_>
    (::operator new(BlockSize));

    // 将新内存块链接到内存块链表的前端
    reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);

    // 计算第一个可用槽的位置，考虑对齐
    data_pointer_ body = newBlock + sizeof(slot_pointer_);
    size_type padding = padPointer(body, alignof(slot_type_));
    currentSlot_ = reinterpret_cast<slot_pointer_>(body + padding);

    // 计算最后一个槽的位置
    /**
     * 【哨兵触发与惰性初始化权衡 (Trade-off: Sentinel Triggering vs. Lazy Initialization)】
     * 
     * 1. 哨兵冲突问题 (The Sentinel Problem):
     *    在初始状态下，currentSlot_ 与 lastSlot_ 均被硬编码初始化为 nullptr (数值 0)。
     *    若逻辑判定条件误设为 `currentSlot_ > lastSlot_`，在首次调用 allocate() 时，
     *    判定结果为 `0 > 0 = false`。这将导致程序跳过 allocateBlock() 的初始化流程，
     *    直接对空指针进行解引用 (Null Pointer Dereference)，从而触发段错误 (Segmentation Fault)。
     *
     * 2. 备选方案及其权衡 (Comparative Analysis of Alternatives):
     * 
     *    方案 A：显式条件分支 (Explicit Branch Checking)
     *    逻辑：if (currentSlot_ == nullptr || currentSlot_ >= lastSlot_)
     *    评估：虽然逻辑直观，但由于 allocate() 是高频热点函数 (Hot Path)，每笔分配都会增加一个
     *         额外的分支判断。在现代 CPU 流水线中，这可能增加分支预测失败的概率，降低指令吞吐量。
     *
     *    方案 B：偏移量哨兵策略 (Arithmetic Sentinel Offset) - 本实现采用
     *    逻辑：计算 lastSlot_ 时引入 +1 偏移量，并配合紧约束判断 `>=`。
     *    评估：这是时空开销最优解。
     *
     * 3. 偏移量 +1 与逻辑算术证明 (Formal Logic Proof of Offset +1):
     * 
     *    - 初始化阶段 (Bootstrapping): 
     *      由于初始化时 currentSlot_ 和 lastSlot_ 均为 0，`0 >= 0` 判定为真，
     *      从而无缝触发首次惰性初始化。
     * 
     *    - 边界压榨 (Boundary Maximization):
     *      若不执行 +1，lastSlot_ 将指向 Block 内最后一个槽位的起始地址。
     *      由于采用 `>=` 判断，当指针移动到该地址时会立即触发换块，导致每个 Block 物理上的
     *      最后一个槽位永远无法被利用。
     * 
     *    - 水位线陷阱 (Watermark Trap):
     *      引入 +1 后，lastSlot_ 被推移至【最后一个合法起点】与【第一个非法起点 (越界)】之间的
     *      非对齐区域。由于指针自增步长 (sizeof(slot_type_)) 严格大于 1 字节：
     *      a) 当指针指向最后一个合法槽位时，判定 `currentSlot_ < lastSlot_` (通过)；
     *      b) 当指针自增越过 Block 边界时，判定 `currentSlot_ >= lastSlot_` (截获)。
     *
     * 结论：通过 "+1 偏移" 与 ">= 算子" 的巧妙组合，本实现以零运行时开销 (Zero-overhead) 
     * 解决了初始化判别问题，并实现了 Block 空间利用率的理论最大化。
     */

    // 对应的关键代码行：
    // lastSlot_ = reinterpret_cast<slot_pointer_>(blockEnd - sizeof(slot_type_) + 1);

    // ...

    // if (currentSlot_ >= lastSlot_) {
    //     allocateBlock();
    // }
    data_pointer_ blockEnd = newBlock + BlockSize;
    lastSlot_ = reinterpret_cast<slot_pointer_>(
        blockEnd - sizeof(slot_type_) + 1
    );
}


template<typename T,size_t BlockSize>
typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::allocate(size_type n, const void* hint)
{
    if (freeSlots_ != nullptr) {
        // 从空闲槽链表中分配槽
        slot_pointer_ result = freeSlots_;
        freeSlots_ = freeSlots_->next;
        return reinterpret_cast<pointer>(&result->element);
    } else {
        if (currentSlot_ >= lastSlot_) {//惰性初始化：在第一次 allocate 时发现 nullptr >= nullptr，从而触发第一次 operator new。
            // 当前内存块已满，分配新内存块
            allocateBlock();
        }
        // 从当前内存块分配槽
        slot_pointer_ result = currentSlot_;
        currentSlot_++;
        return reinterpret_cast<pointer>(&result->element);
    }
}

template<typename T,size_t BlockSize>
void MemoryPool<T, BlockSize>::deallocate(pointer p, size_type n) noexcept
{
    if (p != nullptr) {
        // 将槽回收到空闲槽链表中
        slot_pointer_ slot = reinterpret_cast<slot_pointer_>(p);
        slot->next = freeSlots_;
        freeSlots_ = slot;
    }
}

//计算最大可使用的 Slot 槽
template<typename T,size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::max_size () const noexcept
{
    /*
    地址空间总量：是系统能寻址的总字节数（32 位 = 4GB，64 位 = 16EB）；
    size_type 最大值：是 size_t 类型能表示的最大数值，刚好等于「地址空间总字节数 - 1」（因为从 0 开始计数）
    */
    size_type maxBlocks = (static_cast<size_type>(-1) / BlockSize);
    return maxBlocks * (BlockSize - sizeof(slot_pointer_)) / sizeof(slot_type_);
}

template<typename T, size_t BlockSize>
template<typename U, typename... Args>
void MemoryPool<T, BlockSize>::construct(U* p, Args&&... args)
{
    new(p) U(std::forward<Args>(args)...);
}

template<typename T, size_t BlockSize>
template<typename U>
void MemoryPool<T, BlockSize>::destroy(U* p) noexcept
{
    p->~U();
}

template<typename T, size_t BlockSize>
template<typename... Args>
typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::newElement(Args&&... args)
{
    pointer p = allocate();
    construct<value_type>(p, std::forward<Args>(args)...);
    return p;
}

template<typename T, size_t BlockSize>
void MemoryPool<T, BlockSize>::deleteElement(pointer p) noexcept
{
    if(p != nullptr){
        destroy<value_type>(p);
        deallocate(p);
    }
}


#endif //MEMORY_POOL_TCC