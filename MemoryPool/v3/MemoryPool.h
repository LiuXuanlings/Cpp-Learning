#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>  // for size_t, ptrdiff_t
#include <type_traits> // for std::true_type, std::false_type

template<typename T,size_t BlockSize = 4096>
class MemoryPool {
public:
    // 成员类型用于定义与类相关的类型，作用域限定在类内部
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;
    // Difference type,这是一个有符号整数类型，用于表示两个指针之间的差值
    typedef ptrdiff_t difference_type;
    
    // propagate_on_container_xxx 这些类型用于指示在容器操作（如复制、移动和交换）期间分配器的行为
    // 传播是指在容器操作期间，分配器是否应随容器一起复制、移动或交换
    
    /* 分配器传播规则：针对有状态内存池（每个MemoryPool管理独立内存块/空闲链表），
    核心目标是保证“谁分配的内存，谁释放”，避免资源泄漏、双重释放、非法访问 */

    // 容器拷贝赋值时：不传播分配器（false_type）
    /**
     * 【POCCA 设为 false_type 的核心要点：防止状态幻影】
     * 
     * 1. 状态性 (Statefulness):
     *    本 MemoryPool 存储了指向物理内存块的原始指针（currentBlock_ 等）。
     *    它是一个“有状态”的分配器，且没有内置引用计数。
     * 
     * 2. 浅拷贝风险 (Shallow Copy Risk):
     *    若设为 true_type，容器赋值（c1 = c2）会导致分配器内部的指针被原样复制。
     *    结果：两个独立的分配器将持有完全相同的 Block 链表地址。
     * 
     * 3. 崩溃触发 (Crash Trigger):
     *    - 系统级：当 c1 和 c2 析构时，它们会分别对相同的 currentBlock_ 
     *      执行 operator delete，触发“双重释放”崩溃。
     *    - 逻辑级：同一块 Slot 内存可能被两个分配器同时视为空闲并重复分配。
     * 
     * 4. 结论：
     *    虽然多个容器可以手动共享同一个 MemoryPool 实例（由开发者控制生命周期），
     *    但在容器赋值时，必须禁止分配器状态的自动传播，以强制实现内存资源的逻辑隔离。
     * 
     *  这里的“共享”是指**开发者有意识的、受控的实例共享**，而不是容器赋值时的自动传播。
     *  **开发者可以这样做：**
     *  创建一个 `MemoryPool<int> globalPool`，然后把这个**同一个实例**传给两个容器。
     *   ```cpp
     *  std::list<int, MemoryPool<int>> list1(globalPool);
     *  std::list<int, MemoryPool<int>> list2(globalPool);
     *  ```
     *  在这种情况下，`globalPool` 的生命周期独立于容器。只要容器在 `globalPool` 销毁前先销毁，就是安全的。
     */

     
    typedef std::false_type propagate_on_container_copy_assignment;

    // 容器移动赋值时：传播分配器（true_type）
    typedef std::true_type  propagate_on_container_move_assignment;

    // 容器交换时：传播分配器（true_type）
    // 原因：若不传播（false_type），仅交换数据指针但保留各自内存池 → 内存与管理者错位（如pool1分配的内存被pool2释放），导致非法访问/崩溃、内存泄漏；
    // 正确行为：同步交换数据和内存池，保证“pool1管理自己分配的内存，pool2管理自己分配的内存”，资源正确回收
    typedef std::true_type  propagate_on_container_swap;

    template<typename U>
    struct rebind {
        typedef MemoryPool<U, BlockSize> other;
    };

    MemoryPool() noexcept;
    MemoryPool(const MemoryPool& other) noexcept;
    MemoryPool(MemoryPool&& other) noexcept;
    // 模板拷贝构造函数，实现不同类型内存池间的转换
    template<typename U>
    MemoryPool(const MemoryPool<U, BlockSize>& other) noexcept;

    ~MemoryPool() noexcept;

    // 删除拷贝/移动赋值运算符，防止不安全的赋值操作
    MemoryPool& operator=(const MemoryPool& other) = delete;
    MemoryPool& operator=(MemoryPool&& other) noexcept;

    pointer address(reference x) const noexcept;
    const_pointer address(const_reference x) const noexcept;

    // Can only allocate one object at a time. n and hint are ignored
    //一次只能为一个目标分配空间 常量指针（const T*  -> 不可以修改该地址存放的数据）
    pointer allocate(size_type n = 1, const void* hint = 0);
    void deallocate(pointer p, size_type n = 1) noexcept;

    size_type max_size () const noexcept;//计算最大可使用的 Slot 槽

    // 仅构造对象（已分配内存），不申请/释放内存
    template<typename U ,typename... Args>
    void construct(U* p, Args&&... args);
    // 仅析构对象，不释放内存（内存可复用）
    template<typename U>
    void destroy(U* p) noexcept;

    // 分配内存 + 构造对象（一站式创建）
    template<class ... Args>//这里的class跟typename是等价的
    pointer newElement(Args&&... args);
    // 析构对象 + 释放内存（归还给内存池，一站式销毁）
    void deleteElement(pointer p) noexcept;
private:
    union Slot_ {
        value_type element;
        Slot_* next;
    };

    typedef char* data_pointer_;
    typedef Slot_ slot_type_;
    typedef Slot_* slot_pointer_;

    slot_pointer_ currentBlock_;   // 指向当前内存块的指针
    //后面allocate函数中为每一个元素分配空间的时候currentSlot指针是明确的只能往后走的，
    //也就是说这个 “可用” 不包括已经在空闲链表上的Slot（使用过但现在释放并还给内存池的槽）,
    //指向的是从没有被置放过数据的Slot槽。
    slot_pointer_ currentSlot_;    // 指向当前可用槽的指针
    slot_pointer_ lastSlot_;       // 指向当前内存块最后一个槽的
    slot_pointer_ freeSlots_;      // 空闲槽链表的头指针

    void allocateBlock();
    size_type padPointer(data_pointer_ p, size_type align) const noexcept;

    //static_assert用于在编译时检查条件是否为真，如果条件为假，则会触发编译错误
    static_assert(BlockSize >= 2 * sizeof(slot_type_), "BlockSize too small.");

};

// 引入模板实现(.tcc)：模板需声明实现同可见，.tcc是模板实现文件的通用约定后缀
#include "MemoryPool.tcc"

#endif //MEMORY_POOL_H