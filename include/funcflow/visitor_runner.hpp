#pragma once

/**
 * @file visitor_runner.hpp
 * @brief Implements the visitor runner infrastructure.
 */

#include <variant>
#include <vector>
#include <functional>
#include <type_traits>
#include "funcflow/visitor_concepts.hpp"
#include "funcflow/scheduler.hpp"
#include "funcflow/task_utils.hpp"

namespace funcflow {
namespace context_view {
// ============================================================================
// Collector Runner - Simplified runner for collection operations
// ============================================================================
//
// This runner executes collectors (functors that return vectors) and provides
// a cleaner interface than the visitor runner by avoiding variant handling.
// All collectors return vectors, and the merge callback receives all vectors
// flattened into a single vector.
//
// Supports both sequential and parallel execution based on ExecPolicy.

template <typename TView,
          MovableResult TResult,
          typename... TCollectors>
  requires (Collector<TCollectors, TView, TResult> && ...)
struct view_collector_runner {
  using collector_result_t = std::vector<TResult>;
  scheduler::execution_mode collection_execution_mode{scheduler::execution_mode::sequential};

  // Constructor to allow initialization with execution mode
  view_collector_runner() = default;

  explicit view_collector_runner(scheduler::execution_mode mode)
  : collection_execution_mode(mode) {
  }

private:
  // Execute all collectors and gather their vector results
  // Uses scheduler::run_functors_typed for parallel/sequential execution
  std::vector<collector_result_t> run_collectors(const TView& view, scheduler::StepResult& parent_result) {
    // Initialize parent_result before running functors
    parent_result.step_name = "Run collectors";

    // Use scheduler to run collectors in parallel or sequential based on ExecPolicy
    std::vector<collector_result_t> all_results =
    scheduler::run_functors_typed<TView, collector_result_t, TCollectors...>(collection_execution_mode, view,
                                                                             parent_result);

    return all_results;
  }

public:
  // Pure collector execution without merge callback
  std::vector<collector_result_t> run_collectors_only(const TView& view, scheduler::StepResult& parent_result) {
    return run_collectors(view, parent_result);
  }

  // Execute collectors and invoke merge callback with flattened results
  template <typename MergeCallback>
    requires (
      std::is_same_v<std::decay_t<MergeCallback>, std::nullptr_t> ||
      std::invocable<MergeCallback, TView&, const std::vector<TResult>&>
    )
  bool operator()(TView& view, MergeCallback&& cb, scheduler::StepResult& parent_result) {
    std::vector<collector_result_t> results = run_collectors(view, parent_result);

    if constexpr (!std::is_same_v<std::decay_t<MergeCallback>, std::nullptr_t>) {
      auto& callback_result     = parent_result.sub_steps.emplace_back();
      callback_result.step_name = "collector merge callback";

      try {
        // Flatten all collected vectors into a single vector
        std::vector<TResult> flattened_results;

        // Calculate total size for efficient allocation
        size_t total_size = 0;
        for (const auto& vec : results) {
          total_size += vec.size();
        }
        flattened_results.reserve(total_size);

        // Flatten all vectors
        for (auto& vec : results) {
          flattened_results.insert(
            flattened_results.end(),
            std::make_move_iterator(vec.begin()),
            std::make_move_iterator(vec.end())
          );
        }

        // Invoke the callback with the flattened vector
        std::invoke(std::forward<MergeCallback>(cb), view, flattened_results);
      }
      catch (...) {
        callback_result  = false;
        parent_result    = false;        
        STORE_EXCEPTION(callback_result.exception);
      }
    }
    return parent_result.success();
  }
};
} // namespace context_view
} // namespace funcflow
