#include <iostream>
#include <cassert>
#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 步骤1专属测试：验证 Block 分配与游标移动
void TestBlockAllocation()
{
    std::cout << "[Test] Block Allocation & Alignment..." << std::endl;
    
    // 1. 创建一个内存池，Block大小设为 1024 字节
    MemoryPool pool(1024);
    // 2. 初始化槽大小为 8 字节
    pool.init(8);

    // 3. 连续分配 10 个对象
    // 由于此时 FreeList 为空，应触发 allocateNewBlock
    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    
    assert(p1 != nullptr);
    assert(p2 != nullptr);

    // 4. 验证地址连续性（因为是顺序从 Block 切分的）
    // p2 应该在 p1 之后，且相差 8 字节（未考虑复杂的 Padding 情况下，理想状态）
    size_t addr1 = reinterpret_cast<size_t>(p1);
    size_t addr2 = reinterpret_cast<size_t>(p2);
    
    std::cout << "Addr1: " << addr1 << ", Addr2: " << addr2 << std::endl;
    assert(addr2 - addr1 == 8);

    // 5. 验证对齐
    assert(addr1 % 8 == 0);
    assert(addr2 % 8 == 0);

    std::cout << "[Pass] Block Allocation basic logic works." << std::endl;
}

int main()
{
    TestBlockAllocation();
    return 0;
}