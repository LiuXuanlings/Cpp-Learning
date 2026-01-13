## 步骤3：HashBucket 多规格池管理与模板封装
### 步骤总览
本步骤将完成 v1 版本的最后一块拼图。在步骤1和2中，我们实现了一个支持特定对象大小的内存池 `MemoryPool`。但在实际项目中，对象大小千变万化。
本步骤的核心是实现 **HashBucket（哈希桶）**，它充当“路由器”的角色：根据用户申请的字节数（如 12B、30B），快速计算出对应的数组下标，将请求分发给管理对应大小（如 16B、32B）的 `MemoryPool` 实例。同时，我们将利用 **C++ 模板编程** 和 **Placement New** 技术，封装出类似 `new/delete` 的高层接口，实现“内存分配+对象构造”的一体化。

### include/目录
#### MemoryPool.h
##### 改造后代码
```cpp
// ... (前序 MemoryPool 类定义保持不变) ...

class HashBucket
{
public:
    // 初始化所有规格的内存池
    static void initMemoryPool();
    // 获取指定索引的内存池实例
    static MemoryPool& getMemoryPool(int index);

    // 核心路由函数：根据大小分配内存
    static void* useMemory(size_t size)
    {
        if (size <= 0)
            return nullptr;
        
        // 超过最大规格（512字节）的对象，内存池不接管，直接走系统申请
        if (size > MAX_SLOT_SIZE) 
            return operator new(size);

        // TODO：【映射算法】计算 size 对应的内存池索引
        // 1. 目标：将大小映射到 0 ~ (MEMORY_POOL_NUM-1) 的范围内
        // 2. 规则：按 SLOT_BASE_SIZE (8字节) 对齐
        //    例如：1-8字节 -> index 0; 9-16字节 -> index 1
        // 3. 算法公式：((size + 7) / 8) - 1
        //    (size + 7) / 8 实现了向上取整（Ceil）的效果
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
    }

    // 核心路由函数：根据大小释放内存
    static void freeMemory(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        if (size > MAX_SLOT_SIZE)
        {
            operator delete(ptr);
            return;
        }

        // 同样的映射算法找到对应的池进行回收
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
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        // 在地址 p 上构造对象 T，传入参数 args
        new(p) T(std::forward<Args>(args)...);

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
```
##### 模块讲解
- **功能定位**：`HashBucket` 是对外暴露的统一入口。它隐藏了底层 64 个 `MemoryPool` 对象的细节，提供了类型安全（Type-Safe）的泛型接口。
- **语法要点**：
  - **Placement New (`new(p) T(...)`)**：标准 `new` 操作符会同时做“申请内存”和“调用构造函数”两件事。但在内存池中，内存是预先申请好的。Placement New 允许我们在**已分配的内存地址**上调用构造函数，这是实现自定义内存管理器的必备技能。
  - **变长模板参数 (`Args&&... args`)**：配合 `std::forward`，允许 `newElement` 接受任意数量、任意类型的参数，并完美转发给对象的构造函数，实现与标准 `new` 一致的灵活性。
  - **显式析构 (`p->~T()`)**：与 Placement New 对应。由于内存不是通过系统 `delete` 释放的（而是归还给池子），系统不会自动调用析构函数，必须手动显式调用，否则会导致对象内部管理的资源（如 `std::vector`）泄漏。
- **设计思路**：
  - **哈希映射（Hash Mapping）**：这里的“Hash”实际上是一个简单的线性映射（除法）。将对象大小规整化为 8 的倍数，直接映射到数组下标。相比于 `std::map` 或复杂的 Hash 函数，这种 O(1) 的数学计算效率最高。

### src/目录
#### MemoryPool.cpp
##### 改造后代码
```cpp
#include "../include/MemoryPool.h"

namespace Kama_memoryPool 
{

// ... (MemoryPool 成员函数实现保持不变) ...

void HashBucket::initMemoryPool()
{
    // TODO：【初始化逻辑】初始化 64 个不同规格的内存池
    // 1. 遍历 0 到 MEMORY_POOL_NUM-1
    // 2. 计算每个池子对应的 Slot 大小：(i + 1) * SLOT_BASE_SIZE
    //    即：idx 0 -> 8B, idx 1 -> 16B ... idx 63 -> 512B
    // 3. 调用 MemoryPool::init 完成每个实例的初始化
    for (int i = 0; i < MEMORY_POOL_NUM; i++)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
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
```
##### 模块讲解
- **功能定位**：实现了 `HashBucket` 的静态成员。由于内存池是全局通用的基础设施，使用静态成员（Static Members）可以保证全程序只有一个 `HashBucket` 管理体系。
- **语法要点**：
  - **Meyers Singleton**：在 `getMemoryPool` 中使用 `static MemoryPool memoryPool[...]`。C++11 保证了静态局部变量初始化的线程安全性。这意味着我们不需要显式加锁就能安全地创建这 64 个内存池对象。
- **设计思路**：
  - **空间换时间**：直接开辟 64 个 `MemoryPool` 对象（即使某些大小可能很少用到）。相比于动态查找或红黑树管理大小类，数组索引访问是绝对的最快路径。

### tests/目录
#### UnitTest.cpp
恢复完整的 `UnitTest.cpp`，进行基准测试（Benchmark），对比系统 `new/delete` 与内存池的性能。

##### 完整测试代码
```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <chrono> // 使用 chrono 计时更准确
#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 步骤3专属测试：基准性能对比

// 定义几个不同大小的测试类
class P1 { int id_; };             // 4 bytes -> 对齐到 8 bytes
class P2 { int id_[5]; };          // 20 bytes -> 对齐到 24 bytes
class P3 { int id_[10]; };         // 40 bytes
class P4 { int id_[20]; };         // 80 bytes

// 内存池性能测试函数
void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks); 
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k) 
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				auto begin1 = std::chrono::high_resolution_clock::now();
				for (size_t i = 0; i < ntimes; i++)
				{
                    // 使用内存池接口
                    P1* p1 = newElement<P1>(); deleteElement<P1>(p1);
                    P2* p2 = newElement<P2>(); deleteElement<P2>(p2);
                    P3* p3 = newElement<P3>(); deleteElement<P3>(p3);
                    P4* p4 = newElement<P4>(); deleteElement<P4>(p4);
				}
				auto end1 = std::chrono::high_resolution_clock::now();
				total_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end1 - begin1).count();
			}
		});
	}
	for (auto& t : vthread) t.join();
	printf("[MemoryPool] %lu 线程, %lu 轮, 每轮 %lu 次: 总耗时 %lu ms\n", nworks, rounds, ntimes, total_costtime);
}

// 系统 New/Delete 性能测试函数
void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				auto begin1 = std::chrono::high_resolution_clock::now();
				for (size_t i = 0; i < ntimes; i++)
				{
                    // 使用系统接口
                    P1* p1 = new P1; delete p1;
                    P2* p2 = new P2; delete p2;
                    P3* p3 = new P3; delete p3;
                    P4* p4 = new P4; delete p4;
				}
				auto end1 = std::chrono::high_resolution_clock::now();
				total_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end1 - begin1).count();
			}
		});
	}
	for (auto& t : vthread) t.join();
	printf("[System New] %lu 线程, %lu 轮, 每轮 %lu 次: 总耗时 %lu ms\n", nworks, rounds, ntimes, total_costtime);
}

int main()
{
    // 必须先初始化内存池
    HashBucket::initMemoryPool(); 

    // 参数：单轮次数 10000，线程数 4，轮次 10
	BenchmarkMemoryPool(10000, 4, 10); 
	std::cout << "------------------------------------------------" << std::endl;
	BenchmarkNew(10000, 4, 10); 
	
	return 0;
}
```

### 步骤3测试指导
- **测试目标**：
  1. 验证 `newElement/deleteElement` 模板接口能否正确分配内存并构造/析构对象。
  2. 通过基准测试对比内存池与系统 `new` 的性能差异。
- **测试命令**：
  ```bash
  # 编译命令
  g++ -o step3_benchmark src/MemoryPool.cpp tests/UnitTest.cpp -I include/ -std=c++11 -pthread -O2
  
  # 运行命令
  ./step3_benchmark
  ```
  *(注：建议开启 -O2 优化，以便更真实地反映性能差距，但是实测发现开启 -O2 优化后，system new的总耗时为 0 ms)*
- **通过标准**：
  - 编译无报错，运行无崩溃。
  - **性能表现**：在小对象（8B-80B）高频分配场景下，`[MemoryPool]` 的总耗时应小于或接近 `[System New]`。
    - *预期*：由于 v1 版本仅实现了简单的无锁栈，且多线程下多个 `MemoryPool` 之间虽然独立，但同一个 `MemoryPool` 内部仍有原子操作竞争。在 4 线程下，内存池通常能获得 **1.5x - 3x** 的性能提升（取决于操作系统 malloc 的实现优化程度）。
- **常见错误**：
  1. **忘记调用 initMemoryPool**：如果在 `main` 函数开始前未调用 `HashBucket::initMemoryPool()`，所有 `MemoryPool` 的 `SlotSize_` 均为 0，导致除以零错误或分配逻辑异常。
     - *解决方法*：务必在程序入口处显式初始化。
  2. **映射越界**：如果在 `useMemory` 中没有判断 `size > MAX_SLOT_SIZE`，对于大对象（如 1024B），计算出的索引将超出数组范围（64），导致内存破坏。
     - *解决方法*：严格检查 size 上限，超过 512B 的走 `operator new`。



-----

### 1\. Meyers Singleton：最优雅的“懒汉”

> **原话：** “在 `getMemoryPool` 中使用 `static MemoryPool memoryPool[...]`。C++11 保证了静态局部变量初始化的线程安全性。”

#### 它的需求（为什么我们需要它？）

假设你有一个巨大的仓库（内存池），你希望：

1.  **全局唯一**：全程序只有这一个仓库，大家都要用它。
2.  **按需创建**：如果没人用，就别建，省地皮（省内存）。这叫“懒汉模式”。
3.  **多线程安全**：如果有两个人（两个线程）同时第一次跑过来要用仓库，不能因为大家动作太快，结果建了两个仓库，或者建到一半就崩了。

在 C++11 之前，为了同时满足这三点，程序员需要写很多复杂的加锁代码（Mutex），非常容易写错。

#### 它的解决（Meyers Singleton）

Scott Meyers（C++ 大神）提出了一个极其简单的写法，利用了 C++11 的新特性：

```cpp
MemoryPool& getMemoryPool() {
    // 就在函数肚子里定义 static 变量
    static MemoryPool pool[64]; 
    return pool;
}
```

#### 为什么好理解？

  * **Static 的作用**：在函数内部的 `static` 变量，虽然写在函数里，但它**只会被初始化一次**，并且生命周期贯穿整个程序运行期间。
  * **C++11 的魔法**：编译器现在承诺，当执行到 `static xxx` 这行代码时，如果多个线程同时冲进来，编译器会自动在这个变量构建完成前“挡住”其他线程。
  * **结论**：你不用写锁，编译器帮你加了锁。既实现了“懒加载”（第一次调用才创建），又实现了“绝对安全”。

-----

### 2\. 变长模板参数：神奇的省略号 `...`

> **原话：** “变长模板参数 (`Args&&... args`)”

#### 它的需求

你想写一个函数 `newElement`，它的作用是帮用户 `new` 一个对象。
但是，你不知道用户想创建的对象构造函数里需要几个参数：

  * 有的对象构造不需要参数：`Point()`
  * 有的需要两个：`Point(10, 20)`
  * 有的需要五个字符串...

以前我们得写无数个重载函数，或者干脆没法写。我们希望能写一个函数，**接受任意数量、任意类型的参数**。

#### 它的解决（... 的妙用）

C++11 引入了 `...`（Parameter Pack，参数包）。

#### 怎么记忆 `...` 的位置？

这通常是初学者最晕的地方：三个点到底放哪？请记住这个\*\*“打包 -\> 传递 -\> 拆包”\*\*的三部曲逻辑：

1.  **定义模板（打包类型）**：放在 `typename` 后面。

      * `template <typename... Args>`
      * **含义**：告诉我，接下来有一堆**类型**，我给它们起个名叫 `Args`。

2.  **函数参数（打包数值）**：放在类型 `Args` 后面。

      * `void newElement(Args... args)`
      * **含义**：根据上面那一堆类型，生成一堆**形参变量**，名叫 `args`。

3.  **函数体内（拆包使用）**：放在变量名 `args` 后面。

      * `T* p = new T(args...);`
      * **含义**：把这堆变量里的东西**倒出来**，逗号隔开，喂给构造函数。

**记忆口诀：**

> **类型后面跟三点，表示“一堆类型”。**
> **变量后面跟三点，表示“把包拆开”。**

-----

### 3\. 完美转发：不做“二道贩子”

> **原话：** “配合 `std::forward`... 完美转发给对象的构造函数。”

#### 它的需求

你在写 `newElement` 这个函数，你是个\*\*“中间人”\*\*。
用户把参数传给你，你再把参数传给对象的构造函数。

这里有个大坑：**C++ 中，一旦你给一个变量起了名字，它就变成了“左值”（持久存在的变量）。**

  * **场景**：用户传给你一个**临时变量**（比如数字 `5`，或者 `string("abc")`）。这是一次性的东西，本来可以直接“移动（Move）”给构造函数，效率极高。
  * **问题**：因为你用 `args` 接住了它，`args` 有了名字，它就变成了“左值”。当你把它传给构造函数时，构造函数不得不进行一次\*\*“拷贝（Copy）”\*\*，而不是“移动”。这造成了性能浪费。

#### 它的解决 (`Args&&` + `std::forward`)

我们要当一个诚实的“中间人”：**如果用户给我的是临时的，我传下去也得是临时的；如果用户给我是持久的，我传下去就是持久的。**

这就是**完美转发**。

1.  **`Args&&` (万能引用)**：

      * 当它在模板参数里出现时，它不是“右值引用”，而是“万能引用”。它像一个黑洞，什么都能吸进来（左值、右值都能接）。

2.  **`std::forward<Args>(args)`**：

      * 这是一个强制类型转换的“复原器”。
      * 它会看 `Args` 的原始类型。如果它本来是临时的，就把它转回临时的（让构造函数可以 Move）；如果本来是持久的，就保持原样。

**代码全貌解析：**

```cpp
// 1. 定义：Args 是一个类型包
template <typename... Args> 
// 2. 参数：args 是一个参数包，&& 表示万能引用（啥都能接）
void newElement(Args&&... args) { 
    // 3. 使用：std::forward 还原它的本来面目，... 拆包传给构造函数
    new T(std::forward<Args>(args)...); 
}
```

-----

### 总结：一句话理解

1.  **Meyers Singleton**：把 `static` 变量藏在函数里，C++ 编译器自动帮你搞定多线程安全，不用自己加锁。
2.  **变长模板参数 (`...`)**：
      * `typename...` 是**声明**一堆类型。
      * `args...` 是**拆开**这堆参数去使用。
3.  **完美转发 (`std::forward`)**：做一个“透明”的中间商，不因转手了一次就改变参数“临时”或“持久”的性质，保证传给构造函数时效率最高。
