#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <set>
#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 1. 复用性测试：验证释放的内存是否真的回到了链表并被再次分配
void TestReuse()
{
    std::cout << "[Test] Memory Reuse Logic..." << std::endl;
    MemoryPool pool(4096);
    pool.init(8);

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();

    // 记录 p1 的地址
    size_t addr1 = reinterpret_cast<size_t>(p1);
    
    // 释放 p1，此时 p1 应该成为 freeList_ 的头节点
    pool.deallocate(p1);

    // 再次分配，根据 LIFO 原则，应该立刻拿到 p1
    void* p3 = pool.allocate();
    size_t addr3 = reinterpret_cast<size_t>(p3);

    assert(addr1 == addr3); // 核心校验：地址必须相同
    
    pool.deallocate(p2);
    pool.deallocate(p3);
    std::cout << "[Pass] Memory reused successfully (LIFO confirmed)." << std::endl;
}

// 2. 多线程并发测试：验证无锁操作是否有数据竞争导致崩溃或丢数据
void TestConcurrency()
{
    std::cout << "[Test] Multi-threaded Allocate/Deallocate..." << std::endl;
    MemoryPool pool(4096); // 较小的 Block 触发频繁竞争
    pool.init(8);

    const int THREAD_COUNT = 8;
    const int OPS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;

    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j)
            {
                void* p = pool.allocate();
                // 简单的负载模拟
                *reinterpret_cast<int*>(p) = j; 
                pool.deallocate(p);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }
    std::cout << "[Pass] Multi-threaded operations finished without crash." << std::endl;
}

int main()
{
    TestReuse();
    TestConcurrency();
    return 0;
}