#include "catch/catch.hpp"
#include "engine/core/memory_pool.h"

namespace {

struct CountingPoolObject {
    static int ctor_count;
    static int dtor_count;
    int value = 7;

    CountingPoolObject() {
        ++ctor_count;
    }

    ~CountingPoolObject() {
        ++dtor_count;
    }
};

int CountingPoolObject::ctor_count = 0;
int CountingPoolObject::dtor_count = 0;

struct CountingReset {
    CountingReset() {
        CountingPoolObject::ctor_count = 0;
        CountingPoolObject::dtor_count = 0;
    }

    ~CountingReset() {
        CountingPoolObject::ctor_count = 0;
        CountingPoolObject::dtor_count = 0;
    }
};

} // namespace

TEST_CASE("Given_MemoryPool_When_AllocateAndFree_Then_ConstructAndDestructExactlyOnce", "[engine][unit][memory_pool]") {
    CountingReset reset;
    dse::core::MemoryPool<CountingPoolObject> pool(1);

    auto* object = pool.Allocate();
    REQUIRE(object != nullptr);
    REQUIRE(object->value == 7);
    REQUIRE(CountingPoolObject::ctor_count == 1);
    REQUIRE(CountingPoolObject::dtor_count == 0);

    pool.Free(object);
    REQUIRE(CountingPoolObject::dtor_count == 1);
}

TEST_CASE("Given_MemoryPool_When_FreedPointerReallocated_Then_SameStorageCanBeReused", "[engine][unit][memory_pool]") {
    CountingReset reset;
    dse::core::MemoryPool<CountingPoolObject> pool(1);

    auto* first = pool.Allocate();
    REQUIRE(first != nullptr);
    first->value = 42;
    pool.Free(first);

    auto* second = pool.Allocate();
    REQUIRE(second != nullptr);
    REQUIRE(second == first);
    REQUIRE(second->value == 7);
    REQUIRE(CountingPoolObject::ctor_count == 2);
    REQUIRE(CountingPoolObject::dtor_count == 1);

    pool.Free(second);
    REQUIRE(CountingPoolObject::dtor_count == 2);
}

TEST_CASE("Given_MemoryPool_When_CapacityExceeded_Then_ExpandAndKeepAllocating", "[engine][unit][memory_pool]") {
    CountingReset reset;
    dse::core::MemoryPool<CountingPoolObject> pool(1);

    auto* first = pool.Allocate();
    auto* second = pool.Allocate();

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first != second);
    REQUIRE(CountingPoolObject::ctor_count == 2);

    pool.Free(first);
    pool.Free(second);
    REQUIRE(CountingPoolObject::dtor_count == 2);
}

TEST_CASE("Given_MemoryPool_When_FreeNull_Then_NoCrashAndCountersUnchanged", "[engine][unit][memory_pool]") {
    CountingReset reset;
    dse::core::MemoryPool<CountingPoolObject> pool(1);

    pool.Free(nullptr);

    REQUIRE(CountingPoolObject::ctor_count == 0);
    REQUIRE(CountingPoolObject::dtor_count == 0);
}
