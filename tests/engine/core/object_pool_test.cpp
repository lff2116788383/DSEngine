#include "catch/catch.hpp"
#include "engine/core/object_pool.h"
#include <string>

TEST_CASE("Given_ObjectPoolWithReserve_When_AcquireCalled_Then_PreallocatedObjectsAreConsumed", "[engine][unit][object_pool]") {
    dse::core::ObjectPool<int> pool(3, []() { return 123; });

    REQUIRE(pool.AvailableCount() == 3);
    REQUIRE(pool.Acquire() == 123);
    REQUIRE(pool.AvailableCount() == 2);
    REQUIRE(pool.Acquire() == 123);
    REQUIRE(pool.AvailableCount() == 1);
}

TEST_CASE("Given_ObjectPoolWithoutFreeItems_When_AcquireCalled_Then_FactoryCreatesNewValue", "[engine][unit][object_pool]") {
    int next_value = 1;
    dse::core::ObjectPool<int> pool(0, [&]() { return next_value++; });

    REQUIRE(pool.AvailableCount() == 0);
    REQUIRE(pool.Acquire() == 1);
    REQUIRE(pool.Acquire() == 2);
    REQUIRE(pool.AvailableCount() == 0);
}

TEST_CASE("Given_ObjectPool_When_ReleaseCalled_Then_ValueReturnsToPool", "[engine][unit][object_pool]") {
    dse::core::ObjectPool<std::string> pool;

    pool.Release("alpha");
    pool.Release("beta");

    REQUIRE(pool.AvailableCount() == 2);
    REQUIRE(pool.Acquire() == "beta");
    REQUIRE(pool.AvailableCount() == 1);
    REQUIRE(pool.Acquire() == "alpha");
    REQUIRE(pool.AvailableCount() == 0);
}

TEST_CASE("Given_ObjectPool_When_ReserveCalledTwice_Then_DoesNotShrinkOrDuplicateUnexpectedly", "[engine][unit][object_pool]") {
    dse::core::ObjectPool<int> pool(2, []() { return 9; });

    REQUIRE(pool.AvailableCount() == 2);
    pool.Reserve(1);
    REQUIRE(pool.AvailableCount() == 2);
    pool.Reserve(5);
    REQUIRE(pool.AvailableCount() == 5);
}
