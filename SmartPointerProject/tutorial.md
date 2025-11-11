### **C++ 智能指针：从原理到实现的完整教程**

本教程将带领大家彻底征服 C++ 中最重要也最强大的特性之一——智能指针。我们将不仅仅停留在“如何使用”的层面，更要深入其内部，亲手构建一套属于我们自己的智能指针库。

**学习路径：**
1.  **理论篇**：理解智能指针为何存在，以及其背后的核心设计哲学 RAII。
2.  **实践篇**：我们将得到一份待完成的智能指针库“蓝图”，并亲手实现 `MyUniquePtr`, `MySharedPtr`, `MyWeakPtr` 的所有功能。
3.  **指南篇**：学习在实际开发中如何规避常见陷阱，掌握最佳实践。
4.  **工程篇**：搭建一个专业的测试环境来验证我们的代码。
5.  **附录**：提供完整的参考实现，供大家对照学习。

---

### **Part 1：理论篇 - 告别手动内存管理**

在 C++ 中，使用 `new` 和 `delete` 手动管理内存，常常会陷入以下四个“陷阱”：

| 裸指针问题 | 描述 |
| :--- | :--- |
| **内存泄漏** | 动态分配内存后，忘记或因异常跳过 `delete`，导致内存无法被系统回收。 |
| **悬空指针** | 内存被 `delete` 后，指针未置空，后续使用导致未定义行为。 |
| **重复释放** | 对同一块内存 `delete` 多次，直接导致程序崩溃。 |
| **异常安全问题** | 在 `new` 和 `delete` 之间发生异常，导致 `delete` 无法被执行。 |

为了从根本上解决这些问题，C++ 社区引入了 **RAII (Resource Acquisition Is Initialization)** 的设计哲学，它的核心思想是：

> 利用 C++ 中对象生命周期结束时析构函数必然会被调用的特性，来保证资源的自动释放。

智能指针就是 RAII 思想的最佳实践：它是一个**栈上**的对象，其内部包装了一个**堆上**的裸指针。当智能指针对象离开作用域时，它的析构函数会自动被调用，并在析构函数中安全地 `delete` 其管理的内存。

---

### **Part 2：实践篇 - 你的任务：构建智能指针**

现在，是时候开始你们的实践任务了。下面是我们将要实现的智能指针库的完整 API 框架。

#### **2.1 项目设置**

首先，创建如下的项目目录结构：

```
SmartPointerProject/
├── include/
│   └── my_smart_ptr.hpp    // 智能指针的蓝图
└── src/
    └── main.cpp            // 测试代码
```

#### **2.2 智能指针蓝图 (`my_smart_ptr.hpp`)**

请将以下代码框架复制到 `include/my_smart_ptr.hpp` 文件中。这份蓝图定义了我们需要实现的所有 API，但函数体是空的，等待你们去填充。

```cpp
// include/my_smart_ptr.hpp
#ifndef MY_SMART_PTR_HPP
#define MY_SMART_PTR_HPP

#include <utility> // for std::move

namespace my_std {

// 前向声明
template<typename T> class MySharedPtr;
template<typename T> class MyWeakPtr;

// ====================================================================================
//                                  MyUniquePtr
// API 功能描述：一个轻量级的、独占所有权的智能指针。
// ====================================================================================
template <typename T>
class MyUniquePtr {
public:
    // API 1.1: 构造函数
    // 功能：创建一个 MyUniquePtr 实例，接管传入的裸指针 `ptr` 的所有权。
    //       如果 `ptr` 为空 (默认值)，则创建一个空的 MyUniquePtr。
    //       必须是 `explicit`，以防止危险的隐式转换。
    explicit MyUniquePtr(T* ptr = nullptr) { /* ... */ }

    // API 1.2: 析构函数
    // 功能：当 MyUniquePtr 对象本身被销毁时，必须释放其所拥有的资源，
    //       即 `delete` 内部的裸指针 `_ptr` (如果它不为空)。
    ~MyUniquePtr() { /* ... */ }

    // API 1.3: 禁用拷贝
    // 功能：完全禁止 MyUniquePtr 的拷贝行为，以保证所有权的唯一性。
    MyUniquePtr(const MyUniquePtr& other) = delete;
    MyUniquePtr& operator=(const MyUniquePtr& other) = delete;

    // API 1.4: 移动构造与赋值
    // 功能：允许所有权的转移。从源 `other` "窃取"其裸指针，
    //       并且必须将源 `other` 的内部指针置为 `nullptr`。
    MyUniquePtr(MyUniquePtr&& other) noexcept { /* ... */ }
    MyUniquePtr& operator=(MyUniquePtr&& other) noexcept { /* ... */ return *this; }

    // API 1.5: 指针操作符
    // operator*: 返回对所管理对象的引用。
    // operator->: 返回所管理对象的裸指针，以便访问其成员。
    T& operator*() const { /* ... */ }
    T* operator->() const { /* ... */ }

    // API 1.6: 管理接口
    // get():       返回内部存储的裸指针，但不放弃所有权。
    // release():   放弃所有权，返回内部的裸指针，并将内部指针置为 `nullptr`。
    // reset(ptr):  释放当前拥有的指针，并接管新的指针 `ptr`。
    // operator bool: 检查当前是否拥有一个非空的指针。
    T* get() const { return nullptr; }
    T* release() { return nullptr; }
    void reset(T* ptr = nullptr) { /* ... */ }
    explicit operator bool() const { return false; }

private:
    T* _ptr;
};

// ====================================================================================
//                                  ControlBlock & Co.
// API 功能描述：`MySharedPtr` 和 `MyWeakPtr` 的核心，实现共享所有权和弱引用观察。
// ====================================================================================
// API 2.1: 控制块
// 功能：一个独立于被管理对象的结构体，用于存储两种引用计数。
// strong_count: 记录有多少个 MySharedPtr 实例正共享所有权。对象的生命周期由它决定。
// weak_count:   记录有多少个 MyWeakPtr 实例正观察该对象。控制块的生命周期由它和 strong_count 共同决定。
struct ControlBlock {
    int strong_count;
    int weak_count;
};

template <typename T>
class MySharedPtr {
    friend class MyWeakPtr<T>;
public:
    // API 2.2: 构造函数 (从裸指针)
    // 功能：创建一个 MySharedPtr，为传入的 `ptr` 创建一个新的控制块，并将 strong_count 初始化为 1。
    explicit MySharedPtr(T* ptr = nullptr) { /* ... */ }

    // API 2.3: 析构函数
    // 功能：减少强引用计数。如果计数降为 0，则释放资源。
    ~MySharedPtr() { /* ... */ }

    // API 2.4: 拷贝构造与赋值
    // 功能：使新的 MySharedPtr 与 `other` 共享同一个对象和控制块，并使 strong_count 加 1。
    MySharedPtr(const MySharedPtr& other) { /* ... */ }
    MySharedPtr& operator=(const MySharedPtr& other) { /* ... */ return *this; }
    
    // API 2.5: 移动构造与赋值
    // 功能：从 `other` 窃取指针和控制块，不改变引用计数，并将 `other` 置空。
    MySharedPtr(MySharedPtr&& other) noexcept { /* ... */ }
    MySharedPtr& operator=(MySharedPtr&& other) noexcept { /* ... */ return *this; }
    
    // API 2.6: 构造函数 (从 MyWeakPtr)
    // 功能：这是 `lock()` 的核心。检查 `weak` 指向的控制块中 strong_count 是否大于 0。
    //       如果是，则构造一个有效的 MySharedPtr，并使 strong_count 加 1。
    //       如果否，则构造一个空的 MySharedPtr。
    MySharedPtr(const MyWeakPtr<T>& weak) { /* ... */ }

    // API 2.7: 常用接口
    // use_count(): 返回当前的 strong_count。
    // get(), operator*, operator->, operator bool: 与 MyUniquePtr 功能相同。
    int use_count() const { return 0; }
    T* get() const { return nullptr; }
    /* ... 其他指针操作 ... */

private:
    // API 2.8: release 辅助函数
    // 功能：封装引用计数递减和资源释放的核心逻辑。
    void release() { /* ... */ }

    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

template <typename T>
class MyWeakPtr {
    friend class MySharedPtr<T>;
public:
    // API 3.1: 构造函数
    // 默认构造: 创建一个空的 MyWeakPtr。
    // 从 MySharedPtr 构造: 创建一个指向 `shared` 所管理对象的观察者，并使 weak_count 加 1。
    MyWeakPtr() { /* ... */ }
    MyWeakPtr(const MySharedPtr<T>& shared) { /* ... */ }

    // API 3.2: 析构函数
    // 功能：减少弱引用计数。如果需要，释放控制块。
    ~MyWeakPtr() { /* ... */ }

    // API 3.3: 核心功能
    // expired(): 检查被观察的对象是否已被销毁 (即 strong_count 是否为 0)。
    // lock():    尝试获取一个指向被观察对象的 MySharedPtr。如果对象存活，返回一个有效的 MySharedPtr；
    //            如果对象已被销毁，返回一个空的 MySharedPtr。
    // use_count(): 返回当前的 strong_count。
    bool expired() const { return true; }
    MySharedPtr<T> lock() const { return MySharedPtr<T>(nullptr); }
    int use_count() const { return 0; }

private:
    // API 3.4: release 辅助函数
    // 功能：封装弱引用计数递减和控制块释放的逻辑。
    void release() { /* ... */ }

    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

} // namespace my_std
#endif // MY_SMART_PTR_HPP
```

---

### **Part 3：指南篇 - 实现思路与最佳实践**


### **Part 3.1：指南篇 - 引导实现：填补框架的逻辑**

现在，我们将扮演库设计者的角色，面对眼前的 API 框架，一步步思考如何将它变为现实。我们将通过一系列问题来引导你的思考。

#### **`MyUniquePtr` 的实现思路：独占与转移**

`MyUniquePtr` 的逻辑相对简单，是理解 RAII 和所有权的最佳起点。

*   **TODO 1.1 & 1.2 (构造/析构)：资源生命周期的绑定**
    *   **思考**：RAII 的核心是什么？是在对象的构造函数中获取资源，在析构函数中释放资源。
    *   **实现**：构造函数 `MyUniquePtr(T* ptr)` 应该做什么？很简单，用 `_ptr` 成员变量保存传入的 `ptr`。析构函数 `~MyUniquePtr()` 又该做什么？检查 `_ptr` 是否为空，如果不为空，就 `delete` 它。

*   **TODO 1.3 (禁用拷贝)：如何保证“唯一”所有权？**
    *   **思考**：如果 `MyUniquePtr` 可以被拷贝（例如 `MyUniquePtr p2 = p1;`），会发生什么？`p1` 和 `p2` 内部会拥有相同的裸指针。当它们各自析构时，就会对同一个指针调用两次 `delete`，导致程序崩溃。
    *   **实现**：为了在编译期就杜绝这种危险行为，我们需要明确禁止拷贝。使用 C++11 的 `= delete` 语法，将拷贝构造函数和拷贝赋值运算符标记为已删除。

*   **TODO 1.4 (实现移动)：如何安全地“转移”所有权？**
    *   **思考**：我们虽然禁止了拷贝，但有时确实需要将指针的所有权从一个函数转移到另一个函数。这就是“移动语义”的用武之地。移动的本质不是复制，而是“资源窃取”。
	*   **TODO 1.4 (实现移动)**：
    *   **移动构造 `MyUniquePtr(MyUniquePtr&& other)`**：这是在创建一个**新对象**。它的任务很简单：  
		1.  用 `other._ptr` 来初始化当前对象的 `_ptr`。
        2.  **（关键！）** 将 `other._ptr` 设置为 `nullptr`。
	*   **深入思考**：为什么第二步至关重要？因为如果不这样做，当 `other` 对象（它是一个右值引用，生命周期即将结束）被析构时，它依然会 `delete` 那个已经被我们“窃取”的指针，再次导致重复释放。置空 `other._ptr` 就相当于解除了它的“资源管理职责”。
		
    *   **移动赋值 `operator=(MyUniquePtr&& other)`**：这是在给一个**已存在的对象**赋值。它的逻辑更复杂：**首先，必须释放自己当前管理的旧资源；然后，才能从 `other` 接管新资源；最后，将 `other` 置空。** 并且不要忘记处理自赋值的情况！
    

*   **TODO 1.6 (`release`/`reset`)：提供灵活的所有权控制**
    *   **`release()`**：这个函数意味着“我（`MyUniquePtr`）不再负责这个指针了，现在交给你（调用者）了”。它应该返回当前的 `_ptr`，同时将 `_ptr` 内部设为 `nullptr`。
    *   **`reset(T* ptr)`**：这个函数意味着“我要换一个新的管理对象”。它应该先 `delete` 当前持有的 `_ptr`（如果存在），然后再用新的 `ptr` 替换它。

---

#### **`MySharedPtr` 与 `MyWeakPtr` 的实现思路：协作与共生**

这对组合的实现要复杂得多，关键在于理解它们如何通过一个共享的**控制块**来通信和协作。

*   **TODO 2.1 (ControlBlock)：设计共享状态的“大脑”**
    *   **思考**：为了让多个 `MySharedPtr` 共享同一个对象，我们需要一个独立于对象本身的地方来存储“有多少个 `MySharedPtr` 指向我？”。这个信息就是**强引用计数 (`strong_count`)**。
    *   **再思考**：`MyWeakPtr` 又是如何知道对象是否还存活呢？它也需要访问这个控制块。但如果最后一个 `MySharedPtr` 析构时，把控制块也删除了，那 `MyWeakPtr` 去哪里查询信息呢？这就引出了第二个需求：我们需要知道“有多少个 `MyWeakPtr` 在观察我？”。这个信息就是**弱引用计数 (`weak_count`)**。
    *   **结论**：`ControlBlock` 至少需要两个成员：`int strong_count;` 和 `int weak_count;`。

*   **TODO 2.2 & 2.4 (构造与拷贝)：所有权的诞生与传播**
    *   **`MySharedPtr(T* ptr)`**：当从一个裸指针创建第一个 `MySharedPtr` 时，这是“所有权”的源头。它必须：
        1.  用 `_ptr` 保存 `ptr`。
        2.  `new` 一个 `ControlBlock`，并用 `_control_block` 指向它。
        3.  将这个新控制块的 `strong_count` 初始化为 `1`，`weak_count` 初始化为 `0`。
    *   **`MySharedPtr(const MySharedPtr& other)` (拷贝构造)**：当拷贝一个 `MySharedPtr` 时，我们是在“传播”所有权。它应该：
        1.  将 `_ptr` 和 `_control_block` 都指向 `other` 的同名成员。
        2.  **（关键！）** 如果 `_control_block` 不是 `nullptr`，就将 `_control_block->strong_count` 的值加 1。

*   **TODO 2.8 & 2.3 (`release` 与析构)：生命周期的核心裁决**
    `release()` 是所有权变化时都会被调用的核心函数（析构、赋值给另一个指针时）。它的逻辑必须清晰无误：
    1.  首先检查 `_control_block` 是否存在，如果不存在，直接返回。
    2.  将 `strong_count` 减 1。
    3.  **第一个裁决点（对象生死）**：检查 `strong_count` 是否变成了 `0`？
        *   **是**：意味着最后一个拥有者消失了。此时，必须 `delete _ptr` 来释放被管理的对象。
        *   **否**：什么都不做，对象继续存活。
    4.  **第二个裁决点（控制块生死）**：这个检查**只在 `strong_count` 变为 0 之后**发生。在 `delete _ptr` 之后，我们接着检查 `weak_count` 是否也为 `0`？
        *   **是**：意味着最后一个拥有者和最后一个观察者都消失了。控制块本身也失去了存在的意义，`delete _control_block`。
        *   **否**：意味着还有 `MyWeakPtr` 在观察，我们必须保留控制块，以便它们可以查询到对象已“过期”。

*   **TODO 3.1 & 3.2 (`MyWeakPtr` 构造与析构)：观察者的加入与离开**
    *   **`MyWeakPtr(const MySharedPtr& shared)`**：当 `MyWeakPtr` 从 `MySharedPtr` 构造时，它只是一个“观察者”的加入。它：
        1.  拷贝 `_ptr` 和 `_control_block` 的地址。
        2.  **（关键！）** 如果 `_control_block` 存在，**只将 `weak_count` 加 1**。它绝不触碰 `strong_count`。
    *   **`~MyWeakPtr()`**：当一个观察者离开时，它在 `release()` 函数中：
        1.  将 `weak_count` 减 1。
        2.  **（关键！）** 检查 `strong_count` 是否为 `0` **并且** `weak_count` 是否也为 `0`。如果两者都满足，才 `delete _control_block`。

*   **TODO 3.3 (`lock` 与 `expired`)：`MyWeakPtr` 与 `MySharedPtr` 的神之一手**
    这是两者交互最紧密的地方，也是 `weak_ptr` 价值的最终体现。
    *   **`expired()`**：这个函数回答“我观察的对象还活着吗？”。如何判断？很简单，只需检查 `_control_block->strong_count` 是否为 `0` 即可。
    *   **`lock()`**：这个函数尝试将“观察权”安全地提升为“所有权”。它的逻辑是：
        1.  检查 `expired()` 是否为 `true`（或者直接检查 `strong_count` 是否为 0）。
        2.  如果是，说明对象已死，返回一个空的 `MySharedPtr`。
        3.  如果不是，说明对象还活着。此时，它必须**创建一个新的、有效的 `MySharedPtr` 实例**，这个新实例将共享同一个控制块，并将 `strong_count` 加 1。
    *   **深入思考**：`MyWeakPtr::lock()` 如何创建一个 `MySharedPtr` 呢？它本身无法直接修改 `strong_count`。答案就在 **TODO 2.6**：我们需要为 `MySharedPtr` 提供一个特殊的构造函数 `MySharedPtr(const MyWeakPtr<T>& weak)`。`lock()` 的实现就只有一句话：`return MySharedPtr<T>(*this);`。所有复杂的逻辑都在 `MySharedPtr` 的这个特殊构造函数中完成。这正体现了 C++ 中类之间通过接口和友元进行协作的精妙之处。

---

### **Part 3.2：指南篇 - 最佳实践与常见陷阱详解**

掌握智能指针的 API 只是第一步，真正的精髓在于理解其背后的设计哲学，并在实践中遵循最佳实践，规避那些微妙而危险的陷阱。

好的，这是一个非常重要的补充。理解 `auto_ptr` 的历史和缺陷，是真正领会 `unique_ptr` 设计精髓的关键。

我们将在“指南篇”的开头，增加一个专门的小节来详细阐述这个问题。

---

### **Part 3：指南篇 - 深入理解与最佳实践**

掌握智能指针的 API 只是第一步，真正的精髓在于理解其背后的设计哲学，并在实践中遵循最佳实践，规避那些微妙而危险的陷阱。
#### **1. 历史的教训：`auto_ptr` 的消亡与 `unique_ptr` 的崛起**

在 C++11 之前，`std::auto_ptr` 是 C++ 标准库中唯一的独占型智能指针。它的目标与 `unique_ptr` 相同：保证一个动态分配的对象只有一个所有者。然而，它的实现方式存在一个致命的设计缺陷，导致它在实践中非常危险，并最终在 C++17 中被彻底移除。

*   **`auto_ptr` 的致命缺陷：拷贝即“剪切”**

   `auto_ptr` 的问题出在它的拷贝构造函数和拷贝赋值运算符上。当你“拷贝”一个 `auto_ptr` 时，它并不会像普通对象那样创建一个副本，而是会**转移所有权**，并将源 `auto_ptr` 置空。这种行为就像“剪切-粘贴”，而非“复制-粘贴”，这完全违背了人们对拷贝的直觉。

*   **一个灾难性的例子：函数传参**

   这种“伪拷贝”行为在函数按值传参时会引发灾难。

   ```cpp
   #include <iostream>
   #include <memory> // for std::auto_ptr

   // 这个函数按值接收一个 auto_ptr
   void process_item(std::auto_ptr<int> item_ptr) {
       if (item_ptr.get()) {
           std::cout << "Processing item with value: " << *item_ptr << std::endl;
       }
   } // 函数结束，item_ptr 离开作用域，它所拥有的内存被 delete

   int main() {
       std::auto_ptr<int> my_item(new int(100));

       std::cout << "Before calling process_item, my_item points to: " << *my_item << std::endl;

       // 调用函数，my_item 被“拷贝”给参数 item_ptr
       // 实际上，所有权从 my_item 转移给了 item_ptr
       process_item(my_item); 

       // 当函数返回时，my_item 内部的指针已经被置为 nullptr
       std::cout << "After calling process_item, trying to access my_item..." << std::endl;
       
       // !!! 灾难发生点 !!!
       // 此时 my_item 是一个悬空指针，解引用它会导致未定义行为（通常是程序崩溃）
       // *my_item; 
       if (my_item.get() == nullptr) {
           std::cout << "my_item is now a null pointer!" << std::endl;
       }
       
       return 0;
   }
   ```
   **发生了什么？**
   1. `main` 函数中的 `my_item` 拥有一个值为 100 的 `int`。
   2. 调用 `process_item(my_item)` 时，`my_item` 的所有权被**静默地、隐式地**转移给了函数参数 `item_ptr`。
   3. `process_item` 函数执行完毕，其局部变量 `item_ptr` 被析构，从而 `delete` 了它拥有的内存。
   4. 控制权回到 `main` 函数，但此时 `my_item` 已经是一个空壳，它不再指向任何有效的内存。任何后续对 `my_item` 的使用都将是危险的。

*   **`unique_ptr` 如何完美解决此问题**

   `unique_ptr` 的设计者吸取了 `auto_ptr` 的教训，从两个方面解决了这个问题：

   1.  **在编译期禁止隐式的所有权转移**：`unique_ptr` **删除了拷贝构造函数和拷贝赋值运算符**。这意味着，任何试图“拷贝”`unique_ptr` 的行为，都会导致一个清晰的编译错误，而不是在运行时悄悄地埋下地雷。

       ```cpp
       std::unique_ptr<int> p1(new int(100));
       // std::unique_ptr<int> p2 = p1; // 编译错误！不允许拷贝
       // process_item(p1);            // 同样是编译错误！
       ```
       这就将一个危险的运行时逻辑错误，提升为了一个安全的编译期错误。

   2.  **引入明确的移动语义**：`unique_ptr` 仍然允许所有权转移，但**必须通过 `std::move` 显式地进行**。`std::move` 在代码中是一个清晰的信号，它告诉阅读代码的人：“注意，这里正在发生所有权的转移”。

       ```cpp
       void process_item_unique(std::unique_ptr<int> item_ptr) {
           // ... 函数内部逻辑不变 ...
       }

       int main() {
           auto my_item = std::make_unique<int>(100);

           // 必须使用 std::move 来显式转移所有权
           // 这段代码清晰地表达了“我愿意放弃 my_item 的所有权”
           process_item_unique(std::move(my_item)); 

           // 程序员现在清楚地知道，在这一行之后，my_item 不应再被使用
           if (my_item == nullptr) {
               std::cout << "After moving, my_item is now a null pointer!" << std::endl;
           }
           return 0;
       }
       ```

#### **2. 禁止：用一个裸指针初始化多个 `shared_ptr`**

这是使用 `shared_ptr` 时最严重的错误之一，必须彻底理解并避免。

*   **错误演示**
    ```cpp
    void disastrous_function() {
        int* raw_ptr = new int(42);

        // p1 创建了第一个控制块，强引用计数为 1
        std::shared_ptr<int> p1(raw_ptr); 

        // p2 用同一个裸指针，创建了第二个、完全独立的控制块！
        // 这个新控制块的强引用计数也为 1
        std::shared_ptr<int> p2(raw_ptr); 

        std::cout << "p1 use_count: " << p1.use_count() << std::endl; // 输出 1
        std::cout << "p2 use_count: " << p2.use_count() << std::endl; // 输出 1
    } // 函数结束
    // p2 析构，发现其控制块的引用计数变为 0，于是 delete raw_ptr
    // p1 析构，发现其控制块的引用计数也变为 0，于是再次 delete raw_ptr
    // 程序崩溃（重复释放）
    ```

*   **深层原因**
    `shared_ptr` 的构造函数 `shared_ptr(T* ptr)` 的职责是：为这个裸指针 `ptr` **创建一个全新的控制块**。它无从知晓这个 `ptr` 是否已经被其他 `shared_ptr` 所管理。因此，每次调用这个构造函数，都会产生一个新的、独立的“所有权链条”。

*   **正确做法：从已有的 `shared_ptr` 进行拷贝**
    要共享所有权，必须通过拷贝构造或拷贝赋值，从一个已经存在的 `shared_ptr` 实例来创建新的实例。

    ```cpp
    void correct_function() {
        std::shared_ptr<int> p1(new int(42)); // 创建了唯一的控制块

        // p2 通过拷贝 p1，共享了同一个控制块
        // 控制块的强引用计数会增加到 2
        std::shared_ptr<int> p2 = p1; 

        std::cout << "p1 use_count: " << p1.use_count() << std::endl; // 输出 2
        std::cout << "p2 use_count: " << p2.use_count() << std::endl; // 输出 2
    } // 函数结束
    // p2 析构，控制块的引用计数减为 1
    // p1 析构，控制块的引用计数减为 0，此时安全地 delete 内存
    ```

#### **3. 优先：使用 `std::make_shared` 和 `std::make_unique`**

C++11 和 C++14 分别引入了 `std::make_shared` 和 `std::make_unique`，官方强烈推荐使用它们来创建智能指针，而不是直接使用 `new`。

*   **基本用法**
    ```cpp
    // 替代: std::shared_ptr<MyClass> sp(new MyClass(10, 20));
    auto sp = std::make_shared<MyClass>(10, 20); // 构造函数的参数直接传递

    // 替代: std::unique_ptr<MyClass> up(new MyClass());
    auto up = std::make_unique<MyClass>();
    ```

*   **优势一：更高的效率 (`make_shared`)**
    `std::shared_ptr<T>(new T())` 这个操作涉及**两次**内存分配：
    1.  `new T()` 分配用于存储对象的内存。
    2.  `shared_ptr` 的构造函数内部为控制块分配内存。
    这两块内存通常是不连续的，增加了内存碎片化的可能性和缓存未命中的风险。

    而 `std::make_shared<T>()` 只进行**一次**内存分配，它会申请一块足够大的内存，同时容纳对象 `T` 和控制块。这减少了内存分配的开销，并提高了缓存局部性。

*   **优势二：更强的异常安全**
    考虑一个复杂的函数调用：
    ```cpp
    process_widgets(std::shared_ptr<Widget>(new Widget(1)), 
                    std::shared_ptr<Widget>(new Widget(2)));
    ```
    C++ 标准不保证函数参数的求值顺序。一个可能的、致命的执行顺序是：
    1.  `new Widget(1)`  执行成功。
    2.  `new Widget(2)`  执行并**抛出异常**。
    3.  栈展开开始，但此时 `std::shared_ptr` 的构造函数还未被调用来管理 `new Widget(1)` 返回的指针。
    结果是：第一个 `Widget` 的内存**泄漏**了。

    如果使用 `make_shared`，情况则完全不同：
    ```cpp
    process_widgets(std::make_shared<Widget>(1), 
                    std::make_shared<Widget>(2));
    ```
    `make_shared` 是一个独立的函数调用，它保证了在函数内部，内存分配和 `shared_ptr` 的构造是原子性地完成的。如果 `new` 抛出异常，智能指针根本不会被创建；如果智能指针创建成功，资源就已经被安全地管理了。

#### **4. 警惕：`shared_ptr` 的循环引用**

这是 `shared_ptr` 最著名的陷阱，也是 `weak_ptr` 存在的核心价值。

*   **问题场景**
    当两个或多个对象通过 `shared_ptr` 形成一个环状的相互引用时，它们的强引用计数将永远无法降为零。

    ```cpp
    struct Son;
    struct Father {
        ~Father() { std::cout << "Father destroyed\n"; }
        std::shared_ptr<Son> my_son;
    };
    struct Son {
        ~Son() { std::cout << "Son destroyed\n"; }
        std::shared_ptr<Father> my_father;
    };

    void circular_reference_demo() {
        auto father = std::make_shared<Father>(); // Father 强引用计数 = 1
        auto son = std::make_shared<Son>();     // Son 强引用计数 = 1

        // 建立相互引用
        father->my_son = son;     // Son 强引用计数变为 2
        son->my_father = father;  // Father 强引用计数变为 2
    } // 函数结束
    // son 离开作用域，Son 强引用计数从 2 减为 1 (因为 father->my_son 还在)
    // father 离开作用域，Father 强引用计数从 2 减为 1 (因为 son->my_father 还在)
    // 两个对象的引用计数都为 1，它们的析构函数永远不会被调用，造成内存泄漏
    ```

*   **解决方案：使用 `weak_ptr` 打破循环**
    `weak_ptr` 是一种“观察者”指针，它指向由 `shared_ptr` 管理的对象，但**不增加强引用计数**。它只观察，不拥有。

    我们将关系链中“较弱”的一方（或任意一方）改为 `weak_ptr` 即可。

    ```cpp
    // ... Father 定义不变 ...
    struct Son_fixed {
        ~Son_fixed() { std::cout << "Son_fixed destroyed\n"; }
        std::weak_ptr<Father> my_father; // 改为 weak_ptr
    };

    void fixed_demo() {
        auto father = std::make_shared<Father>();   // Father 强引用 = 1
        auto son = std::make_shared<Son_fixed>(); // Son 强引用 = 1

        father->my_son = son; // Son 强引用变为 2
        son->my_father = father; // Father 强引用不变 (仍为 1)，弱引用+1
    } // 函数结束
    // son 离开作用域，Son 强引用从 2 减为 1 (因为 father->my_son)
    // father 离开作用域，Father 强引用从 1 减为 0 -> Father 被销毁！
    // Father 销毁时，其成员 father->my_son 被析构 -> Son 强引用从 1 减为 0 -> Son 被销毁！
    // 内存泄漏问题解决
    ```
    当需要从 `weak_ptr` 访问对象时，必须调用 `lock()` 方法，它会返回一个 `shared_ptr`。如果对象还存在，返回有效的 `shared_ptr`；如果对象已被销毁，返回空的 `shared_ptr`。这是一种完全安全的访问方式。

#### **4. 必须：安全地从 `this` 创建 `shared_ptr`**

*   **问题根源**
    在类成员函数内部，你可能会想返回一个指向当前对象的 `shared_ptr`。直接用 `this` 创建是绝对错误的，它会犯和第一点同样的“双控制块”错误。

    ```cpp
    class BadClass {
    public:
        std::shared_ptr<BadClass> getSelf() {
            // 灾难：为 this 创建了一个全新的控制块
            return std::shared_ptr<BadClass>(this); 
        }
    };
    
    // 外部调用
    auto p1 = std::make_shared<BadClass>(); // 第一个控制块
    auto p2 = p1->getSelf();                // 第二个控制块
    // p1 和 p2 指向同一个对象，但有两个独立的引用计数，导致重复释放
    ```

*   **解决方案：`std::enable_shared_from_this`**
    这是一个“模板基类”，通过继承它，可以让你的类安全地生成指向自身的 `shared_ptr`。

    ```cpp
    #include <memory>
    
    class GoodClass : public std::enable_shared_from_this<GoodClass> {
    public:
        std::shared_ptr<GoodClass> getSelf() {
            // 正确：从已存在的控制块创建 shared_ptr
            return shared_from_this();
        }
    };

    void good_demo() {
        auto p1 = std::make_shared<GoodClass>();
        auto p2 = p1->getSelf(); // p1 和 p2 共享同一个控制块
        std::cout << p1.use_count() << std::endl; // 输出 2
    }
    ```
    **工作原理**：当你用 `make_shared` 创建 `GoodClass` 的 `shared_ptr` 时，它会检测到该类继承自 `enable_shared_from_this`，并在对象内部悄悄地存储一个指向**其控制块的 `weak_ptr`**。调用 `shared_from_this()` 实际上就是对这个内部的 `weak_ptr` 调用 `lock()`，从而安全、正确地创建新的 `shared_ptr`。

---

### **Part 4：工程篇 - 验证你的实现**

在你完成 `my_smart_ptr.hpp` 的填充后，使用下面的测试框架来验证其正确性。

#### **测试代码 (`src/main.cpp`)**

将以下代码复制到 `src/main.cpp`。

```cpp
// src/main.cpp
#include "../include/my_smart_ptr.hpp" // 假设你的头文件路径
#include <cassert>
#include <iostream>
#include <utility> // for std::move

// 用于打印构造/析构日志的测试类
struct TestClass {
    int id;
    my_std::MyWeakPtr<TestClass> weak_member;

    TestClass(int i) : id(i) { std::cout << "  [Constructor] TestClass " << id << std::endl; }
    ~TestClass() { std::cout << "  [Destructor] TestClass " << id << std::endl; }
};

class TestRunner {
public:
    void run_all_tests() {
        std::cout << "\n===== Running MyUniquePtr Tests =====" << std::endl;
        test_unique_ptr_lifecycle();
        test_unique_ptr_ownership_transfer();
        test_unique_ptr_operators();

        std::cout << "\n===== Running MySharedPtr Tests =====" << std::endl;
        test_shared_ptr_lifecycle_and_copy();
        test_shared_ptr_assignment();
        test_shared_ptr_move();
        test_shared_ptr_reset_extended(); // 使用了增强版 reset 测试
        test_shared_ptr_assignment_with_null(); // 新增的 null 赋值测试
        test_shared_ptr_edge_cases(); // 使用了增强版自我赋值测试
        // test_shared_ptr_double_wrap_issue(); // 新增的错误用法测试（默认注释）

        std::cout << "\n===== Running MyWeakPtr Tests =====" << std::endl;
        test_weak_ptr_lock_and_expired();
        test_weak_ptr_copy_and_move(); // 新增的 weak_ptr 拷贝与移动测试
        test_circular_reference_break();
        
        std::cout << "\n======================================" << std::endl;
        std::cout << "  All smart pointer tests passed!" << std::endl;
        std::cout << "======================================" << std::endl;
    }

private:
    // --- MyUniquePtr Tests (no changes from original) ---
    void test_unique_ptr_lifecycle() {
        std::cout << "\n--- Test: UniquePtr Lifecycle & Nullptr ---" << std::endl;
        {
            my_std::MyUniquePtr<TestClass> p1(new TestClass(1));
            assert(p1);
        } // CHECK: TestClass 1 should be destroyed here.
        my_std::MyUniquePtr<TestClass> p2;
        assert(!p2 && "Default constructed unique_ptr should be null");
        my_std::MyUniquePtr<TestClass> p3(nullptr);
        assert(!p3 && "Nullptr constructed unique_ptr should be null");
    }

    void test_unique_ptr_ownership_transfer() {
        std::cout << "\n--- Test: UniquePtr Ownership Transfer (move, release, reset) ---" << std::endl;
        my_std::MyUniquePtr<TestClass> p1(new TestClass(2));
        TestClass* raw_ptr_2 = p1.get();

        my_std::MyUniquePtr<TestClass> p2 = std::move(p1);
        assert(p1.get() == nullptr && "Source of move construct should be null");
        assert(p2.get() == raw_ptr_2 && "Destination of move construct should own the pointer");

        my_std::MyUniquePtr<TestClass> p3(new TestClass(3));
        p2 = std::move(p3); // TestClass 2 should be destroyed here
        assert(p3.get() == nullptr && "Source of move assignment should be null");

        TestClass* raw_ptr_released = p2.release();
        assert(p2.get() == nullptr && "unique_ptr should be null after release");
        delete raw_ptr_released;

        my_std::MyUniquePtr<TestClass> p4(new TestClass(4));
        p4.reset(new TestClass(5)); // TestClass 4 should be destroyed
        assert(p4->id == 5);
        p4.reset(); // TestClass 5 should be destroyed
        assert(p4.get() == nullptr);
    }

    void test_unique_ptr_operators() {
        std::cout << "\n--- Test: UniquePtr Operators (*, ->) ---" << std::endl;
        my_std::MyUniquePtr<TestClass> p(new TestClass(6));
        assert(p->id == 6);
        (*p).id = 66;
        assert(p->id == 66);
    }

    // --- MySharedPtr Tests ---
    void test_shared_ptr_lifecycle_and_copy() {
        std::cout << "\n--- Test: SharedPtr Lifecycle & Copy Construction ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp_null;
        assert(sp_null.use_count() == 0 && "Default constructed shared_ptr use_count should be 0");

        TestClass* raw_ptr = new TestClass(7);
        {
            my_std::MySharedPtr<TestClass> sp1(raw_ptr);
            assert(sp1.use_count() == 1);
            {
                my_std::MySharedPtr<TestClass> sp2 = sp1;
                assert(sp1.use_count() == 2 && "Copy construction should increment use_count");
                assert(sp2.use_count() == 2);
                {
                    my_std::MySharedPtr<TestClass> sp3(sp2);
                    assert(sp1.use_count() == 3);
                } // sp3 destroyed, count becomes 2
                assert(sp1.use_count() == 2);
            } // sp2 destroyed, count becomes 1
            assert(sp1.use_count() == 1);
        } // sp1 destroyed
        // CHECK: TestClass 7 should be destroyed here.
    }

    void test_shared_ptr_assignment() {
        std::cout << "\n--- Test: SharedPtr Copy Assignment ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(8));
        my_std::MySharedPtr<TestClass> sp2(new TestClass(9));
        assert(sp1.use_count() == 1);
        assert(sp2.use_count() == 1);

        sp1 = sp2; // Object 8 should be destroyed. sp1 and sp2 now share object 9.
        assert(sp1.get() == sp2.get());
        assert(sp1.use_count() == 2 && "Copy assignment should share ownership and increment count");
        assert(sp2.use_count() == 2);
    }

    void test_shared_ptr_move() {
        std::cout << "\n--- Test: SharedPtr Move Semantics ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(10));
        assert(sp1.use_count() == 1);

        my_std::MySharedPtr<TestClass> sp2 = std::move(sp1);
        assert(sp1.get() == nullptr && "Source of move construct should be null");
        assert(sp1.use_count() == 0);
        assert(sp2.use_count() == 1 && "Move construction should not change use_count");
        assert(sp2->id == 10);

        my_std::MySharedPtr<TestClass> sp3(new TestClass(11));
        sp3 = std::move(sp2); // Object 11 should be destroyed
        assert(sp2.get() == nullptr && "Source of move assignment should be null");
        assert(sp2.use_count() == 0);
        assert(sp3.use_count() == 1 && "Move assignment should not change use_count");
        assert(sp3->id == 10);
    }
    
    // [IMPROVED] - Expanded test for reset()
    void test_shared_ptr_reset_extended() {
        std::cout << "\n--- Test: SharedPtr reset() Extended ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp(new TestClass(12));
        my_std::MyWeakPtr<TestClass> wp = sp; // Create a weak pointer to the resource
        assert(sp.use_count() == 1);
        assert(!wp.expired());

        // 1. Reset with a new pointer
        sp.reset(new TestClass(13)); // Object 12 should be destroyed here
        assert(sp->id == 13);
        assert(sp.use_count() == 1);
        assert(wp.expired() && "Weak pointer should be expired after shared_ptr reset");

        // 2. Reset to null
        sp.reset(); // Object 13 should be destroyed here
        assert(sp.get() == nullptr);
        assert(sp.use_count() == 0);
    }
    
    // [NEW] - Added test for assigning to/from null
    void test_shared_ptr_assignment_with_null() {
        std::cout << "\n--- Test: SharedPtr Assignment with Null ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(14));
        my_std::MySharedPtr<TestClass> sp_null;

        sp1 = sp_null; // Assigning null to a valid pointer
        // CHECK: TestClass 14 should be destroyed here
        assert(sp1.get() == nullptr);
        assert(sp1.use_count() == 0);

        my_std::MySharedPtr<TestClass> sp2;
        sp2 = my_std::MySharedPtr<TestClass>(new TestClass(15));
        assert(sp2.use_count() == 1);
        assert(sp2->id == 15);
    }

    // [IMPROVED] - More rigorous self-assignment checks
    void test_shared_ptr_edge_cases() {
        std::cout << "\n--- Test: SharedPtr Edge Cases (self-assignment) ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(16));
        TestClass* raw_ptr = sp1.get();

        // Self-copy-assignment
        sp1 = sp1; 
        assert(sp1.get() == raw_ptr && "Self-copy-assignment should not change the pointer");
        assert(sp1.use_count() == 1 && "Self-copy-assignment should not change use_count");

        // Self-move-assignment
        sp1 = std::move(sp1); 
        assert(sp1.get() == raw_ptr && "Self-move-assignment should be a no-op");
        assert(sp1.use_count() == 1 && "Self-move-assignment should not change state");
    }

    // [NEW] - Test demonstrating incorrect usage that leads to double-free
    // This test is expected to fail or crash, proving the danger of wrapping a raw pointer twice.
    // It's commented out by default to allow the test suite to pass.
    /*
    void test_shared_ptr_double_wrap_issue() {
        std::cout << "\n--- Test: SharedPtr Double Wrap (EXPECTED CRASH) ---" << std::endl;
        TestClass* raw_ptr = new TestClass(17);
        {
            my_std::MySharedPtr<TestClass> sp1(raw_ptr);
            my_std::MySharedPtr<TestClass> sp2(raw_ptr); // DANGER: Two control blocks now own the same pointer.
        } // The first destructor will delete raw_ptr. The second will cause a double-free.
    }
    */

    // --- MyWeakPtr Tests ---
    void test_weak_ptr_lock_and_expired() {
        std::cout << "\n--- Test: WeakPtr lock() & expired() ---" << std::endl;
        my_std::MyWeakPtr<TestClass> wp1; // Default constructed
        assert(wp1.expired() && "Default constructed weak_ptr should be expired");

        my_std::MySharedPtr<TestClass> sp_null;
        wp1 = sp_null;
        assert(wp1.expired() && "Weak pointer from null shared_ptr should be expired");
        
        {
            my_std::MySharedPtr<TestClass> sp2(new TestClass(18));
            my_std::MyWeakPtr<TestClass> wp2 = sp2;
            assert(!wp2.expired());
            assert(wp2.use_count() == 1);

            {
                auto locked_sp = wp2.lock();
                assert(locked_sp);
                assert(locked_sp->id == 18);
                assert(sp2.use_count() == 2 && "lock() should create a new shared_ptr and increment count");
            } // locked_sp destroyed, count back to 1
            assert(sp2.use_count() == 1);
        } // sp2 destroyed, object 18 should be destroyed
        // CHECK: TestClass 18 should be destroyed here.
        // assert(wp2.expired()); // wp2 is out of scope here.
    }

    // [NEW] - Added test for weak_ptr copy and move semantics
    void test_weak_ptr_copy_and_move() {
        std::cout << "\n--- Test: WeakPtr Copy & Move ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp(new TestClass(19));
        
        my_std::MyWeakPtr<TestClass> wp1 = sp;
        // NOTE: A public weak_count() would be needed to assert weak count directly.
        // We test its behavior indirectly via lock() and expired().
        
        {
            my_std::MyWeakPtr<TestClass> wp2 = wp1; // Test copy constructor
            assert(!wp2.expired());

            my_std::MyWeakPtr<TestClass> wp3;
            wp3 = wp2; // Test copy assignment
            assert(wp3.lock()->id == 19);
        } // wp2 and wp3 are destroyed, weak count should decrease but not affect the object.
        
        assert(!wp1.expired()); // The original weak pointer is still valid.

        my_std::MyWeakPtr<TestClass> wp4 = std::move(wp1); // Test move constructor
        assert(wp1.expired() && "Source of move construct should be expired/empty");
        assert(!wp4.expired());
        assert(wp4.lock()->id == 19);
    }

    void test_circular_reference_break() {
        std::cout << "\n--- Test: WeakPtr Breaks Circular Reference ---" << std::endl;
        {
            my_std::MySharedPtr<TestClass> node1(new TestClass(20));
            my_std::MySharedPtr<TestClass> node2(new TestClass(21));
            
            node1->weak_member = node2;
            node2->weak_member = node1;
            
            assert(node1.use_count() == 1);
            assert(node2.use_count() == 1);
        }
        // CHECK: Verify that TestClass 20 and 21 destructors are both called.
    }
};

int main() {
    TestRunner runner;
    runner.run_all_tests();
    return 0;
}

/*
--- COMPILE AND RUN RECOMMENDATIONS ---

For more robust testing, especially for memory-related issues like leaks,
double-frees, or use-after-frees, compile with AddressSanitizer and
UndefinedBehaviorSanitizer.

Example command (GCC/Clang):
g++ -g -std=c++11 -fsanitize=address,undefined -I./include src/main.cpp -o smart_ptr_test

Then run the compiled executable:
./smart_ptr_test

This will automatically detect many subtle memory errors that might not cause an
obvious crash in a simple run.
*/
```

#### **编译与运行**

1.  打开终端，进入 `SmartPointerProject` 根目录。
2.  执行编译命令：`g++ -g -std=c++11 -fsanitize=address,undefined -I./include src/main.cpp -o smart_ptr_test`
3.  执行测试程序：`./smart_ptr_test`

如果你的实现正确，程序将顺利运行并通过所有断言，你将能从输出的日志中清晰地看到每个对象的构造和析构过程。

---

### **附录：参考实现**

**请在独立完成你的实现之后，再来查阅此处的参考答案。**

```cpp
// include/my_smart_ptr.hpp (完整实现)
#ifndef MY_SMART_PTR_HPP
#define MY_SMART_PTR_HPP

#include <utility> // for std::move, std::swap
#include <iostream>

namespace my_std {

// 前向声明，为后续的 friend 和参数类型做准备
template<typename T> class MySharedPtr;
template<typename T> class MyWeakPtr;

// ====================================================================================
//                                  MyUniquePtr
// ====================================================================================
template <typename T>
class MyUniquePtr {
public:
    // 构造函数：获取裸指针的所有权。
    // explicit 关键字防止从裸指针到 MyUniquePtr 的隐式类型转换。
    explicit MyUniquePtr(T* ptr = nullptr) : _ptr(ptr) {}

    // 析构函数：RAII 核心。在对象生命周期结束时，自动释放其管理的内存。
    ~MyUniquePtr() {
        if (_ptr) delete _ptr;
    }

    // 禁用拷贝语义：通过 '= delete' 明确禁止拷贝构造和拷贝赋值，
    // 这是保证所有权“唯一”的编译期强制手段。
    MyUniquePtr(const MyUniquePtr& other) = delete;
    MyUniquePtr& operator=(const MyUniquePtr& other) = delete;

    // 移动构造函数：从另一个 MyUniquePtr "窃取"资源。
    // noexcept 保证移动操作不抛出异常，这对于标准库容器优化至关重要。
    MyUniquePtr(MyUniquePtr&& other) noexcept : _ptr(other._ptr) {
        // 关键一步：将源对象的指针置空，防止其析构时重复释放内存。
        other._ptr = nullptr;
    }

    // 移动赋值运算符：
    MyUniquePtr& operator=(MyUniquePtr&& other) noexcept {
        // 防止自赋值 (例如: ptr = std::move(ptr))
        if (this != &other) {
            // 1. 释放当前对象持有的资源
            if (_ptr) delete _ptr;
            // 2. 从源对象窃取资源
            _ptr = other._ptr;
            // 3. 将源对象置空
            other._ptr = nullptr;
        }
        return *this;
    }

    // 指针操作符重载
    T& operator*() const { return *_ptr; }
    T* operator->() const { return _ptr; }

    // 管理接口
    T* get() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

    // release: 放弃所有权，并返回裸指针。调用者现在需要手动管理这块内存。
    T* release() {
        T* temp = _ptr;
        _ptr = nullptr;
        return temp;
    }

    // reset: 释放当前管理的指针，并接管一个新的指针。
    void reset(T* ptr = nullptr) {
        if (_ptr) delete _ptr;
        _ptr = ptr;
    }
private:
    T* _ptr;
};

// ====================================================================================
//                                  ControlBlock & Co.
// ====================================================================================
// 控制块：独立于被管理的对象，用于存储强引用和弱引用计数。
struct ControlBlock {
    int strong_count = 0; // 决定对象生命周期
    int weak_count = 0;   // 决定控制块自身生命周期
};

template <typename T>
class MySharedPtr {
    // 友元声明：允许 MyWeakPtr 访问 MySharedPtr 的私有成员，
    // 这是实现 MyWeakPtr::lock() 功能所必需的。
    friend class MyWeakPtr<T>;
public:
    // 从裸指针构造：这是所有权的起点。
    // 创建一个新的控制块，并将强引用计数初始化为 1。
    explicit MySharedPtr(T* ptr = nullptr) : _ptr(ptr) {
        if (_ptr) {
            _control_block = new ControlBlock{1, 0};
        }
    }

    // 析构函数：调用 release 减少引用计数。
    ~MySharedPtr() {
        release();
    }

    // 拷贝构造函数：共享所有权。
    // 拷贝指针，并使强引用计数加 1。
    MySharedPtr(const MySharedPtr& other) : _ptr(other._ptr), _control_block(other._control_block) {
        if (_control_block) {
            _control_block->strong_count++;
        }
    }

    // 拷贝赋值运算符：
    MySharedPtr& operator=(const MySharedPtr& other) {
        if (this != &other) {
            // 1. 先对自己当前持有的资源执行 release 操作，减少旧资源的引用计数。
            release();
            // 2. 再从 other 拷贝，共享新资源。
            _ptr = other._ptr;
            _control_block = other._control_block;
            if (_control_block) {
                _control_block->strong_count++;
            }
        }
        return *this;
    }
    
    // 移动构造函数：转移所有权，比拷贝更高效。
    MySharedPtr(MySharedPtr&& other) noexcept : _ptr(other._ptr), _control_block(other._control_block) {
        // 将源对象置空，这样它析构时就不会影响引用计数。
        other._ptr = nullptr;
        other._control_block = nullptr;
    }
    
    // 移动赋值运算符：
    MySharedPtr& operator=(MySharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            _ptr = other._ptr;
            _control_block = other._control_block;
            other._ptr = nullptr;
            other._control_block = nullptr;
        }
        return *this;
    }
    
    // (高级) 从 MyWeakPtr 构造：这是 lock() 功能的核心。
    // 这是一个关键的、非 explicit 的构造函数。
    MySharedPtr(const MyWeakPtr<T>& weak) : _ptr(weak._ptr), _control_block(weak._control_block) {
        // 只有当资源还存在时 (strong_count > 0)，才增加强引用计数，构造一个有效的 MySharedPtr。
        if (_control_block && _control_block->strong_count > 0) {
            _control_block->strong_count++;
        } else {
            // 否则，说明对象已被销毁，构造一个空的 MySharedPtr。
            _ptr = nullptr;
            _control_block = nullptr;
        }
    }

    // 常用接口
    int use_count() const { return _control_block ? _control_block->strong_count : 0; }
    T* get() const { return _ptr; }
    T& operator*() const { return *_ptr; }
    T* operator->() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

private:
    // 核心的资源释放辅助函数
    void release() {
        if (!_control_block) return; // 如果是空指针，什么都不做。

        _control_block->strong_count--;
        if (_control_block->strong_count == 0) {
            // 当强引用计数为 0 时，释放被管理的对象。
            if(_ptr) delete _ptr;
            _ptr = nullptr;
            
            // 此时，检查弱引用计数。如果也为 0，说明已没有任何指针(shared或weak)
            // 指向控制块，可以释放控制块本身。
            if (_control_block->weak_count == 0) {
                delete _control_block;
                _control_block = nullptr;
            }
        }
    }

    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

template <typename T>
class MyWeakPtr {
    friend class MySharedPtr<T>;
public:
    MyWeakPtr() = default; // 默认构造函数

    // 从 MySharedPtr 构造：创建一个观察者。
    // 拷贝指针，但只增加弱引用计数，不影响对象的生命周期。
    MyWeakPtr(const MySharedPtr<T>& shared) : _ptr(shared._ptr), _control_block(shared._control_block) {
        if (_control_block) {
            _control_block->weak_count++;
        }
    }

    ~MyWeakPtr() {
        release();
    }
    
    // 省略了拷贝/移动构造和赋值的实现，但原理与 MySharedPtr 类似，只是操作对象是 weak_count。
    
    // 检查被观察的对象是否已被销毁。
    bool expired() const {
        return use_count() == 0;
    }

    // 尝试获取一个有效的 MySharedPtr。这是与被管理对象交互的唯一安全方式。
    MySharedPtr<T> lock() const {
        // 利用 MySharedPtr 的特殊构造函数 MySharedPtr(const MyWeakPtr<T>&) 来实现。
        // 如果对象有效，会返回一个共享所有权的 MySharedPtr；否则返回空指针。
        return MySharedPtr<T>(*this);
    }

    // 返回当前对象的强引用计数。
    int use_count() const {
        return _control_block ? _control_block->strong_count : 0;
    }

private:
    // MyWeakPtr 的释放逻辑
    void release() {
        if (!_control_block) return;

        _control_block->weak_count--;
        // 关键：当强引用为 0 且弱引用也为 0 时，说明控制块的生命周期结束。
        if (_control_block->strong_count == 0 && _control_block->weak_count == 0) {
            delete _control_block;
            _control_block = nullptr;
        }
    }

    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

} // namespace my_std
```
---

### **附录：C++ 核心概念补充**

在实现智能指针的过程中，我们用到了一些 C++ 的关键特性，如 `explicit`, `noexcept`, `friend` 等。理解这些关键字的含义和用途，对于写出安全、高效、健壮的 C++ 代码至关重要。

#### **1. 为什么构造函数要用 `explicit`？**

**一句话概括：为了防止意外的、危险的隐式类型转换。**

在 C++ 中，如果一个构造函数可以只用一个参数来调用，那么它默认也定义了一种从参数类型到类类型的“隐式转换规则”。

*   **问题场景 (没有 `explicit`)**
    假设我们的 `MyUniquePtr` 构造函数不是 `explicit`：
    ```cpp
    // MyUniquePtr(T* ptr = nullptr); // 非 explicit

    void process_ptr(MyUniquePtr<int> p) {
        // ... 函数接管所有权 ...
    }

    int main() {
        int* raw_ptr = new int(42);
        process_ptr(raw_ptr); // 危险！这行代码竟然可以编译通过！
        // *raw_ptr = 10;     // 灾难！raw_ptr 已经是悬空指针
    }
    ```
    **发生了什么？** 编译器看到 `process_ptr` 需要一个 `MyUniquePtr<int>`，而你提供了一个 `int*`。于是它会自动查找是否存在一个可以用 `int*` 调用的构造函数。它找到了 `MyUniquePtr(int*)`，然后**悄悄地**为你创建了一个临时的 `MyUniquePtr` 对象，并用它来调用函数。这导致 `raw_ptr` 的所有权在你不知不觉中就被转移了，极易导致悬空指针错误。

*   **解决方案 (`explicit`)**
    `explicit` 关键字告诉编译器：“嘿，这个构造函数只能用于直接、显式的初始化，绝不能用于隐式转换！”

    ```cpp
    // explicit MyUniquePtr(T* ptr = nullptr); // 使用 explicit

    // process_ptr(raw_ptr); // 编译错误！编译器会阻止这个危险的隐式转换
    
    // 正确的、意图明确的调用方式：
    process_ptr(MyUniquePtr<int>(raw_ptr)); 
    ```
    **结论**：对单参数构造函数（或所有参数都有默认值的构造函数）使用 `explicit`，是一种防御性编程的最佳实践，它能让你的代码更安全、意图更清晰。

#### **2. 为什么移动操作要用 `noexcept`？**

**一句话概括：为了在标准库容器（如 `std::vector`）中获得巨大的性能提升，并保证异常安全。**

`noexcept` 是一个承诺，你向编译器保证这个函数绝对不会抛出任何异常。

*   **问题场景 (没有 `noexcept`)**
    想象一个 `std::vector<MyUniquePtr<int>>`。当 vector 的容量不足需要扩容时，它会：
    1.  申请一块更大的新内存。
    2.  将所有旧内存中的元素**移动**到新内存中。
    3.  释放旧内存。

    在第 2 步中，vector 会尝试调用 `MyUniquePtr` 的移动构造函数。但如果这个移动构造函数**没有**被标记为 `noexcept`，编译器就必须做最坏的打算：万一移动到一半时抛出异常怎么办？

    为了提供“强异常保证”（即操作失败时，vector 状态回滚到操作之前的样子），如果移动构造函数可能抛异常，`std::vector` **不敢**使用它。它会退而求其次，选择更安全但效率低下的**拷贝构造函数**来逐一“复制”元素。但我们的 `MyUniquePtr` 已经禁止了拷贝，所以这会导致编译错误！即使允许拷贝，对于复杂对象，拷贝的开销也远大于移动。

*   **解决方案 (`noexcept`)**
    当你将移动构造和移动赋值标记为 `noexcept` 时，你就向编译器做出了庄严的承诺。

    ```cpp
    MyUniquePtr(MyUniquePtr&& other) noexcept { /* ... */ }
    MyUniquePtr& operator=(MyUniquePtr&& other) noexcept { /* ... */ }
    ```
    现在，`std::vector` 在扩容时，会放心地调用你高效的移动操作，因为它知道这个过程绝对不会中途失败。

    **结论**：对于那些仅仅是转移成员（如交换指针）而绝不应该失败的移动操作，**总是**将它们标记为 `noexcept`，这是现代 C++ 的一条重要准则。

#### **3. 为什么 `release()` 有时公有，有时私有？**

**一句话概括：这取决于类的“设计契约”，即它向用户承诺了什么样的所有权管理模型。**

*   **`MyUniquePtr::release()` 是 `public` 的**
    *   **原因**：`unique_ptr` 代表的是**可转移的独占所有权**。它的设计契约中包含了“用户有权在任何时候收回所有权，转为手动管理”这一条。这在与只接受裸指针的 C 风格 API 交互时非常有用。`release()` 是其核心功能之一，必须对用户可见。
    *   **示例**：`C_API_takes_ownership(my_uptr.release());`

*   **`MySharedPtr::release()` 是 `private` 的**
    *   **原因**：`shared_ptr` 代表的是**自动化的共享所有权**。它的核心承诺是“你完全不用担心资源的释放，交给我自动管理”。如果向用户暴露一个 `public` 的 `release()`，就会打破这个承诺。用户一旦调用它，就会得到一个裸指针，而此时可能还有其他 `shared_ptr` 在共享这个对象，引用计数机制会立刻被破坏，导致混乱和崩溃。
    *   **定位**：在 `MySharedPtr` 中，`release()` 纯粹是一个**内部实现细节**。它是被析构函数和赋值运算符调用的一个辅助函数，用来封装复杂的引用计数逻辑，绝不应该被外部用户直接干预。

#### **4. 简单讲一下友元 (`friend`)**

**一句话概括：`friend` 是一种受控地“打破”封装的机制，允许一个外部函数或类访问另一个类的 `private` 和 `protected` 成员。**

封装是 C++ 面向对象编程的基石，它隐藏了类的内部实现。但有时，两个类（或一个类和一个函数）在概念上是紧密耦合的，它们需要像一个整体一样协同工作。

*   **我们教程中的场景**
    `MySharedPtr` 和 `MyWeakPtr` 就是这样一对紧密耦合的类。`MyWeakPtr` 的核心功能 `lock()`，需要创建一个 `MySharedPtr`。这个创建过程很特殊：
    1.  它需要访问 `MyWeakPtr` 实例的私有成员 `_ptr` 和 `_control_block`。
    2.  它需要调用 `MySharedPtr` 的一个特殊构造函数（即 `MySharedPtr(const MyWeakPtr<T>&)`），这个构造函数需要根据控制块的状态来决定是创建有效指针还是空指针。

    为了实现这种高效的内部协作，又不想把所有成员都变成 `public` 破坏封装，`friend` 就是最佳选择。

*   **如何使用**
    ```cpp
    class MySharedPtr {
        // 这行代码声明：MyWeakPtr<T> 是我的朋友，
        // 它可以访问我所有的私有成员。
        friend class MyWeakPtr<T>;
        // ...
    };
    ```
    通过这个声明，`MySharedPtr` 的特殊构造函数 `MySharedPtr(const MyWeakPtr<T>&)` 就可以顺利地访问传入的 `weak` 对象的私有成员 `weak._ptr` 和 `weak._control_block`，从而完成 `lock()` 的功能。

    **结论**：`friend` 应当谨慎使用。但对于像 `shared_ptr` 和 `weak_ptr` 这样，在逻辑上属于同一个组件的两个部分，使用友元是一种清晰且合理的设计选择。