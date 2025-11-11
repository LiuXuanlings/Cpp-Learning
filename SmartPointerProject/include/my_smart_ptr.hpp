// include/my_smart_ptr.hpp
#ifndef MY_SMART_PTR_HPP
#define MY_SMART_PTR_HPP

#include <utility> // for std::move

namespace my_std {

// 前向声明
template<typename T> class MySharedPtr;
template<typename T> class MyWeakPtr;

// ====================================================================================
//                                  MyUniquePtr 框架
// ====================================================================================
template <typename T>
class MyUniquePtr {
public:
    // TODO 1.1: 实现构造函数
    explicit MyUniquePtr(T* ptr = nullptr) { 
        _ptr = ptr;
    }
    // TODO 1.2: 实现析构函数
    ~MyUniquePtr() { 
        if(_ptr != nullptr){
            delete _ptr;
        }
    }

    // TODO 1.3: 禁用拷贝
    MyUniquePtr(const MyUniquePtr& other) = delete;
    MyUniquePtr& operator=(const MyUniquePtr& other) = delete;

    // TODO 1.4: 实现移动构造和移动赋值
    MyUniquePtr(MyUniquePtr&& other) noexcept { 
            _ptr = other._ptr;
            other._ptr = nullptr;
    }
    MyUniquePtr& operator=(MyUniquePtr&& other) noexcept { 
        if(this != &other){
            if(_ptr != nullptr) delete _ptr;
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this; 
    }

    // TODO 1.5: 实现指针操作
    T& operator*() const { return *_ptr; }
    T* operator->() const { return _ptr; }

    // TODO 1.6: 实现管理接口
    T* get() const { return _ptr; }
    T* release() { 
        auto ptr = _ptr;
        _ptr = nullptr;
        return ptr; 
    }
    void reset(T* ptr = nullptr) { 
        if(_ptr != nullptr){
            delete _ptr;
        }
        _ptr = ptr;
    }
    explicit operator bool() const { return _ptr != nullptr; }

private:
    T* _ptr;
};

// ====================================================================================
//                                  ControlBlock & Co. 框架
// ====================================================================================
// TODO 2.1: 定义控制块结构体
struct ControlBlock {
    size_t strong_count = 0;
    size_t weak_count = 0;
};

template <typename T>
class MySharedPtr {
    friend class MyWeakPtr<T>;
public:
    // TODO 2.2: 实现构造函数
    explicit MySharedPtr(T* ptr = nullptr) { 
        _ptr = ptr;
        if(ptr == nullptr){
            _control_block = nullptr;
        }else{
            _control_block = new ControlBlock;
            _control_block->strong_count = 1;
            _control_block->weak_count = 0;
        }

    }
    // TODO 2.3: 实现析构函数
    ~MySharedPtr() { 
        release();
    }

    // TODO 2.4: 实现拷贝构造和拷贝赋值
    MySharedPtr(const MySharedPtr& other) {  
        _ptr = other._ptr;
        _control_block = other._control_block;
        if(_control_block != nullptr)
            _control_block->strong_count++;
    }
    MySharedPtr& operator=(const MySharedPtr& other) { 
        if(this != &other){
            if(_control_block != nullptr){//检查控制快而不是指针
                release();
            }
            _ptr = other._ptr;
            _control_block = other._control_block;
            if(_control_block != nullptr)
                _control_block->strong_count++;
        }
        return *this; 
    }
    
    // TODO 2.5: 实现移动构造和移动赋值
    MySharedPtr(MySharedPtr&& other) noexcept { 
        _ptr = other._ptr;
        _control_block = other._control_block;
        other._ptr = nullptr;
        other._control_block = nullptr;
    }
    MySharedPtr& operator=(MySharedPtr&& other) noexcept { 
        if(this != &other){
            release();
            _ptr = other._ptr;
            _control_block = other._control_block;
            other._ptr = nullptr;
            other._control_block = nullptr;
        }
        return *this; 
    }
    
    // TODO 2.6: 实现从 MyWeakPtr 构造 (用于 lock())
    MySharedPtr(const MyWeakPtr<T>& weak) {  
        if(!weak.expired()){
            _ptr = weak._ptr;
            _control_block = weak._control_block;
            _control_block->strong_count++;
        }else {
            _ptr = nullptr;
            _control_block = nullptr;
        }
    }

    // TODO 2.7: 实现常用接口
    void reset(T* ptr = nullptr) { 
        release();
        if(ptr != nullptr){
            _ptr = ptr;
            _control_block = new ControlBlock;
            _control_block->strong_count = 1;
            _control_block->weak_count = 0;
        }
    }

    int use_count() const { 
        if(_control_block)
            return _control_block->strong_count; 
        return 0;
    }
    T* get() const { return _ptr; }
    T& operator*() const { return *_ptr; }
    T* operator->() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

private:
    // TODO 2.8: 实现私有的 release 辅助函数
    void release() {
        if (_control_block == nullptr) return;

        _control_block->strong_count--;
        if (_control_block->strong_count == 0) {
            delete _ptr;
            if (_control_block->weak_count == 0) {
                delete _control_block;
            }
        }

        _ptr = nullptr;
        _control_block = nullptr;
    }


    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

template <typename T>
class MyWeakPtr {
    friend class MySharedPtr<T>;
public:
    // TODO 3.1: 实现构造函数
    MyWeakPtr() { _ptr = nullptr; _control_block = nullptr; }
    MyWeakPtr(const MySharedPtr<T>& shared) { 
        _ptr = shared._ptr;
        _control_block = shared._control_block;   
        if(shared._control_block)
            shared._control_block->weak_count++;
    }

    // TODO 3.2: 实现析构函数
    ~MyWeakPtr() { 
        release();
    }

    MyWeakPtr(const MyWeakPtr& other) { 
        _ptr = other._ptr;
        _control_block = other._control_block;
        if(_control_block)
            _control_block->weak_count++;
    }
    MyWeakPtr& operator=(const MyWeakPtr& other) { 
        if(this != &other){
            release();
            _ptr = other._ptr;
            _control_block = other._control_block;
            if(_control_block)
                _control_block->weak_count++;
        }
        return *this;
    }

    MyWeakPtr(MyWeakPtr&& other) noexcept{
        _ptr = other._ptr;
        _control_block = other._control_block;
        other._ptr = nullptr;
        other._control_block = nullptr;
    }
    MyWeakPtr& operator=(MyWeakPtr&& other) {
        if(this != &other){
            release();
            _ptr = other._ptr;
            _control_block = other._control_block;
            other._ptr = nullptr;
            other._control_block = nullptr;
        }
        return *this;
    }

    // TODO 3.3: 实现核心功能
    bool expired() const { 
        if(_control_block)
            return _control_block->strong_count == 0; 
        return 1;
    }
    MySharedPtr<T> lock() const { return MySharedPtr<T>(*this); }
    int use_count() const { 
        if(_control_block)
            return _control_block->strong_count; 
        return 0;
    }

private:
    // TODO 3.4: 实现私有的 release 辅助函数
    void release() { 
        if(_control_block){
            _control_block->weak_count--;
            if(_control_block->strong_count == 0 && _control_block->weak_count == 0) 
                delete _control_block;
        }
    }

    T* _ptr {nullptr};
    ControlBlock* _control_block {nullptr};
};

} // namespace my_std
#endif // MY_SMART_PTR_HPP



/*****************************************************************************************
 *                                                                                       *
 *                     我的第一版代码错误经验集锦 (My V1.0 Bug Fixes)                      *
 *                                                                                       *
 *   这是我根据第一版代码的编译错误和逻辑问题总结出的宝贵经验。每一个错误都是一个深刻的教训， *
 *   记录在此，以备将来回顾。                                                              *
 *                                                                                       *
 *****************************************************************************************/

/*
------------------------------------------------------------------------------------------
|                                                                                        |
|                            核心逻辑类错误 (Logic Bugs)                                 |
|                                                                                        |
------------------------------------------------------------------------------------------

### 教训 1：赋值运算符 (`operator=`) 必须处理旧资源

- **我的错误 (V1.0)**：在 `MySharedPtr` 的拷贝赋值运算符中，我直接用 `other` 的成员
  覆盖了 `this` 的成员，忘记了 `this` 可能已经管理着一块内存，导致旧资源泄漏。
  
  // 错误版本 V1.0 的伪代码
  MySharedPtr& operator=(const MySharedPtr& other) {
      // 错误：没有释放 this 原本管理的资源
      _ptr = other._ptr;
      _control_block = other._control_block;
      _control_block->strong_count++;
  }

- **后果**：导致 `this` 原本指向的对象的引用计数永远不会减少，从而引发 **内存泄漏**。

- **深刻教训**：**赋值操作 = 释放旧资源 + 获取新资源**。在获取新资源（拷贝
  `other` 的成员）之前，必须先对自己当前管理的资源调用 `release()`，履行释放
  旧资源的职责。


### 教训 2：`release()` 逻辑必须严谨，区分对象和控制块的生命周期

- **我的错误 (V1.0)**：`release()` 函数的判断条件不完整，没有正确处理控制块的释放时机。

  // 错误版本 V1.0 的伪代码
  void release() {
      _control_block->strong_count--;
      if (_control_block->strong_count == 0) delete _ptr;
      // 错误：当 strong_count > 0 时，这个判断永远为假；
      // 当 strong_count = 0, weak_count > 0 时，控制块应该保留，但谁来最终删除它？
      if (_control_block->weak_count == 0) delete _control_block;
  }

- **后果**：当 `strong_count` 变为 0 但 `weak_count` 不为 0 时，控制块会被遗留下
  来，造成**控制块泄漏**。

- **深刻教训**：
    1. **对象 (`_ptr`) 的生死**：只由 `strong_count` 决定。它在 `strong_count` 
       减为 0 的**那一刻**被删除。
    2. **控制块 (`_control_block`) 的生死**：由 `strong_count` **和** `weak_count` 
       共同决定。它必须在两者都为 0 的**那一刻**被删除。
    - 这意味着 `MySharedPtr::release()` 和 `MyWeakPtr::release()` 的逻辑必须协同工作。



### 教训 X (新增)：养成良好习惯，Delete 之后立即置空

- **潜在风险**: 在 `release()` 函数中，当我 `delete _ptr;` 之后，`_ptr` 变量本身
  还保留着那个已经无效的内存地址，成了一个“悬空指针”。

- **后果**: 虽然在当前代码中可能不会立即引发问题，但如果未来有人修改代码，在 `delete`
  之后无意中再次使用了 `_ptr`，就会导致严重的未定义行为。这是一种潜在的安全隐患。

- **深刻教训（黄金法则）**: **释放指针指向的内存后，立即将指针本身设为 `nullptr`**。
  这可以从根本上杜绝悬空指针问题，是一种极其重要的防御性编程习惯。

  // 最佳实践
  delete _ptr;
  _ptr = nullptr;

  delete _control_block;
  _control_block = nullptr;

------------------------------------------------------------------------------------------
|                                                                                        |
|                  空指针与边界条件错误 (Null Pointer & Edge Cases)                      |
|                                                                                        |
------------------------------------------------------------------------------------------

### 教训 3：构造函数必须优雅地处理 `nullptr`

- **我的错误 (V1.0)**：`MySharedPtr(T* ptr)` 构造函数没有正确处理 `ptr` 为 
  `nullptr` 的情况，依然为其 `new` 了一个 `ControlBlock`。

- **后果**：为一个空指针创建了不必要的管理开销，不符合 `std::shared_ptr` 的行为。

- **深刻教训**：一个空的智能指针，其内部的资源指针和控制块指针**都应该是 `nullptr`**。
  构造函数是所有逻辑的入口，必须在一开始就处理好空指针这种最基本的边界条件。


### 教训 4：所有操作前，必须检查指针是否为空

- **我的错误 (V1.0)**：在 `MySharedPtr` 的拷贝构造/赋值和 `MyWeakPtr` 的构造函数中，
  直接对 `_control_block->strong_count++` 或 `->weak_count++` 进行操作，没有检查
  `_control_block` 本身是否为 `nullptr`。

- **后果**：当用一个空的智能指针去构造或赋值时，程序会因访问空指针而**立即崩溃**。

- **深刻教训**：在使用任何指针之前，**永远**要先问自己：“这个指针有可能是 `nullptr`
  吗？”。如果是，就必须加上 `if (_control_block)` 这样的保护性检查。

------------------------------------------------------------------------------------------
|                                                                                        |
|                         C++ 核心概念类错误 (Core C++ Concepts)                         |
|                                                                                        |
------------------------------------------------------------------------------------------

### 教训 5：构造函数的上下文是 `this`，不是临时变量

- **我的错误 (V1.0)**：在 `MySharedPtr(const MyWeakPtr<T>&)` 构造函数中，创建了
  一个局部的临时变量 `shared` 并对其操作，完全忽略了正在被构造的 `this` 对象。

  // 错误版本 V1.0
  MySharedPtr(const MyWeakPtr<T>& weak) {  
      if (!weak.expired()) {
          MySharedPtr<T> shared; // 致命错误：创建了无关的局部变量
          shared._ptr = weak._ptr;
          // ...
      }
  } // 局部变量 shared 在此被销毁，*this 什么都没得到

- **后果**：构造函数执行完毕后，新创建的 `MySharedPtr` 对象是空的，完全没有达到预期效果。

- **深刻教训**：**构造函数的唯一职责是初始化 `this`**。所有的数据成员赋值都应该是对
  `_ptr`、`_control_block`（即 `this->_ptr`、`this->_control_block`）进行。

### 教训 6：`#include` 路径与编译环境

- **我的错误 (V1.0)**：在 `src/main.cpp` 中使用了不正确的相对路径 
  `#include "include/..."`。

- **深刻教训**：
    1. **理解相对路径**: 路径是相对于当前 `.cpp` 文件的。
    2. **掌握 `-I` 标志**: 这是更专业、更健壮的解决方案，它让代码与文件系统的
       物理布局解耦。通过 `g++ ... -Iinclude` 命令，可以在代码中直接使用
       `#include "my_smart_ptr.hpp"`。


### 教训 7：拷贝赋值运算符必须检查控制块而非原始指针

- **我的错误 (V1.0)**：在 `MySharedPtr` 的拷贝赋值运算符中，错误地检查了 `_ptr` 是否为 `nullptr` 来决定是否释放当前资源，而非检查实际代表所有权的 `_control_block`。

  ```cpp
  // 错误版本 V1.0 的伪代码
  MySharedPtr& operator=(const MySharedPtr& other) { 
      if(this != &other){
          if(_ptr != nullptr){  // 致命错误：应检查 _control_block
              release();
          }
          _ptr = other._ptr;
          _control_block = other._control_block;
          if(_control_block != nullptr) {
              _control_block->strong_count++;
          }
      }
      return *this; 
  }
  ```

- **后果**：
  1. **内存泄漏**：如果当前对象拥有控制块但 `_ptr` 为 `nullptr`（例如 `release()` 后未正确置空），`release()` 不会被调用，导致控制块和托管对象永久泄漏。
  2. **逻辑混乱**：所有权的判定与原始指针状态耦合，而非与控制块耦合，破坏了资源管理的封装性。当 `_ptr` 和 `_control_block` 状态不一致时（通常由其他 bug 导致），此判断会加剧未定义行为。
  3. **测试通过假象**：在简单的测试用例中可能碰巧正确，但在复杂的移动赋值后接拷贝赋值等混合场景下，极易触发双重释放或泄漏。

- **深刻教训**：
  1. **语义清晰性**：智能指针的"所有权"语义由**控制块**体现，而不是原始指针。所有生命周期决策（是否释放、是否递增计数）必须基于控制块的存在与否。
  2. **单一数据源 (Single Source of Truth)**：`_control_block` 是判断对象是否管理资源的唯一可靠依据。检查 `_ptr` 是冗余且危险的，因为它不是权威状态标识。
  3. **防御性编程**：永远使用 `if (_control_block != nullptr)` 作为资源管理操作的入口保护，这能确保所有后续操作（如 `release()`）都在安全的上下文中执行。
*/