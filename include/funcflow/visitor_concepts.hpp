#pragma once

/**
 * @file visitor_concepts.hpp
 * @brief Defines concepts for various visitor patterns.
 */

#include <functional>
#include <variant>
#include <vector>
#include "funcflow/context_view_concepts.hpp"

namespace funcflow {
namespace context_view {

// ============================================================================
// Collector Concepts - Simplified visitor pattern for collection operations
// ============================================================================
//
// A collector is a functor that:
// - Accepts a read-only view
// - Returns a vector of movable results
// - Is default constructible
//
// Collectors are simpler than visitors because they always return vectors,
// eliminating the need for variant handling. The merge callback receives
// all collected vectors and combines them into a single result.

// Concept for a collector that accepts a const view and returns a vector of results
template <typename T, typename TView, typename TResult>
concept Collector = MovableResult<TResult> && requires(T collector, const TView& view) {
  { collector(view) } -> std::same_as<std::vector<TResult>>;
  T{};
};

// Define a type alias for a callback that receives the flattened collected results
// All vectors from all collectors are flattened into a single vector
template <typename TView, typename TResult>
using collector_merge_callback_t = std::function<void(TView&, const std::vector<TResult>&)>;

} // namespace context_view
} // namespace funcflow