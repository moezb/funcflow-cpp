#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include <string>
#include <cstdint>
#include <funcflow/utils/flags.hpp>

// Test enum with bit flag values
enum class TestFlags : uint32_t {
  None  = 0,
  Flag1 = 1 << 0, // 1
  Flag2 = 1 << 1, // 2
  Flag3 = 1 << 2, // 4
  Flag4 = 1 << 3, // 8
  Flag5 = 1 << 4  // 16
};

DECLARE_MAX_FLAGS_VALUE(TestFlags, TestFlags::Flag5);

// Specialize enum_to_string for TestFlags
namespace funcflow::utils {
template <>
inline ::std::string enum_to_string<TestFlags>(TestFlags flag) {
  switch (flag) {
  case TestFlags::None:
    return "None";
  case TestFlags::Flag1:
    return "Flag1";
  case TestFlags::Flag2:
    return "Flag2";
  case TestFlags::Flag3:
    return "Flag3";
  case TestFlags::Flag4:
    return "Flag4";
  case TestFlags::Flag5:
    return "Flag5";
  default:
    return "Unknown";
  }
}
}

// Additional test enums for edge cases
enum class SingleFlag : uint8_t {
  None = 0,
  OnlyOne = 1 << 0
};

DECLARE_MAX_FLAGS_VALUE(SingleFlag, SingleFlag::OnlyOne);

enum class ManyFlags : uint64_t {
  None  = 0,
  Flag1 = 1ULL << 0,
  Flag2 = 1ULL << 1,
  Flag3 = 1ULL << 2,
  Flag4 = 1ULL << 3,
  Flag5 = 1ULL << 4,
  Flag6 = 1ULL << 5,
  Flag7 = 1ULL << 6,
  Flag8 = 1ULL << 7
};

DECLARE_MAX_FLAGS_VALUE(ManyFlags, ManyFlags::Flag8);

TEST_CASE("Flags - Construction", "[flags]") {
  SECTION("Default constructor creates empty flags") {
    Flags<TestFlags> flags;
    REQUIRE(flags == TestFlags::None);
    REQUIRE_FALSE(static_cast<bool>(flags));
  }

  SECTION("Construct from single enum value") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    REQUIRE(flags == TestFlags::Flag1);
    REQUIRE(static_cast<bool>(flags));
  }

  SECTION("Construct from underlying type") {
    Flags<TestFlags> flags(static_cast<uint32_t>(5)); // Flag1 | Flag3
    REQUIRE(flags == (TestFlags::Flag1 | TestFlags::Flag3));
    REQUIRE(flags.has_all_flags({TestFlags::Flag1, TestFlags::Flag3}));
    REQUIRE(flags.has_all_flags(TestFlags::Flag1 | TestFlags::Flag3));
  }

  SECTION("Copy construction") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2 = flags1;
    REQUIRE(flags2 == flags1);
  }
}

TEST_CASE("Flags - Bitwise OR operations", "[flags]") {
  SECTION("OR with enum value") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    auto             result = flags | TestFlags::Flag2;
    REQUIRE(result == (TestFlags::Flag1 | TestFlags::Flag2));
  }

  SECTION("OR with another Flags object") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2(TestFlags::Flag2);
    auto             result = flags1 | flags2;
    REQUIRE(result == (TestFlags::Flag1 | TestFlags::Flag2));
  }

  SECTION("OR assignment with enum") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags |= TestFlags::Flag2;
    REQUIRE(flags == (TestFlags::Flag1 | TestFlags::Flag2));
  }

  SECTION("OR assignment with Flags") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2(TestFlags::Flag2);
    flags1 |= flags2;
    REQUIRE(flags1 == (TestFlags::Flag1 | TestFlags::Flag2));
  }

  SECTION("Free function OR between enum values") {
    auto result = TestFlags::Flag1 | TestFlags::Flag2;
    REQUIRE(result == (TestFlags::Flag1 | TestFlags::Flag2));
  }
}

TEST_CASE("Flags - Bitwise AND operations", "[flags]") {
  SECTION("AND with enum value") {
    Flags<TestFlags> flags(TestFlags::Flag1 | TestFlags::Flag2); // Flag1 | Flag2
    auto             result = flags & TestFlags::Flag1;
    REQUIRE(result == TestFlags::Flag1);
  }

  SECTION("AND with another Flags object") {
    Flags<TestFlags> flags1(TestFlags::Flag1 | TestFlags::Flag2);
    Flags<TestFlags> flags2(TestFlags::Flag1 | TestFlags::Flag3);
    auto             result = flags1 & flags2;
    REQUIRE(result == TestFlags::Flag1); // Only Flag1 is common
  }

  SECTION("AND assignment with enum") {
    Flags<TestFlags> flags(TestFlags::Flag1 | TestFlags::Flag2);
    flags &= TestFlags::Flag1;
    REQUIRE(flags == TestFlags::Flag1);
  }

  SECTION("AND assignment with Flags") {
    Flags<TestFlags> flags1(TestFlags::Flag1 | TestFlags::Flag2);
    Flags<TestFlags> flags2(TestFlags::Flag1 | TestFlags::Flag3);
    flags1 &= flags2;
    REQUIRE(flags1 == TestFlags::Flag1);
  }

  SECTION("Free function AND between enum values") {
    auto result = TestFlags::Flag1 & TestFlags::Flag1;
    REQUIRE(result == TestFlags::Flag1);
  }
}

TEST_CASE("Flags - Bitwise XOR operations", "[flags]") {
  SECTION("XOR with enum value") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    auto             result = flags ^ TestFlags::Flag1;
    REQUIRE(result == TestFlags::None); // Toggle off
  }

  SECTION("XOR with another Flags object") {
    Flags<TestFlags> flags1(TestFlags::Flag1 | TestFlags::Flag2);
    Flags<TestFlags> flags2(TestFlags::Flag1 | TestFlags::Flag3);
    auto             result = flags1 ^ flags2;
    REQUIRE(result == (TestFlags::Flag2 | TestFlags::Flag3)); // Flag2 | Flag3 (Flag1 cancels out)
  }

  SECTION("XOR assignment with enum") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags ^= TestFlags::Flag1;
    REQUIRE(flags == TestFlags::None);
  }

  SECTION("XOR assignment with Flags") {
    Flags<TestFlags> flags1(TestFlags::Flag1 | TestFlags::Flag2);
    Flags<TestFlags> flags2(TestFlags::Flag1 | TestFlags::Flag3);
    flags1 ^= flags2;
    REQUIRE(flags1 == (TestFlags::Flag2 | TestFlags::Flag3)); // Flag2 | Flag3 (Flag1 cancels out)
  }

  SECTION("Free function XOR between enum values") {
    auto result = TestFlags::Flag1 ^ TestFlags::Flag1;
    REQUIRE(result == TestFlags::None);
  }
}

TEST_CASE("Flags - Bitwise NOT operation", "[flags]") {
  SECTION("NOT operator inverts all bits") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    auto             result   = ~flags;
    auto             expected = ~Flags<TestFlags>(TestFlags::Flag1);
    REQUIRE(result == expected);
  }

  SECTION("Double NOT returns original") {
    Flags<TestFlags> flags(TestFlags::Flag1 | TestFlags::Flag3);
    auto             result = ~~flags;
    REQUIRE(result == flags);
  }
}

TEST_CASE("Flags - Comparison operations", "[flags]") {
  SECTION("Equality operator") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2(TestFlags::Flag1);
    Flags<TestFlags> flags3(TestFlags::Flag2);

    REQUIRE(flags1 == flags2);
    REQUIRE_FALSE(flags1 == flags3);
  }

  SECTION("Inequality operator") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2(TestFlags::Flag2);

    REQUIRE(flags1 != flags2);
    REQUIRE_FALSE(flags1 != flags1);
  }

  SECTION("Empty flags are equal") {
    Flags<TestFlags> flags1;
    Flags<TestFlags> flags2;
    REQUIRE(flags1 == flags2);
  }
}

TEST_CASE("Flags - Flag testing", "[flags]") {
  SECTION("testFlag returns true for set flags") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2;
    REQUIRE(flags.testFlag(TestFlags::Flag1));
    REQUIRE(flags.testFlag(TestFlags::Flag2));
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag3));
  }

  SECTION("testFlag on empty flags") {
    Flags<TestFlags> flags;
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag1));
  }
}

TEST_CASE("Flags - Setting and clearing flags", "[flags]") {
  SECTION("set_flag enables a flag") {
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1);
    REQUIRE(flags.testFlag(TestFlags::Flag1));
  }

  SECTION("set_flag with false disables a flag") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags.set_flag(TestFlags::Flag1, false);
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag1));
  }

  SECTION("clearFlag disables a flag") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags.clearFlag(TestFlags::Flag1);
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag1));
  }

  SECTION("Setting multiple flags") {
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1)
         .set_flag(TestFlags::Flag2)
         .set_flag(TestFlags::Flag3);
    REQUIRE(flags.testFlag(TestFlags::Flag1));
    REQUIRE(flags.testFlag(TestFlags::Flag2));
    REQUIRE(flags.testFlag(TestFlags::Flag3));
  }
}

TEST_CASE("Flags - Toggle flag", "[flags]") {
  SECTION("toggle_flag switches flag state") {
    Flags<TestFlags> flags;
    flags.toggle_flag(TestFlags::Flag1);
    REQUIRE(flags.testFlag(TestFlags::Flag1));
    flags.toggle_flag(TestFlags::Flag1);
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag1));
  }

  SECTION("Toggle multiple flags") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags.toggle_flag(TestFlags::Flag1)
         .toggle_flag(TestFlags::Flag2);
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag1));
    REQUIRE(flags.testFlag(TestFlags::Flag2));
  }
}

TEST_CASE("Flags - has_any_flag", "[flags]") {
  SECTION("Returns true when any flag matches") {
    Flags<TestFlags> flags1 = TestFlags::Flag1 | TestFlags::Flag2;
    Flags<TestFlags> flags2 = TestFlags::Flag2 | TestFlags::Flag3;
    REQUIRE(flags1.has_any_flag(flags2)); // Flag2 is common
  }

  SECTION("Returns false when no flags match") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2(TestFlags::Flag2);
    REQUIRE_FALSE(flags1.has_any_flag(flags2));
  }

  SECTION("Returns false for empty flags") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2;
    REQUIRE_FALSE(flags1.has_any_flag(flags2));
  }
}

TEST_CASE("Flags - has_all_flags with Flags parameter", "[flags]") {
  SECTION("Returns true when all flags are present") {
    Flags<TestFlags> flags1 = TestFlags::Flag1 | TestFlags::Flag2 | TestFlags::Flag3;
    Flags<TestFlags> flags2 = TestFlags::Flag1 | TestFlags::Flag2;
    REQUIRE(flags1.has_all_flags(flags2));
  }

  SECTION("Returns false when some flags are missing") {
    Flags<TestFlags> flags1 = TestFlags::Flag1 | TestFlags::Flag2;
    Flags<TestFlags> flags2 = TestFlags::Flag1 | TestFlags::Flag3;
    REQUIRE_FALSE(flags1.has_all_flags(flags2));
  }

  SECTION("Returns true for empty flags check") {
    Flags<TestFlags> flags1(TestFlags::Flag1);
    Flags<TestFlags> flags2;
    REQUIRE(flags1.has_all_flags(flags2));
  }
}

TEST_CASE("Flags - has_all_flags with initializer_list", "[flags]") {
  SECTION("Returns true when all flags in list are present") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 | TestFlags::Flag3;
    REQUIRE(flags.has_all_flags({TestFlags::Flag1, TestFlags::Flag2}));
  }

  SECTION("Returns false when any flag in list is missing") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2;
    REQUIRE_FALSE(flags.has_all_flags({TestFlags::Flag1, TestFlags::Flag3}));
  }

  SECTION("Returns true for empty list") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    REQUIRE(flags.has_all_flags({}));
  }

  SECTION("Single flag check") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    REQUIRE(flags.has_all_flags({TestFlags::Flag1}));
    REQUIRE_FALSE(flags.has_all_flags({TestFlags::Flag2}));
  }
}

TEST_CASE("Flags - clear", "[flags]") {
  SECTION("Clear resets all flags to zero") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 | TestFlags::Flag3;
    flags.clear();
    REQUIRE(flags == TestFlags::None);
    REQUIRE_FALSE(static_cast<bool>(flags));
  }

  SECTION("Clear on already empty flags") {
    Flags<TestFlags> flags;
    flags.clear();
    REQUIRE(flags == TestFlags::None);
  }
}

TEST_CASE("Flags - value accessor", "[flags]") {
  SECTION("value() returns underlying type") {
    Flags<TestFlags> flags(static_cast<uint32_t>(42));
    REQUIRE(flags.value() == 42);
  }

  SECTION("Implicit conversion to underlying type") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    uint32_t         value = flags;
    REQUIRE(value == 1);
  }
}

TEST_CASE("Flags - bool conversion", "[flags]") {
  SECTION("Empty flags convert to false") {
    Flags<TestFlags> flags;
    REQUIRE_FALSE(static_cast<bool>(flags));
  }

  SECTION("Non-empty flags convert to true") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    REQUIRE(static_cast<bool>(flags));
  }

  SECTION("Can be used in conditional") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    bool             tested = false;
    if (flags) {
      tested = true;
    }
    REQUIRE(tested);
  }
}

TEST_CASE("Flags - to_string", "[flags]") {
  SECTION("Single flag to string") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    std::string      str = flags.to_string();
    REQUIRE(str == "Flag1");
  }

  SECTION("Multiple flags to string with default separator") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2;
    std::string      str   = flags.to_string();
    REQUIRE(str == "Flag1 | Flag2");
  }

  SECTION("Multiple flags to string with custom separator") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag3;
    std::string      str   = flags.to_string(", ");
    REQUIRE(str == "Flag1, Flag3");
  }

  SECTION("Empty flags to string") {
    Flags<TestFlags> flags;
    std::string      str = flags.to_string();
    // When value is 0 and there's a None = 0 enum value, it returns "None"
    REQUIRE(str == "None");
  }

  SECTION("All flags to string") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 |
                             TestFlags::Flag3 | TestFlags::Flag4 | TestFlags::Flag5;
    std::string str = flags.to_string();
    REQUIRE_FALSE(str.empty());
    REQUIRE(str.find("Flag1") != std::string::npos);
    REQUIRE(str.find("Flag2") != std::string::npos);
    REQUIRE(str.find("Flag3") != std::string::npos);
    REQUIRE(str.find("Flag4") != std::string::npos);
    REQUIRE(str.find("Flag5") != std::string::npos);
  }
}

TEST_CASE("Flags - Complex scenarios", "[flags]") {
  SECTION("Combine multiple operations") {
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1)
         .set_flag(TestFlags::Flag2)
         .set_flag(TestFlags::Flag3);

    flags.clearFlag(TestFlags::Flag2);
    flags.toggle_flag(TestFlags::Flag4);

    REQUIRE(flags.testFlag(TestFlags::Flag1));
    REQUIRE_FALSE(flags.testFlag(TestFlags::Flag2));
    REQUIRE(flags.testFlag(TestFlags::Flag3));
    REQUIRE(flags.testFlag(TestFlags::Flag4));
  }

  SECTION("Build flags incrementally") {
    Flags<TestFlags> flags;
    flags |= TestFlags::Flag1;
    flags |= TestFlags::Flag2;
    flags |= TestFlags::Flag3;

    REQUIRE(flags.has_all_flags({TestFlags::Flag1, TestFlags::Flag2, TestFlags::Flag3}));
  }

  SECTION("Mask operations") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 |
                             TestFlags::Flag3 | TestFlags::Flag4;
    Flags<TestFlags> mask = TestFlags::Flag1 | TestFlags::Flag2;

    auto result = flags & mask;
    REQUIRE(result.testFlag(TestFlags::Flag1));
    REQUIRE(result.testFlag(TestFlags::Flag2));
    REQUIRE_FALSE(result.testFlag(TestFlags::Flag3));
    REQUIRE_FALSE(result.testFlag(TestFlags::Flag4));
  }
}

TEST_CASE("Flags - Constexpr operations", "[flags]") {
  SECTION("Constexpr construction") {
    constexpr Flags<TestFlags> flags(TestFlags::Flag1);
    static_assert(flags.value() == 1, "Constexpr construction failed");
  }

  SECTION("Constexpr operations") {
    constexpr auto flags1 = Flags<TestFlags>(TestFlags::Flag1);
    constexpr auto flags2 = flags1 | TestFlags::Flag2;
    static_assert(flags2.value() == 3, "Constexpr OR failed");
  }

  SECTION("Constexpr comparison") {
    constexpr Flags<TestFlags> flags1(TestFlags::Flag1);
    constexpr Flags<TestFlags> flags2(TestFlags::Flag1);
    static_assert(flags1 == flags2, "Constexpr comparison failed");
  }

  SECTION("Constexpr testFlag") {
    constexpr Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2;
    static_assert(flags.testFlag(TestFlags::Flag1), "Constexpr testFlag failed");
  }
}

TEST_CASE("Flags - Edge cases", "[flags]") {
  SECTION("Maximum value") {
    Flags<TestFlags> flags(static_cast<uint32_t>(0xFFFFFFFF));
    REQUIRE(flags.value() == 0xFFFFFFFF);
  }

  SECTION("Zero value behavior") {
    Flags<TestFlags> flags(TestFlags::None);
    REQUIRE(flags == TestFlags::None);
    REQUIRE_FALSE(static_cast<bool>(flags));
  }

  SECTION("Self assignment") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    flags |= flags;
    REQUIRE(flags == TestFlags::Flag1);
  }

  SECTION("XOR with self clears flags") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2;
    flags ^= flags;
    REQUIRE(flags == TestFlags::None);
  }
}

TEST_CASE("Flags - get_set_bit_positions", "[flags]") {
  SECTION("Empty flags returns empty vector") {
    Flags<TestFlags> flags;
    auto             positions = flags.get_set_bit_positions();
    REQUIRE(positions.empty());
  }

  SECTION("Single flag returns single bit position") {
    Flags<TestFlags> flags(TestFlags::Flag1); // bit 0
    auto             positions = flags.get_set_bit_positions();
    REQUIRE(positions.size() == 1);
    REQUIRE(positions[0] == 0);
  }

  SECTION("Multiple flags return multiple bit positions") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag3 | TestFlags::Flag5;
    // Flag1 = bit 0, Flag3 = bit 2, Flag5 = bit 4
    auto positions = flags.get_set_bit_positions();
    REQUIRE(positions.size() == 3);
    REQUIRE(positions[0] == 0);
    REQUIRE(positions[1] == 2);
    REQUIRE(positions[2] == 4);
  }

  SECTION("All flags return all bit positions") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 |
                             TestFlags::Flag3 | TestFlags::Flag4 | TestFlags::Flag5;
    auto positions = flags.get_set_bit_positions();
    REQUIRE(positions.size() == 5);
    REQUIRE(positions[0] == 0);
    REQUIRE(positions[1] == 1);
    REQUIRE(positions[2] == 2);
    REQUIRE(positions[3] == 3);
    REQUIRE(positions[4] == 4);
  }

  SECTION("Bit positions are in ascending order") {
    Flags<TestFlags> flags     = TestFlags::Flag5 | TestFlags::Flag2 | TestFlags::Flag4;
    auto             positions = flags.get_set_bit_positions();
    REQUIRE(positions.size() == 3);
    // Should be sorted: bit 1, bit 3, bit 4
    REQUIRE(positions[0] == 1);
    REQUIRE(positions[1] == 3);
    REQUIRE(positions[2] == 4);
  }
}

TEST_CASE("Flags - get_set_flags", "[flags]") {
  SECTION("Empty flags returns empty vector") {
    Flags<TestFlags> flags;
    auto             result = flags.get_set_flags();
    REQUIRE(result.empty());
  }

  SECTION("Single flag returns single enum value") {
    Flags<TestFlags> flags(TestFlags::Flag1);
    auto             result = flags.get_set_flags();
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == TestFlags::Flag1);
  }

  SECTION("Multiple flags return multiple enum values") {
    Flags<TestFlags> flags  = TestFlags::Flag1 | TestFlags::Flag3 | TestFlags::Flag5;
    auto             result = flags.get_set_flags();
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == TestFlags::Flag1);
    REQUIRE(result[1] == TestFlags::Flag3);
    REQUIRE(result[2] == TestFlags::Flag5);
  }

  SECTION("All flags return all enum values") {
    Flags<TestFlags> flags = TestFlags::Flag1 | TestFlags::Flag2 |
                             TestFlags::Flag3 | TestFlags::Flag4 | TestFlags::Flag5;
    auto result = flags.get_set_flags();
    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == TestFlags::Flag1);
    REQUIRE(result[1] == TestFlags::Flag2);
    REQUIRE(result[2] == TestFlags::Flag3);
    REQUIRE(result[3] == TestFlags::Flag4);
    REQUIRE(result[4] == TestFlags::Flag5);
  }

  SECTION("Enum values are in bit order") {
    Flags<TestFlags> flags  = TestFlags::Flag5 | TestFlags::Flag2 | TestFlags::Flag4;
    auto             result = flags.get_set_flags();
    REQUIRE(result.size() == 3);
    // Should be ordered by bit position: Flag2 (bit 1), Flag4 (bit 3), Flag5 (bit 4)
    REQUIRE(result[0] == TestFlags::Flag2);
    REQUIRE(result[1] == TestFlags::Flag4);
    REQUIRE(result[2] == TestFlags::Flag5);
  }

  SECTION("Can iterate over returned enum values") {
    Flags<TestFlags> flags  = TestFlags::Flag1 | TestFlags::Flag2 | TestFlags::Flag3;
    auto             result = flags.get_set_flags();

    // Verify we can use the enum values directly
    for (const auto& flag : result) {
      REQUIRE(flags.testFlag(flag));
    }
  }

  SECTION("Reconstructing flags from get_set_flags") {
    Flags<TestFlags> original    = TestFlags::Flag1 | TestFlags::Flag3 | TestFlags::Flag5;
    auto             enum_values = original.get_set_flags();

    // Reconstruct the flags from the enum values
    Flags<TestFlags> reconstructed;
    for (const auto& flag : enum_values) {
      reconstructed |= flag;
    }

    REQUIRE(reconstructed == original);
  }
}

TEST_CASE("Flags - get_set_flags and get_set_bit_positions consistency", "[flags]") {
  SECTION("Both methods return same number of elements") {
    Flags<TestFlags> flags       = TestFlags::Flag1 | TestFlags::Flag3 | TestFlags::Flag5;
    auto             positions   = flags.get_set_bit_positions();
    auto             enum_values = flags.get_set_flags();
    REQUIRE(positions.size() == enum_values.size());
  }

  SECTION("Bit positions correspond to enum values") {
    Flags<TestFlags> flags       = TestFlags::Flag2 | TestFlags::Flag4;
    auto             positions   = flags.get_set_bit_positions();
    auto             enum_values = flags.get_set_flags();

    REQUIRE(positions.size() == 2);
    REQUIRE(enum_values.size() == 2);

    // Flag2 = 1 << 1, so bit position should be 1
    REQUIRE(positions[0] == 1);
    REQUIRE(enum_values[0] == TestFlags::Flag2);

    // Flag4 = 1 << 3, so bit position should be 3
    REQUIRE(positions[1] == 3);
    REQUIRE(enum_values[1] == TestFlags::Flag4);
  }
}

TEST_CASE("Flags - get_all_flag_values", "[flags]") {
  SECTION("Returns all possible flag values up to max") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();

    // Should return all 5 flags: Flag1, Flag2, Flag3, Flag4, Flag5
    REQUIRE(all_flags.size() == 6); // Including None
    REQUIRE(all_flags[0] == TestFlags::None);
    REQUIRE(all_flags[1] == TestFlags::Flag1);
    REQUIRE(all_flags[2] == TestFlags::Flag2);
    REQUIRE(all_flags[3] == TestFlags::Flag3);
    REQUIRE(all_flags[4] == TestFlags::Flag4);
    REQUIRE(all_flags[5] == TestFlags::Flag5);
  }

  SECTION("All returned flags are power-of-2 values") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();

    for (const auto& flag : all_flags) {
      auto value = static_cast<uint32_t>(flag);
      // Check if value is a power of 2 (has only one bit set)
      if (value != 0) {
        REQUIRE((value & (value - 1)) == 0);
      }
      
    }
  }

  SECTION("Flags are in ascending order") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();

    for (size_t i = 0; i < all_flags.size() - 1; ++i) {
      auto current = static_cast<uint32_t>(all_flags[i]);
      auto next    = static_cast<uint32_t>(all_flags[i + 1]);
      REQUIRE(current < next);
    }
  }

  SECTION("Can combine all flags into a complete set") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();
    Flags<TestFlags> combined;

    for (const auto& flag : all_flags) {
      combined |= flag;
    }

    // Verify all individual flags are set
    REQUIRE(combined.testFlag(TestFlags::Flag1));
    REQUIRE(combined.testFlag(TestFlags::Flag2));
    REQUIRE(combined.testFlag(TestFlags::Flag3));
    REQUIRE(combined.testFlag(TestFlags::Flag4));
    REQUIRE(combined.testFlag(TestFlags::Flag5));
  }

  SECTION("Each returned flag corresponds to a bit position") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();

    REQUIRE(all_flags.size() == 6); // Including None
    // Flag0 = 0
    REQUIRE(static_cast<uint32_t>(all_flags[0]) == 0);
    // Flag1 = 1 << 0 = 1
    REQUIRE(static_cast<uint32_t>(all_flags[1]) == 1);
    // Flag2 = 1 << 1 = 2
    REQUIRE(static_cast<uint32_t>(all_flags[2]) == 2);
    // Flag3 = 1 << 2 = 4
    REQUIRE(static_cast<uint32_t>(all_flags[3]) == 4);
    // Flag4 = 1 << 3 = 8
    REQUIRE(static_cast<uint32_t>(all_flags[4]) == 8);
    // Flag5 = 1 << 4 = 16
    REQUIRE(static_cast<uint32_t>(all_flags[5]) == 16);
  }

  SECTION("Can use returned flags to test individual bits") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();
    Flags<TestFlags> test_flags = TestFlags::Flag2 | TestFlags::Flag4;

    int match_count = 0;
    for (const auto& flag : all_flags) {
      if (test_flags.testFlag(flag)) {
        match_count++;
      }
    }

    // Should match exactly 2 flags: Flag2 and Flag4
    REQUIRE(match_count == 2);
  }

  SECTION("Returned flags can be used to iterate over all possibilities") {
    auto all_flags = Flags<TestFlags>::get_all_flag_values();

    // Use the returned flags to build all possible combinations
    std::vector<Flags<TestFlags>> combinations;

    // Test a few combinations
    combinations.push_back(Flags<TestFlags>());  // Empty
    combinations.push_back(Flags<TestFlags>(all_flags[1]));  // Just Flag1
    combinations.push_back(Flags<TestFlags>(all_flags[1]) | all_flags[2]);  // Flag1 | Flag2

    REQUIRE(combinations[0] == TestFlags::None);
    REQUIRE(combinations[1] == TestFlags::Flag1);
    REQUIRE(combinations[2] == (TestFlags::Flag1 | TestFlags::Flag2));
  }

  SECTION("Works with different enum types") {
    // This test verifies the template nature of the function
    // by ensuring it works with the TestFlags enum type
    auto all_flags = Flags<TestFlags>::get_all_flag_values();
    REQUIRE_FALSE(all_flags.empty());
    REQUIRE(all_flags.size() == 6);  // We have 6 flags in TestFlags
  }
}

TEST_CASE("Flags - get_all_flag_values edge cases", "[flags]") {
  SECTION("Single flag enum returns one value") {
    auto all_flags = Flags<SingleFlag>::get_all_flag_values();
    REQUIRE(all_flags.size() == 2); // None + OnlyOne
    REQUIRE(all_flags[0] == SingleFlag::None);
    REQUIRE(all_flags[1] == SingleFlag::OnlyOne);
  }

  SECTION("Many flags enum returns all values") {
    auto all_flags = Flags<ManyFlags>::get_all_flag_values();
    REQUIRE(all_flags.size() == 9); // Including None

    // Verify first and last
    REQUIRE(all_flags[0] == ManyFlags::None);
    REQUIRE(all_flags[1] == ManyFlags::Flag1);
    REQUIRE(all_flags[8] == ManyFlags::Flag8);
    // Verify all are power of 2
    for (const auto& flag : all_flags) {
      auto value = static_cast<uint64_t>(flag);
      if (value != 0) {
        REQUIRE((value & (value - 1)) == 0);
      }
    }
  }
}
