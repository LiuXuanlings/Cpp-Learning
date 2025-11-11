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