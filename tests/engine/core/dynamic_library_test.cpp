#include "catch/catch.hpp"
#include "engine/core/dynamic_library.h"

using dse::core::DynamicLibrary;

TEST_CASE("Given_DefaultDynamicLibrary_When_Queried_Then_NotLoadedAndMissingSymbolIsNull", "[engine][unit][dynamic_library]") {
    DynamicLibrary library;

    REQUIRE_FALSE(library.IsLoaded());
    REQUIRE(library.GetSymbol("definitely_missing_symbol") == nullptr);
}

TEST_CASE("Given_MovedFromDynamicLibrary_When_Queried_Then_RemainsSafeAndUnloaded", "[engine][unit][dynamic_library]") {
    DynamicLibrary original;
    DynamicLibrary moved(std::move(original));

    REQUIRE_FALSE(original.IsLoaded());
    REQUIRE_FALSE(moved.IsLoaded());
    REQUIRE(original.GetSymbol("missing") == nullptr);
    REQUIRE(moved.GetSymbol("missing") == nullptr);

    DynamicLibrary reassigned;
    reassigned = std::move(moved);
    REQUIRE_FALSE(reassigned.IsLoaded());
    REQUIRE_FALSE(moved.IsLoaded());
    REQUIRE(reassigned.GetSymbol("missing") == nullptr);
}

TEST_CASE("Given_MissingLibraryPath_When_LoadCalled_Then_ReturnsFalse", "[engine][unit][dynamic_library]") {
    DynamicLibrary library;

    REQUIRE_FALSE(library.Load("definitely_missing_dse_test_library"));
    REQUIRE_FALSE(library.IsLoaded());
    REQUIRE(library.GetSymbol("missing") == nullptr);
}
