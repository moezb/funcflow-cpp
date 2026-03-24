#pragma once

#include <type_traits>
#include <string>
#include <sstream>
#include <vector>
#include <bit>

namespace funcflow::utils {

/**
 * \brief Template function to convert enum values to string.
 *
 * This function must be specialized for each enum type that needs string conversion.
 * It is used by the Flags<Enum>::to_string() method.
 *
 * \tparam Enum The enum type to convert
 * \param value The enum value to convert to string
 * \return std::string String representation of the enum value
 *
 * \example
 * template<>
 * inline std::string funcflow::utils::enum_to_string(MyFlags flag) {
 *     switch(flag) {
 *         case MyFlags::None: return "None";
 *         case MyFlags::Flag1: return "Flag1";
 *         case MyFlags::Flag2: return "Flag2";
 *         default: return "Unknown";
 *     }
 * }
 */
template<typename Enum>
std::string enum_to_string(Enum value);


// Helper trait to store max enum values, used in ::Flags::get_all_flag_values
template<typename Enum>
struct FlagsMaxValue {
    static constexpr Enum value = static_cast<Enum>(0);
};


} // namespace funcflow::utils


/**
 * \brief Type-safe wrapper for bitwise flag operations on enum types.
 *
 * This template class provides a type-safe interface for performing bitwise operations
 * on enum values used as flags. It supports all standard bitwise operations (OR, AND, XOR, NOT)
 * and provides convenient methods for testing, setting, clearing, and toggling individual flags.
 *
 * \tparam Enum The enum type to use for flags. Must be an enum or enum class.
 *
 * \note IMPORTANT ASSUMPTIONS:
 * - Most operations work with any enum values (including combination values)
 * - The methods get_set_bit_positions() and get_set_flags() ASSUME that all enum values
 *   are powers of 2 (e.g., 1, 2, 4, 8, 16, etc. or equivalently 1<<0, 1<<1, 1<<2, etc.)
 * - These enumeration methods will NOT work correctly if the enum contains combination
 *   values (e.g., Combined = Flag1 | Flag2)
 * - If your enum has combination values, do not use get_set_bit_positions() or get_set_flags()
 *
 * \example
 * enum class MyFlags : uint32_t {
 *     None = 0,
 *     Flag1 = 1 << 0,  // 1
 *     Flag2 = 1 << 1,  // 2
 *     Flag3 = 1 << 2,  // 4
 * };
 *
 * Flags<MyFlags> flags = MyFlags::Flag1 | MyFlags::Flag3;
 * if (flags.testFlag(MyFlags::Flag1)) {
 *     // Flag1 is set
 * }
 *
 * // Enumerate all set flags (requires power-of-2 enum values)
 * auto setFlags = flags.get_set_flags();  // Returns {MyFlags::Flag1, MyFlags::Flag3}
 */
template<typename Enum>
class Flags {
    static_assert(std::is_enum_v<Enum>, "Flags can only be used with enum types");

    using UnderlyingType = std::underlying_type_t<Enum>;    

private:
    UnderlyingType value_;

public:    
    constexpr Flags() noexcept : value_(0) {}
    constexpr Flags(Enum flag) noexcept : value_(static_cast<UnderlyingType>(flag)) {}
    constexpr explicit Flags(UnderlyingType value) noexcept : value_(value) {}

    constexpr operator UnderlyingType() const noexcept { return value_; }
    constexpr explicit operator bool() const noexcept { return value_ != 0; }

    constexpr Flags operator|(Enum flag) const noexcept {
        return Flags(value_ | static_cast<UnderlyingType>(flag));
    }

    constexpr Flags operator|(const Flags& other) const noexcept {
        return Flags(value_ | other.value_);
    }

    constexpr Flags operator&(Enum flag) const noexcept {
        return Flags(value_ & static_cast<UnderlyingType>(flag));
    }

    constexpr Flags operator&(const Flags& other) const noexcept {
        return Flags(value_ & other.value_);
    }

    constexpr Flags operator^(Enum flag) const noexcept {
        return Flags(value_ ^ static_cast<UnderlyingType>(flag));
    }

    constexpr Flags operator^(const Flags& other) const noexcept {
        return Flags(value_ ^ other.value_);
    }

    constexpr Flags operator~() const noexcept {
        return Flags(~value_);
    }

    constexpr Flags& operator|=(Enum flag) noexcept {
        value_ |= static_cast<UnderlyingType>(flag);
        return *this;
    }

    constexpr Flags& operator|=(const Flags& other) noexcept {
        value_ |= other.value_;
        return *this;
    }

    constexpr Flags& operator&=(Enum flag) noexcept {
        value_ &= static_cast<UnderlyingType>(flag);
        return *this;
    }

    constexpr Flags& operator&=(const Flags& other) noexcept {
        value_ &= other.value_;
        return *this;
    }

    constexpr Flags& operator^=(Enum flag) noexcept {
        value_ ^= static_cast<UnderlyingType>(flag);
        return *this;
    }

    constexpr Flags& operator^=(const Flags& other) noexcept {
        value_ ^= other.value_;
        return *this;
    }

    constexpr bool operator==(const Flags& other) const noexcept {
        return value_ == other.value_;
    }

    constexpr bool operator!=(const Flags& other) const noexcept {
        return value_ != other.value_;
    }

    constexpr bool testFlag(Enum flag) const noexcept {
        return (value_ & static_cast<UnderlyingType>(flag)) != 0;
    }

    constexpr Flags& set_flag(Enum flag, bool on = true) noexcept {
        if (on) {
            value_ |= static_cast<UnderlyingType>(flag);
        } else {
            value_ &= ~static_cast<UnderlyingType>(flag);
        }
        return *this;
    }

    constexpr Flags& clearFlag(Enum flag) noexcept {
        return set_flag(flag, false);
    }

    constexpr Flags& toggle_flag(Enum flag) noexcept {
        value_ ^= static_cast<UnderlyingType>(flag);
        return *this;
    }

    constexpr bool has_any_flag(const Flags& other) const noexcept {
        return (value_ & other.value_) != 0;
    }

    constexpr bool has_all_flags(const Flags& other) const noexcept {
        return (value_ & other.value_) == other.value_;
    }

    bool has_all_flags(std::initializer_list<Enum> list) const noexcept {
        UnderlyingType combined = 0;
        for (const auto flag : list) {
            combined |= static_cast<UnderlyingType>(flag);
        }
        return (value_ & combined) == combined;
    }

    constexpr void clear() noexcept {
        value_ = 0;
    }

    constexpr UnderlyingType value() const noexcept {
        return value_;
    }    

    /**
     * \brief Convert flags to string representation.
     *
     * Converts the current flag value to a human-readable string by enumerating all
     * set flags and converting each to its string representation. Requires a specialized
     * funcflow::utils::enum_to_string function for the specific Enum type.
     *
     * \param separator String to use between flag names (default: " | ")
     * \return std::string String representation of the flags
     *
     * \warning This method uses get_set_flags() internally, so it requires enum values
     * to be powers of 2. See get_set_flags() documentation for details.
     *
     * \note You must provide a specialization of enum_to_string for your enum type:
     * \code
     * template<>
     * inline std::string funcflow::utils::enum_to_string(MyFlags flag) {
     *     switch(flag) {
     *         case MyFlags::Flag1: return "Flag1";
     *         case MyFlags::Flag2: return "Flag2";
     *         default: return "Unknown";
     *     }
     * }
     * \endcode
     */
    std::string to_string(const std::string& separator = " | ") const {
        if (value_ == 0) {
            return funcflow::utils::enum_to_string(static_cast<Enum>(0));
        }

        auto set_flags = get_set_flags();
        if (set_flags.empty()) {
            return "";
        }

        std::ostringstream oss;
        for (size_t i = 0; i < set_flags.size(); ++i) {
            if (i > 0) {
                oss << separator;
            }
            oss << funcflow::utils::enum_to_string(set_flags[i]);
        }
        return oss.str();
    }

    /**
     * \brief Get the bit positions of all set flags.
     *
     * Scans through the flag value and returns a vector containing the bit positions
     * (0-based) of all set bits. The positions are returned in ascending order.
     *
     * \return std::vector<int> Vector of bit positions (e.g., [0, 2, 4] for bits 0, 2, and 4)
     *
     * \warning IMPORTANT: This method assumes all enum values are powers of 2 (single bits).
     * It will produce incorrect results if the enum contains combination values
     * (e.g., Combined = Flag1 | Flag2). Only use this method if you can guarantee
     * your enum values are of the form (1 << n) where n is 0, 1, 2, 3, etc.
     *
     * \example
     * enum class Flags { Flag1 = 1<<0, Flag2 = 1<<1, Flag3 = 1<<2 };
     * Flags<Flags> f = Flags::Flag1 | Flags::Flag3;
     * auto positions = f.get_set_bit_positions();  // Returns {0, 2}
     */
    std::vector<int> get_set_bit_positions() const {
        std::vector<int> positions;
        UnderlyingType temp = value_;
        
        while (temp != 0) {
            int bit_pos = std::countr_zero(temp);
            positions.push_back(bit_pos);
            temp &= temp - 1;  // Clear lowest set bit
        }
        return positions;
    }

    /**
     * \brief Get a list of all set flags as enum values.
     *
     * Scans through the flag value and returns a vector containing the individual
     * enum values for each set bit. The enum values are returned in bit order
     * (lowest bit position first).
     *
     * \return std::vector<Enum> Vector of enum values representing the set flags
     *
     * \warning IMPORTANT: This method assumes all enum values are DISTINCT powers of 2 (single bits).
     * It will produce incorrect or meaningless results if the enum contains combination
     * values (e.g., Combined = Flag1 | Flag2). Only use this method if you can guarantee
     * your enum values are of the form (1 << n) where n is 0, 1, 2, 3, etc.
     *
     * This method is useful for iterating over individual flags without using magic_enum
     * or other reflection libraries, at the cost of requiring power-of-2 enum values.
     *
     */
    std::vector<Enum> get_set_flags() const {
        std::vector<Enum> result;
        UnderlyingType temp = value_;
        int bit_pos = 0;

        while (temp != 0) {
            if (temp & 1) {
                result.push_back(static_cast<Enum>(UnderlyingType(1) << bit_pos));
            }
            temp >>= 1;
            bit_pos++;
        }

        return result;
    }    

    /**
     * \brief Get all possible individual flag values up to a maximum value.
     *
     * This static method generates a list of all possible power-of-2 flag values
     * that exist up to and including the specified maximum enum value. 
     * This is useful for iterating over all defined flags in the concerned enum.
     * In order to use this method, you must specialize the FlagsMaxValue<Enum>
     *          
     * \return std::vector<Enum> Vector of all individual flag values as enum values
     * including the zero (none) value.
     *
     * \warning This method assumes enum values are powers of 2 (single bits).
     * It enumerates all power-of-2 values up to max_value: (1<<0, 1<<1, 1<<2, ...).     
     *
     * \example
     * enum class MyFlags : uint32_t {
     *     None = 0,
     *     Flag1 = 1 << 0,  // 1
     *     Flag2 = 1 << 1,  // 2
     *     Flag3 = 1 << 2,  // 4
     *     Flag4 = 1 << 3,  // 8
     * };
     * auto all = Flags<MyFlags>::get_all_flag_values(MyFlags::Flag4);
     * // Returns {MyFlags::Flag1, MyFlags::Flag2, MyFlags::Flag3, MyFlags::Flag4}
     */
    static std::vector<Enum> get_all_flag_values() {
        // Start with zero value
        std::vector<Enum> result{static_cast<Enum>(0)};
        constexpr auto max_flags_value = ::funcflow::utils::FlagsMaxValue<Enum>::value;
        constexpr auto max_val = static_cast<UnderlyingType>(max_flags_value);
        static_assert(max_val > 0, "You must specialize max_enum for your flag type to provide max enum value.");
        
        // Find the highest bit position in max_value
        if (max_val == 0) {
            return result;
        }

        // Enumerate all power-of-2 values up to max_val
        for (int bit_pos = 0; bit_pos < static_cast<int>(sizeof(UnderlyingType) * 8); ++bit_pos) {
            UnderlyingType flag_value = UnderlyingType(1) << bit_pos;
            if (flag_value > max_val) {
                break;
            }
            result.push_back(static_cast<Enum>(flag_value));
        }

        return result;
    }

private:
    static std::string to_hex_string(UnderlyingType val) {
        std::ostringstream oss;
        oss << std::hex << val;
        return oss.str();
    }
};

template<typename Enum>
constexpr Flags<Enum> operator|(Enum lhs, Enum rhs) noexcept {
    return Flags<Enum>(lhs) | rhs;
}

template<typename Enum>
constexpr Flags<Enum> operator&(Enum lhs, Enum rhs) noexcept {
    return Flags<Enum>(lhs) & rhs;
}

template<typename Enum>
constexpr Flags<Enum> operator^(Enum lhs, Enum rhs) noexcept {
    return Flags<Enum>(lhs) ^ rhs;
}

#define DECLARE_FLAGS(FlagsType,EnumType) \
    using FlagsType = Flags<EnumType>;


#define DECLARE_OPERATORS_FOR_FLAGS(FlagsType) \
   inline constexpr FlagsType::UnderlyingType operator*(FlagsType flags) noexcept { \
        return flags.value(); \
    }

/**  
*  \brief Macro to declare the maximum enum value for a Flags<Enum> type
*  \warning This macro Must be used in global namespace
*  TODO try to use adl instead to allow dedclaration inside a namespace
*/
#define DECLARE_MAX_FLAGS_VALUE(EnumType, MaxValue) \
     template<> struct funcflow::utils::FlagsMaxValue<EnumType> { \
        static constexpr EnumType value = MaxValue; \
    }; 
