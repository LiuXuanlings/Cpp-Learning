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