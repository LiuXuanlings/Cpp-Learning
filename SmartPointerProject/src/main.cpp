// src/main.cpp
#include "../include/my_smart_ptr.hpp"
#include <cassert>
#include <iostream>
#include <utility> // for std::move

// 用于打印构造/析构日志的测试类
struct TestClass {
    int id;
    // 注意：MyWeakPtr也需要有拷贝/移动赋值运算符才能用于下面的测试
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
        test_shared_ptr_reset();
        test_shared_ptr_edge_cases();

        std::cout << "\n===== Running MyWeakPtr Tests =====" << std::endl;
        test_weak_ptr_lock_and_expired();
        test_circular_reference_break();
        
        std::cout << "\n======================================" << std::endl;
        std::cout << "  All smart pointer tests passed!" << std::endl;
        std::cout << "======================================" << std::endl;
    }

private:
    // --- MyUniquePtr Tests ---
    void test_unique_ptr_lifecycle() {
        std::cout << "\n--- Test: UniquePtr Lifecycle & Nullptr ---" << std::endl;
        {
            my_std::MyUniquePtr<TestClass> p1(new TestClass(1));
            assert(p1); // 测试 operator bool()
        } // CHECK OUTPUT: TestClass 1 should be destroyed here.

        my_std::MyUniquePtr<TestClass> p2; // 默认构造
        assert(!p2 && "Default constructed unique_ptr should be null");
        
        my_std::MyUniquePtr<TestClass> p3(nullptr);
        assert(!p3 && "Nullptr constructed unique_ptr should be null");
    }

    void test_unique_ptr_ownership_transfer() {
        std::cout << "\n--- Test: UniquePtr Ownership Transfer (move, release, reset) ---" << std::endl;
        my_std::MyUniquePtr<TestClass> p1(new TestClass(2));
        TestClass* raw_ptr_2 = p1.get();

        // 测试移动构造
        my_std::MyUniquePtr<TestClass> p2 = std::move(p1);
        assert(p1.get() == nullptr && "Source of move construct should be null");
        assert(p2.get() == raw_ptr_2 && "Destination of move construct should own the pointer");

        // 测试移动赋值
        my_std::MyUniquePtr<TestClass> p3(new TestClass(3));
        TestClass* raw_ptr_3 = p3.get();
        p2 = std::move(p3); // TestClass 2 should be destroyed here
        assert(p3.get() == nullptr && "Source of move assignment should be null");
        assert(p2.get() == raw_ptr_3 && "Destination of move assignment should own the new pointer");

        // 测试 release
        TestClass* raw_ptr_released = p2.release();
        assert(raw_ptr_released == raw_ptr_3);
        assert(p2.get() == nullptr && "unique_ptr should be null after release");
        delete raw_ptr_released; // Manually delete the released pointer

        // 测试 reset
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

                my_std::MySharedPtr<TestClass> sp3(sp2);
                assert(sp1.use_count() == 3);
            } // sp3 is destroyed, use_count becomes 2
            assert(sp1.use_count() == 2);
        } // sp1, sp2 are destroyed
        // CHECK OUTPUT: TestClass 7 should be destroyed here.
    }

    void test_shared_ptr_assignment() {
        std::cout << "\n--- Test: SharedPtr Copy Assignment ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(8));
        my_std::MySharedPtr<TestClass> sp2(new TestClass(9));
        assert(sp1.use_count() == 1);
        assert(sp2.use_count() == 1);

        sp1 = sp2; // sp1's original object (8) should be destroyed. sp1 and sp2 now share object (9).
        assert(sp1.get() == sp2.get());
        assert(sp1.use_count() == 2 && "Copy assignment should share ownership and increment count");
        assert(sp2.use_count() == 2);

        my_std::MySharedPtr<TestClass> sp3;
        sp3 = sp1;
        assert(sp1.use_count() == 3);
    }

    void test_shared_ptr_move() {
        std::cout << "\n--- Test: SharedPtr Move Semantics ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(10));
        assert(sp1.use_count() == 1);

        // 测试移动构造
        my_std::MySharedPtr<TestClass> sp2 = std::move(sp1);
        assert(sp1.get() == nullptr && "Source of move construct should be null");
        assert(sp1.use_count() == 0);
        assert(sp2.use_count() == 1 && "Move construction should not change use_count");
        assert(sp2->id == 10);

        // 测试移动赋值
        my_std::MySharedPtr<TestClass> sp3(new TestClass(11));
        sp3 = std::move(sp2); // Object 11 should be destroyed
        assert(sp2.get() == nullptr && "Source of move assignment should be null");
        assert(sp2.use_count() == 0);
        assert(sp3.use_count() == 1 && "Move assignment should not change use_count");
        assert(sp3->id == 10);
    }

    void test_shared_ptr_reset() {
        std::cout << "\n--- Test: SharedPtr reset() ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp(new TestClass(12));
        assert(sp.use_count() == 1);
        
        // 如果你的API有名为reset的函数，用它；否则用 sp = MySharedPtr<...>()
        // 假设实现了 reset
        // sp.reset(new TestClass(13)); // Object 12 should be destroyed
        // assert(sp->id == 13);
        // assert(sp.use_count() == 1);

        // sp.reset(); // Object 13 should be destroyed
        // assert(sp.get() == nullptr);
        // assert(sp.use_count() == 0);
    }
    
    void test_shared_ptr_edge_cases() {
        std::cout << "\n--- Test: SharedPtr Edge Cases (self-assignment) ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1(new TestClass(14));
        sp1 = sp1; // Test self-copy-assignment
        assert(sp1.use_count() == 1 && "Self-copy-assignment should not change use_count");

        sp1 = std::move(sp1); // Test self-move-assignment
        assert(sp1.use_count() == 1 && "Self-move-assignment should not change state");
    }

    // --- MyWeakPtr Tests ---
    void test_weak_ptr_lock_and_expired() {
        std::cout << "\n--- Test: WeakPtr lock() & expired() ---" << std::endl;
        my_std::MySharedPtr<TestClass> sp1;
        my_std::MyWeakPtr<TestClass> wp1 = sp1;
        assert(wp1.expired() && "Weak pointer from null shared_ptr should be expired");
        assert(wp1.lock().get() == nullptr && "Lock on expired weak_ptr should return null shared_ptr");
        
        {
            my_std::MySharedPtr<TestClass> sp2(new TestClass(15));
            my_std::MyWeakPtr<TestClass> wp2 = sp2;
            assert(!wp2.expired());
            assert(wp2.use_count() == 1);

            {
                auto locked_sp = wp2.lock();
                assert(locked_sp);
                assert(locked_sp->id == 15);
                assert(sp2.use_count() == 2 && "lock() should create a new shared_ptr and increment count");
            } // locked_sp destroyed, count back to 1
            assert(sp2.use_count() == 1);
        } // sp2 destroyed, object 15 should be destroyed
        // CHECK OUTPUT: TestClass 15 should be destroyed here.
        assert(wp1.expired() && "Weak pointer should be expired after shared_ptr is destroyed");
    }

    void test_circular_reference_break() {
        std::cout << "\n--- Test: WeakPtr Breaks Circular Reference ---" << std::endl;
        {
            my_std::MySharedPtr<TestClass> node1(new TestClass(16));
            my_std::MySharedPtr<TestClass> node2(new TestClass(17));
            
            // 建立弱引用，不会导致循环引用
            node1->weak_member = node2;
            node2->weak_member = node1;
            
            assert(node1.use_count() == 1);
            assert(node2.use_count() == 1);
        }
        // CHECK OUTPUT: Verify that TestClass 16 and 17 destructors are both called,
        // which proves the circular reference was broken.
    }
};

int main() {
    TestRunner runner;
    runner.run_all_tests();
    return 0;
}