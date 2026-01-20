/*-
 * Copyright (c) 2013 Cosku Acay, http://www.coskuacay.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*-
 * Provided to compare the default allocator to MemoryPool
 *
 * Check out StackAlloc.h for a stack implementation that takes an allocator as
 * a template argument. This may give you some idea about how to use MemoryPool.
 *
 * This code basically creates two stacks: one using the default allocator and
 * one using the MemoryPool allocator. It pushes a bunch of objects in them and
 * then pops them out. We repeat the process several times and time how long
 * this takes for each of the stacks.
 *
 * Do not forget to turn on optimizations (use -O2 or -O3 for GCC). This is a
 * benchmark, we want inlined code.
 */

#include <iostream>
#include <cassert>
#include <time.h>
#include <vector>

#include "MemoryPool.h"
#include "StackAlloc.h"

/* Adjust these values depending on how much you trust your computer */
#define ELEMS 1000000
#define REPS 50

int main()
{
  clock_t start;

  std::cout << "Copyright (c) 2013 Cosku Acay, http://www.coskuacay.com\n";
  std::cout << "Provided to compare the default allocator to MemoryPool.\n\n";

  /* Use the default allocator */
  StackAlloc<int, std::allocator<int> > stackDefault;
  start = clock();
  for (int j = 0; j < REPS; j++)
  {
    assert(stackDefault.empty());
    /**
    * 【循环展开 (Loop Unrolling) 技术说明】
    * 
    * 1. 背景：
    *    MemoryPool::push/pop 属于极高性能的纳秒级操作。在进行基准测试（Benchmark）时，
    *    标准的 for 循环管理逻辑（包括：计数器 i 的自增、边界条件判断 i < n、以及 CPU 跳转指令）
    *    所产生的固定开销会占到总计时结果的显著比例。
    * 
    * 2. 目的（"time the actual code and not the loop"）：
    *    为了剔除循环控制逻辑对计时结果的干扰，作者采用了循环展开。
    * 
    * 3. 逻辑：
    *    通过将循环次数缩减为 1/4，并在单次迭代中手动排列 4 次函数调用，将原本需要执行 4 次的
    *    循环管理开销降低到了 1 次。这样可以显著提高“业务代码”与“管理代码”的时间权重比，
    *    使最终测得的秒数更纯净地反映内存分配器（Allocator）本身的物理性能。
    */
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackDefault.push(i);
      stackDefault.push(i);
      stackDefault.push(i);
      stackDefault.push(i);
    }
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackDefault.pop();
      stackDefault.pop();
      stackDefault.pop();
      stackDefault.pop();
    }
  }
  std::cout << "Default Allocator Time: ";
  std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";

  /* Use MemoryPool */
  StackAlloc<int, MemoryPool<int> > stackPool;
  start = clock();
  for (int j = 0; j < REPS; j++)
  {
    assert(stackPool.empty());
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackPool.push(i);
      stackPool.push(i);
      stackPool.push(i);
      stackPool.push(i);
    }
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackPool.pop();
      stackPool.pop();
      stackPool.pop();
      stackPool.pop();
    }
  }
  std::cout << "MemoryPool Allocator Time: ";
  std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";


  std::cout << "Here is a secret: the best way of implementing a stack"
            " is a dynamic array.\n";

  /* Compare MemoryPool to std::vector */
  std::vector<int> stackVector;
  start = clock();
  for (int j = 0; j < REPS; j++)
  {
    assert(stackVector.empty());
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackVector.push_back(i);
      stackVector.push_back(i);
      stackVector.push_back(i);
      stackVector.push_back(i);
    }
    for (int i = 0; i < ELEMS / 4; i++) {
      // Unroll to time the actual code and not the loop
      stackVector.pop_back();
      stackVector.pop_back();
      stackVector.pop_back();
      stackVector.pop_back();
    }
  }
  std::cout << "Vector Time: ";
  std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";

  std::cout << "The vector implementation will probably be faster.\n\n";
  std::cout << "MemoryPool still has a lot of uses though. Any type of tree"
            " and when you have multiple linked lists are some examples (they"
            " can all share the same memory pool).\n";

  return 0;
}