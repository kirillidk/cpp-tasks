#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <cassert>

#include "shared_ptr.h"

// template <typename T>
// using shared_ptr = std::shared_ptr<T>;
//
// template <typename T>
// using weak_ptr = std::weak_ptr<T>;
//
// template <typename T>
// using enable_shared_from_this = std::enable_shared_from_this<T>;
//
// template <typename T, typename... Args>
// shared_ptr<T> make_shared(Args&&... args) {
//     return std::make_shared<T>(std::forward<Args>(args)...);
// }
//
// template <typename T, typename Alloc, typename... Args>
// shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args) {
//     return std::allocate_shared<T>(std::forward<const Alloc>(alloc),
//                                    std::forward<Args>(args)...);
// }

struct Base {
    virtual ~Base() {
    }
};

struct Derived : public Base {
};

void test_shared_ptr() {
    using std::vector;

    auto first_ptr = shared_ptr<vector<int> >(new vector<int>(1'000'000));

    (*first_ptr)[0] = 1;

    vector<int>& vec = *first_ptr;
    auto second_ptr = shared_ptr<vector<int> >(new vector<int>(vec));

    (*second_ptr)[0] = 2;

    for (int i = 0; i < 1'000'000; ++i) first_ptr.swap(second_ptr);
    first_ptr->swap(*second_ptr);

    assert(first_ptr->front() == 2);
    assert(second_ptr->front() == 1);

    assert(first_ptr.use_count() == 1);
    assert(second_ptr.use_count() == 1);

    for (int i = 0; i < 10; ++i) {
        auto third_ptr = shared_ptr<vector<int> >(new vector<int>(vec));
        auto fourth_ptr = second_ptr;
        fourth_ptr.swap(third_ptr);
        assert(second_ptr.use_count() == 2);
    }

    assert(second_ptr.use_count() == 1);

    {
        vector<shared_ptr<vector<int> > > ptrs(
            10,
            shared_ptr<vector<int> >(first_ptr)
        );
        for (int i = 0; i < 100'000; ++i) {
            ptrs.push_back(ptrs.back());
            ptrs.push_back(shared_ptr<vector<int> >(ptrs.back()));
        }
        assert(first_ptr.use_count() == 1 + 10 + 200'000);
    }

    first_ptr.reset(new vector<int>());
    second_ptr.reset();
    shared_ptr<vector<int> >().swap(first_ptr);

    assert(second_ptr.get() == nullptr);
    assert(second_ptr.get() == nullptr);

    for (int k = 0; k < 2; ++k) {
        vector<shared_ptr<int> > ptrs;
        for (int i = 0; i < 100'000; ++i) {
            int* p = new int(rand() % 99'999);
            ptrs.push_back(shared_ptr<int>(p));
        }
        std::sort(ptrs.begin(),
                  ptrs.end(),
                  [](auto&& x, auto&& y) {
                      return *x < *y;
                  });
        for (int i = 0; i + 1 < 100'000; ++i) {
            assert(*(ptrs[i]) <= *(ptrs[i + 1]));
        }
        while (!ptrs.empty()) {
            ptrs.pop_back();
        }
    }

    // test const
    {
        const shared_ptr<int> sp(new int(42));
        assert(sp.use_count() == 1);
        assert(*sp.get() == 42);
        assert(*sp == 42);
    }
}

struct Node;

struct Next {
    shared_ptr<Node> shared;
    weak_ptr<Node> weak;

    Next(const shared_ptr<Node>& shared) : shared(shared) {
    }

    Next(const weak_ptr<Node>& weak) : weak(weak) {
    }
};

struct Node {
    static int constructed;
    static int destructed;

    int value;
    Next next;

    Node(int value) : value(value), next(shared_ptr<Node>()) { ++constructed; }

    Node(int value, const shared_ptr<Node>& next) : value(value), next(next) {
        ++constructed;
    }

    Node(int value, const weak_ptr<Node>& next) : value(value), next(next) {
        ++constructed;
    }

    ~Node() { ++destructed; }
};

int Node::constructed = 0;
int Node::destructed = 0;

shared_ptr<Node> getCyclePtr(int cycleSize) {
    shared_ptr<Node> head(new Node(0));
    shared_ptr<Node> prev(head);
    for (int i = 1; i < cycleSize; ++i) {
        shared_ptr<Node> current(new Node(i));
        prev->next.shared = current;
        prev = current;
        // std::cout << prev.use_count() << '\n';
    }
    // prev->next.shared.~shared_ptr<Node>();
    // new (&prev->next.weak) weak_ptr<Node>(head);
    prev->next.weak = head;
    // prev->next.isLast = true;
    return head;
}

void test_weak_ptr() {
    auto sp = shared_ptr<int>(new int(23));
    weak_ptr<int> weak = sp;
    {
        auto shared = shared_ptr<int>(new int(42));
        weak = shared;
        assert(weak.use_count() == 1);
        assert(!weak.expired());
    }
    assert(weak.use_count() == 0);
    assert(weak.expired());

    weak = sp;
    auto wp = weak;
    assert(weak.use_count() == 1);
    assert(wp.use_count() == 1);
    auto wwp = std::move(weak);
    // assert(weak.use_count() == 0);
    assert(wwp.use_count() == 1);

    auto ssp = wwp.lock();
    assert(sp.use_count() == 2);

    sp = ssp;
    ssp = sp;
    assert(ssp.use_count() == 2);

    sp = std::move(ssp);
    assert(sp.use_count() == 1);

    ssp.reset(); // nothing should happen
    sp.reset();

    unsigned int useless_value = 0;
    for (int i = 0; i < 100'000; ++i) {
        shared_ptr<Node> head = getCyclePtr(8);
        shared_ptr<Node> nextHead = head->next.shared;
        assert(nextHead.use_count() == 2);
        useless_value += 19'937 * i * nextHead.use_count();

        head.reset();
        assert(nextHead.use_count() == 1);
    }
    std::ignore = useless_value;

    assert(Node::constructed == 800'000);
    assert(Node::destructed == 800'000);

    // test inheritance
    {
        shared_ptr<Derived> dsp(new Derived());

        shared_ptr<Base> bsp = dsp;

        weak_ptr<Derived> wdsp = dsp;
        weak_ptr<Base> wbsp = dsp;
        weak_ptr<Base> wwbsp = wdsp;

        assert(dsp.use_count() == 2);

        bsp = std::move(dsp);
        assert(bsp.use_count() == 1);

        bsp.reset();
        assert(wdsp.expired());
        assert(wbsp.expired());
        assert(wwbsp.expired());
    }

    // test const
    {
        shared_ptr<int> sp(new int(42));
        const weak_ptr<int> wp(sp);
        assert(!wp.expired());
        auto ssp = wp.lock();
    }
}

struct NeitherDefaultNorCopyConstructible {
    NeitherDefaultNorCopyConstructible() = delete;

    NeitherDefaultNorCopyConstructible(
        const NeitherDefaultNorCopyConstructible&) = delete;

    NeitherDefaultNorCopyConstructible&
    operator=(const NeitherDefaultNorCopyConstructible&) = delete;

    NeitherDefaultNorCopyConstructible(NeitherDefaultNorCopyConstructible&&)
    = default;

    NeitherDefaultNorCopyConstructible&
    operator=(NeitherDefaultNorCopyConstructible&&) = default;

    explicit NeitherDefaultNorCopyConstructible(int x) : x(x) {
    }

    int x;
};

struct Accountant {
    static int constructed;
    static int destructed;

    Accountant() { ++constructed; }

    Accountant(const Accountant&) { ++constructed; }

    ~Accountant() { ++destructed; }
};

int Accountant::constructed = 0;
int Accountant::destructed = 0;

int allocated = 0;
int deallocated = 0;

int allocate_called = 0;
int deallocate_called = 0;

int new_called = 0;
int delete_called = 0;

int construct_called = 0;
int destroy_called = 0;

void* operator new(size_t n) {
    ++new_called;
    return std::malloc(n);
}

void operator delete(void* ptr) noexcept {
    ++delete_called;
    std::free(ptr);
}

struct VerySpecialType {
};

void* operator new(size_t n, VerySpecialType) { return std::malloc(n); }

void operator delete(void* ptr, VerySpecialType) { std::free(ptr); }

// to prevent compiler warnings
void operator delete(void* ptr, size_t) noexcept {
    ++delete_called;
    std::free(ptr);
}

template <typename T>
struct MyAllocator {
    using value_type = T;

    MyAllocator() = default;

    template <typename U>
    MyAllocator(const MyAllocator<U>&) {
    }

    T* allocate(size_t n) {
        ++allocate_called;
        allocated += n * sizeof(T);;
        return (T*)::operator new(n * sizeof(T), VerySpecialType());
    }

    void deallocate(T* p, size_t n) {
        ++deallocate_called;
        deallocated += n * sizeof(T);
        ::operator delete((void*)p, VerySpecialType());
    }

    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        ++construct_called;
        ::new((void*)ptr) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* ptr) {
        ++destroy_called;
        ptr->~U();
    }
};

void test_make_allocate_shared() {
    new_called = 0;
    delete_called = 0;

    {
        auto sp = make_shared<NeitherDefaultNorCopyConstructible>(
            NeitherDefaultNorCopyConstructible(0)
        );
        weak_ptr<NeitherDefaultNorCopyConstructible> wp = sp;
        auto ssp = sp;
        sp.reset();
        assert(!wp.expired());
        ssp.reset();
        assert(wp.expired());
    }

    {
        auto sp = make_shared<Accountant>();
        assert(Accountant::constructed == 1);

        weak_ptr<Accountant> wp = sp;
        auto ssp = sp;
        sp.reset();
        assert(Accountant::constructed == 1);
        assert(Accountant::destructed == 0);

        assert(!wp.expired());
        ssp.reset();
        assert(Accountant::destructed == 1);

        Accountant::constructed = 0;
        Accountant::destructed = 0;
    }

    assert(new_called == 2);
    assert(delete_called == 2);

    new_called = 0;
    delete_called = 0;

    {
        MyAllocator<NeitherDefaultNorCopyConstructible> alloc;
        auto sp = allocate_shared<NeitherDefaultNorCopyConstructible>(
            alloc,
            NeitherDefaultNorCopyConstructible(0)
        );
        int count = allocated;
        assert(allocated > 0);
        assert(allocate_called == 1);

        weak_ptr<NeitherDefaultNorCopyConstructible> wp = sp;
        auto ssp = sp;
        sp.reset();
        assert(count == allocated);
        assert(deallocated == 0);

        assert(!wp.expired());
        ssp.reset();
        assert(count == allocated);
    }

    assert(allocated == deallocated);

    assert(allocate_called == 1);
    assert(deallocate_called == 1);
    assert(construct_called == 1);
    assert(destroy_called == 1);

    allocated = 0;
    deallocated = 0;
    allocate_called = 0;
    deallocate_called = 0;
    construct_called = 0;
    destroy_called = 0;

    {
        MyAllocator<Accountant> alloc;
        auto sp = allocate_shared<Accountant>(alloc);
        int count = allocated;
        assert(allocated > 0);
        assert(allocate_called == 1);
        assert(Accountant::constructed == 1);

        weak_ptr<Accountant> wp = sp;
        auto ssp = sp;
        sp.reset();
        assert(count == allocated);
        assert(deallocated == 0);
        assert(Accountant::constructed == 1);
        assert(Accountant::destructed == 0);

        assert(!wp.expired());
        ssp.reset();
        assert(count == allocated);
    }

    assert(allocated == deallocated);

    assert(Accountant::constructed == 1);
    assert(Accountant::destructed == 1);

    assert(allocate_called == 1);
    assert(deallocate_called == 1);
    assert(construct_called == 1);
    assert(destroy_called == 1);

    assert(new_called == 0);
    assert(delete_called == 0);

    allocated = 0;
    deallocated = 0;
    allocate_called = 0;
    deallocate_called = 0;
    construct_called = 0;
    destroy_called = 0;
}

struct Enabled : public enable_shared_from_this<Enabled> {
    shared_ptr<Enabled> get_shared() {
        return shared_from_this();
    }
};

void test_enable_shared_from_this() {
    {
        Enabled e;
        bool caught = false;
        try {
            e.get_shared();
        } catch (...) {
            caught = true;
        }
        assert(caught);
    }

    auto esp = make_shared<Enabled>();

    auto& e = *esp;
    auto sp = e.get_shared();

    assert(sp.use_count() == 2);

    esp.reset();
    assert(sp.use_count() == 1);

    sp.reset();
}

int mother_created = 0;
int mother_destroyed = 0;
int son_created = 0;
int son_destroyed = 0;

struct Mother {
    Mother() { ++mother_created; }

    virtual ~Mother() { ++mother_destroyed; }
};

struct Son : public Mother {
    Son() { ++son_created; }

    virtual ~Son() { ++son_destroyed; }
};

void test_inheritance_destroy() {
    {
        shared_ptr<Son> sp(new Son());

        shared_ptr<Mother> mp(new Mother());

        mp = sp;

        sp.reset(new Son());
    }
    assert(son_created == 2);
    assert(son_destroyed == 2);
    assert(mother_created == 3);
    assert(mother_destroyed == 3);

    mother_created = 0;
    mother_destroyed = 0;
    son_created = 0;
    son_destroyed = 0;

    {
        MyAllocator<Son> alloc;
        auto sp = allocate_shared<Son>(alloc);

        shared_ptr<Mother> mp = sp;

        sp.reset(new Son());
    }
    assert(son_created == 2);
    assert(son_destroyed == 2);
    assert(mother_created == 2);
    assert(mother_destroyed == 2);

    assert(allocated == deallocated);
    assert(allocate_called == 1);
    assert(deallocate_called == 1);
    assert(construct_called == 1);
    assert(destroy_called == 1);

    allocated = 0;
    deallocated = 0;
    allocate_called = 0;
    deallocate_called = 0;
    construct_called = 0;
    destroy_called = 0;
}

int custom_deleter_called = 0;

struct MyDeleter {
    template <typename T>
    void operator()(T*) {
        ++custom_deleter_called;
    }
};

void test_custom_deleter() {
    MyDeleter deleter;
    int x = 0;

    new_called = 0;
    delete_called = 0;

    {
        shared_ptr<int> sp(&x, deleter);

        auto ssp = std::move(sp);

        auto sssp = ssp;

        ssp = make_shared<int>(5);
    }
    assert(custom_deleter_called == 1);

    // 1 for ControlBlock in sp and 1 for make_shared
    assert(new_called == 2);
    assert(delete_called == 2);

    new_called = 0;
    delete_called = 0;
    allocated = 0;
    deallocated = 0;
    allocate_called = 0;
    deallocate_called = 0;
    construct_called = 0;
    destroy_called = 0;
    custom_deleter_called = 0;

    Accountant::constructed = 0;
    Accountant::destructed = 0;

    Accountant acc;
    {
        MyAllocator<Accountant> alloc;
        MyDeleter deleter;

        shared_ptr<Accountant> sp(&acc, deleter, alloc);

        auto ssp = std::move(sp);

        auto sssp = ssp;

        ssp = make_shared<Accountant>();
    }

    assert(new_called == 1); // for make_shared
    assert(delete_called == 1);
    assert(allocate_called == 1);
    assert(deallocate_called == 1);
    assert(allocated == deallocated);

    assert(Accountant::constructed == 2);
    assert(Accountant::destructed == 1);

    assert(construct_called == 0);
    assert(destroy_called == 0);
    assert(custom_deleter_called == 1);
}

int main() {
    // static_assert(!std::is_base_of_v<std::shared_ptr<VerySpecialType>,
    // shared_ptr<VerySpecialType>>,
    //         "don't try to use std smart pointers");

    // static_assert(!std::is_base_of_v<std::weak_ptr<VerySpecialType>,
    // weak_ptr<VerySpecialType>>,
    //         "don't try to use std smart pointers");

    std::cerr << "Starting tests..." << std::endl;

    test_shared_ptr();
    std::cerr << "Test 1 (shared ptr) passed." << std::endl;

    test_weak_ptr();
    std::cerr << "Test 2 (weak ptr) passed." << std::endl;

    test_make_allocate_shared();
    std::cerr << "Test 3 (make/allocate shared) passed." << std::endl;

    test_enable_shared_from_this();
    std::cerr << "Test 4 (enable_shared_from_this) passed." << std::endl;

    test_inheritance_destroy();
    std::cerr << "Test 5 (inheritance) passed." << std::endl;

    test_custom_deleter();
    std::cerr << "Test 6 (custom deleter) passed." << std::endl;
}