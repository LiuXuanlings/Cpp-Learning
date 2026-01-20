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
 * A template class that implements a simple stack structure.
 * This demostrates how to use alloctor_traits (specifically with MemoryPool)
 */

#ifndef STACK_ALLOC_H
#define STACK_ALLOC_H

#include <memory>

template <typename T>
struct StackNode_
{
  T data;
  StackNode_* prev;
};

/** T is the object to store in the stack, Alloc is the allocator to use */
template <class T, class Alloc = std::allocator<T> >
class StackAlloc
{
  public:
    typedef StackNode_<T> Node;
    // 将通用分配器Alloc适配为专门分配Node类型的分配器，并定义类型别名allocator
    typedef typename Alloc::template rebind<Node>::other allocator;
    //       ↑          ↑      ↑          ↑         ↑        ↑
    //       1          2      3          4         5        6
    // 1. typename：声明后续是依赖模板参数的类型名（必须，否则编译器无法识别为类型）
    // 2. Alloc：原始分配器模板参数（如std::allocator<int>）
    // 3. ::template：核心！声明rebind是Alloc内部的模板成员 ，
    //    如果省略 ::template，编译器会报错（比如 “expected primary-expression before ‘<’ token”），因为它会误把 < 当成小于号解析。
    // 4. rebind<Node>：分配器的模板方法，将Alloc的分配类型绑定为Node
    // 5. ::other：rebind绑定后得到的新分配器类型（专用于分配Node）
    // 6. allocator：简化的类型别名，后续直接用该名称指代绑定后的分配器

    /** Default constructor */
    StackAlloc() {head_ = 0; }
    /** Default destructor */
    ~StackAlloc() { clear(); }

    /** Returns true if the stack is empty */
    bool empty() {return (head_ == 0);}

    /** Deallocate all elements and empty the stack */
    void clear() {
      Node* curr = head_;
      while (curr != 0)
      {
        Node* tmp = curr->prev;
        allocator_.destroy(curr);
        allocator_.deallocate(curr, 1);
        curr = tmp;
      }
      head_ = 0;
    }

    /** Put an element on the top of the stack */
    void push(T element) {
      Node* newNode = allocator_.allocate(1);
      allocator_.construct(newNode, Node());
      newNode->data = element;
      newNode->prev = head_;
      head_ = newNode;
    }

    /** Remove and return the topmost element on the stack */
    T pop() {
      T result = head_->data;
      Node* tmp = head_->prev;
      allocator_.destroy(head_);
      allocator_.deallocate(head_, 1);
      head_ = tmp;
      return result;
    }

    /** Return the topmost element */
    T top() { return (head_->data); }

  private:
    allocator allocator_;
    Node* head_;
};

#endif // STACK_ALLOC_H