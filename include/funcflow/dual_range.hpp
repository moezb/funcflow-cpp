#pragma once
/**
 * @file dual_range.hpp
 * @brief Implements a dual range struct for holding mutable and non-mutable
 * iterators.
 */
#include "funcflow/context_view_range.hpp"

namespace funcflow {
namespace context_view {
/**
 * @brief Dual range struct that provides both mutable and non-mutable iterator 
 *
 * This struct type aims to simplify some complex calls involving visitors
 * that need to iterate a context view with a constant access but also need mutable access
 * to merge their (the calls) modifications to the view.
 * @tparam TConstRange The const context view range type (used for type info only)
 * @tparam TMutableRange The mutable context view range type (used for actual iteration)
 */
template <ConstContextViewRange TConstRange, AnyContextViewRange TMutableRange>
struct dual_context_view_range {
  using const_range   = TConstRange;
  using mutable_range = TMutableRange;
  using context       = typename TMutableRange::context;
  using view          = typename TMutableRange::view;
  using view_getter   = typename TMutableRange::view_getter;

  explicit dual_context_view_range(context& ctx) : context_(ctx) {
  }

  // Get mutable range - const control enforced at usage level
  mutable_range get_mutable_range() {
    return mutable_range(context_);
  }

  // Convenience methods
  size_t size() const {
    return mutable_range(context_).size();
  }

private:
  context& context_;
};

/**
 * @brief Concept to check if a type is a dual context view range.
 */
template <typename T>
concept DualContextViewRange = requires {
                                 typename T::const_range;
                                 typename T::mutable_range;
                                 typename T::context;
                                 typename T::view;
                                 typename T::view_getter;
                               } && requires(T& dual_range) {
                                 dual_range.get_mutable_range();
                                 dual_range.size();
                               } && ConstContextViewRange<typename T::const_range>
                               && AnyContextViewRange<typename T::mutable_range>;
} // namespace context_view
} // namespace funcflow
