#pragma once

#include <future>
#include <type_traits>
#include <utility>
#include <iterator>
#include <vector>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include "funcflow/task_utils.hpp"
#include <numeric>
#include <list>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace funcflow {
namespace scheduler {
enum class execution_mode {
  sequential,
  parallel
};

using namespace std::string_literals;
using namespace task_utils;

/**
 * @brief Result of a step execution, can contain sub-steps and exceptions
 * This structure is in `safe` version of the scheduler primitives 
 * to report exceptions amd make tasks continue running even if some atre failing. 
 * It help defer exception handling to the end of the workflow.
 * @todo a temporary version will be replaced by a more complete one that 
 * will help attach exception handler to the workflow primitives.
 */
struct StepResult {
  
  std::vector<StepResult>           sub_steps{};
  std::string                     step_name{};
  std::exception_ptr exception{};  

  explicit StepResult()=default;
  
  void operator=(bool success) {
    success_ = success;
    executed_ = true;
  }

  bool success() const {
    return success_;
  }

  bool executed() const {
    return executed_;
  }

  operator bool() const {
    return success_;
  }

  void init_sub_steps(size_t count,const std::vector<std::string>& step_names={}) {
    sub_steps.clear();
    sub_steps.resize(count);
    for (size_t i = 0; i < step_names.size(); ++i) {
        sub_steps[i].step_name = step_names[i];    
    }
  }

void init_sub_steps(size_t count,const std::string& index_prefix) {
    std::vector<std::string> step_names;
    for (size_t i = 0; i < count; ++i) {
        step_names.push_back(index_prefix + "["s + std::to_string(i) + "]");
    }
    init_sub_steps(count, step_names);
  }

  bool apply_sub_steps_failure_policy() {
    for (auto& sub_step : sub_steps) {
        if (!sub_step) {
          *this = false;
          return false;
        }
    }
    *this = true;
    return true;
  }

  StepResult& operator[](size_t index) {
    return sub_steps[index];
  }

private:
  bool  success_{false};
  bool  executed_{false};

};


/**
 * @brief Apply function to each element in range, optionally in parallel
 */
template <std::random_access_iterator Iterator, typename Func>
auto for_each(Iterator begin, Iterator end, Func&& func, execution_mode mode = execution_mode::sequential) {
#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
    auto count = static_cast<std::ptrdiff_t>(end - begin);
#pragma omp parallel for default(none) shared(func) firstprivate(count, begin)
    for (std::ptrdiff_t i = 0; i < count; ++i) {
      std::forward<Func>(func)(*(begin + i));
    }
    return;
  }
#endif
  // fall back to sequential execution
  for (auto it = begin; it != end; ++it) {
    std::forward<Func>(func)(*it);
  }
}

/**
 * @brief Apply function to each element in range, optionally in parallel
 * This function pass also the index to the to be called function
 */
template <std::random_access_iterator Iterator, typename Func>
auto for_each_with_index(Iterator       begin, Iterator end, Func&& func,
                         execution_mode mode = execution_mode::sequential) {
  auto count = static_cast<std::ptrdiff_t>(end - begin);
#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
#pragma omp parallel for default(none) shared(func) firstprivate(count, begin)
    for (std::ptrdiff_t i = 0; i < count; ++i) {
      std::forward<Func>(func)(*(begin + i), i);
    }
    return;
  }
#endif
  // fall back to sequential execution
  for (std::ptrdiff_t i = 0; i < count; ++i) {
    std::forward<Func>(func)(*(begin + i), i);
  }
}

/**
 * @brief Transform input range to output range, optionally in parallel
 * Each thread processes different indices, writing results to corresponding output positions.
 */
template <std::random_access_iterator InputIt, std::random_access_iterator OutputIt, typename UnaryFunc>
auto transform(InputIt        first, InputIt last, OutputIt result, UnaryFunc&& op,
               execution_mode mode = execution_mode::sequential) {
#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
    auto count = static_cast<std::ptrdiff_t>(last - first);
#pragma omp parallel for default(none) shared(result, op) firstprivate(count, first)
    for (std::ptrdiff_t i = 0; i < count; ++i) {
      result[i] = std::forward<UnaryFunc>(op)(first[i]);
    }
    return;
  }
#endif
  // fall back to sequential execution
  for (; first != last; ++first, ++result) {
    *result = std::forward<UnaryFunc>(op)(*first);
  }
}

/**
 * High level utility to run multiple functors on the same argument.
 * it uses std::async to launch each functor in its own thread in case of parallel execution.
 * @tparam TArgument The type of argument passed to each functor
 * @tparam TVariantResult The variant type that can hold any functor's result
 * @tparam TFunctors Parameter pack of functor types to execute
 * @param execution Execution mode (sequential or parallel)
 * @param argument The argument to pass to each functor
 * @param parent_step
 * @return std::vector<TVariantResult> Results from each functor
 *
 * @pre Each functor must be thread-safe when executed in parallel mode
 * @pre Functors should not depend on execution order of other functors
 * @pre TVariantResult must be able to hold the result of each functor
 *
 * @note Use only for a reasonable number of functors.
 * @note Results order matches the order of TFunctors template parameters
 * @todo remove this after removing variant based visitors in favor of
 * simple collectors.
 */
template <typename TArgument, typename TVariantResult, typename... TFunctors>
auto run_functors_typed(execution_mode execution, const TArgument& argument, StepResult& parent_step) {
  // required for encapsulating functor results in std::variant and moving them
  // to the final vector
  static_assert(std::is_move_constructible_v<TVariantResult> &&
                std::is_move_assignable_v<TVariantResult>, "TVariantResult must be movable");

  // Define the variant type for the results
  // Prepare a vector to store the results
  std::vector<TVariantResult> results;
  results.reserve(sizeof...(TFunctors));  
  const auto functors_names = get_types_names<TFunctors...>("functor"s);
  parent_step.init_sub_steps(sizeof...(TFunctors), functors_names);
  if (execution == execution_mode::sequential) {
    // Iterate through the functors and execute them sequentially
    // TODO may demand that teh functor implement name() method
    size_t functor_index = 0;
    const bool dummy[] = {
      (
        [&parent_step, &functor_index]<typename T>(std::vector<TVariantResult>& results,
                                                   const TArgument&             argument) {
          auto& current_step     = parent_step[functor_index++];
          try {
            T    functor; // Default construct the functor
            auto result  = TVariantResult{functor(argument)};
            current_step = true;
            results.emplace_back(std::move(result));
          }
          catch (...) {
            current_step = false;
            STORE_EXCEPTION(current_step.exception);
          }
          return current_step.success();
        }.template operator()<TFunctors>(results, argument),
        ...)
    };
    (void)dummy;
  }
  else {
    // Create a vector to store futures for each functor's result
    std::vector<std::future<TVariantResult>> futures;
    futures.reserve(sizeof...(TFunctors));
    // Use a lambda to launch each functor in a separate thread
    auto launch_functor = [&argument](auto&& functor) -> std::future<TVariantResult>
    {
      return std::async(std::launch::async,
                        [functor, &argument]() -> TVariantResult
                        {
                            return functor(argument);
                        });
    };
    
    // Use a lambda to launch each functor in a separate thread
    // Expand the parameter pack and launch each functor
    size_t functor_index = 0;
    (futures.emplace_back(launch_functor(std::forward<TFunctors>(TFunctors{}))), ...);
    // Collect results and get exceptions from futures
    size_t index = 0;
    for (auto& future : futures) {
      auto& current_step     = parent_step[index++];
      try { 
        results.push_back(future.get());
        current_step  = true;
      }
      catch (...) {
        current_step= false;
        current_step.exception = std::current_exception();
      }
    }
  }
  parent_step.apply_sub_steps_failure_policy();
  return results;
}

 /**
 * Executes a vector of tasks either sequentially or in parallel, storing results in a vector.
 * Exceptions are caught and recorded in parent_step.sub_steps. Failed tasks are marked as std::nullopt.
 * @param tasks Vector of callable tasks accepting Args... and returning TResult.
 * @param parent_step StepResult object to track execution status and exceptions.
 * @param mode Execution mode (sequential or parallel).
 * @param args Arguments forwarded to each task.
 * @return Vector of optional task results (std::nullopt for failed tasks).
 */

template <typename TResult, typename... Args>
auto run_contained_tasks(const std::vector<task_utils::Task<TResult, Args...>>& tasks, StepResult& parent_step,
                         execution_mode                                         mode, Args&&...    args) {
  
                          parent_step.step_name =
  "Run tasks"s + (mode == execution_mode::parallel ? " in parallel"s : " sequentially"s);
  //std::atomic<bool> stop_execution{false};
  std::vector<std::optional<TResult>> results(tasks.size(), std::nullopt);
  parent_step.init_sub_steps(tasks.size(), "Task"s);  
#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
    // Capture args in a tuple for OpenMP compatibility (parameter packs can't be in OpenMP clauses)
    // Use std::make_tuple to copy/move args into the tuple to avoid reference issues
    auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
#pragma omp parallel for default(none) shared(tasks, results,parent_step, args_tuple)
    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
        auto& current_step = parent_step[i];
      try {
        results[i] = std::apply(tasks[i], args_tuple);
        current_step   = true;
      }
      catch (...) {
        current_step = false;
        STORE_EXCEPTION(current_step.exception);
      }
    }

    parent_step.apply_sub_steps_failure_policy();
    return results;
  }
#endif

  // Sequential fallback
  // Sequential execution for exception-safe Task running
  // Note: OpenMP parallel execution with exception tracking has threading issues with exception_ptr
  // so we use sequential mode for this function
  for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
      auto& current_step     = parent_step[i];      
      try {
        results[i] = tasks[i](std::forward<Args>(args)...);
        current_step = true;        
      }
      catch (...) {
        current_step  = false;
        STORE_EXCEPTION(current_step.exception);
      }
  }
  parent_step.apply_sub_steps_failure_policy();
  return results;
}
/**
 * @brief Run a vector of simple std::function tasks 
 * with no arguments and collect their results in a vector
 * @param tasks Vector of std::function tasks
 * @param mode Execution mode (sequential or parallel)
 * @return Vector of results from each task
 * @todo integrate exception reporting
 */
template <typename ResultType>
auto run_function_tasks(const std::vector<std::function<ResultType()>>& tasks,
                        execution_mode                                  mode = execution_mode::sequential) {
  std::vector<ResultType> results(tasks.size());

#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
#pragma omp parallel for default(none) shared(tasks, results)
    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
      results[i] = tasks[i]();
    }
    return results;
  }
#endif

  // Sequential fallback
  for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
    results[i] = tasks[i]();
  }

  return results;
}

/**
 * @brief Exception-safe wrapper for #for_each that collects exceptions.
 *
 * This function wraps the standard #for_each function to ensure that any exceptions
 * thrown by the provided function are caught and recorded in the StepResult.
 *
 * @tparam Iterator Random access iterator type
 * @tparam Func Function type to apply to each element
 * @param begin Iterator to the beginning of the range
 * @param end Iterator to the end of the range
 * @param func Function to apply to each element
 * @param parent_step StepResult into which sub-steps and exceptions will be recorded
 * @param mode Execution mode (sequential or parallel) 
 */
template <std::random_access_iterator Iterator, typename Func>
auto for_each_safe(Iterator       begin, Iterator end, Func&& func, StepResult& parent_step,
                   execution_mode mode = execution_mode::sequential) {

  const auto functor_name = get_type_name<Func>("Functor"s);
  const auto count = static_cast<std::ptrdiff_t>(end - begin);
  parent_step.step_name = "Apply "s + functor_name + " for each item "s +
                          (mode == execution_mode::parallel ? " in parallel"s : " sequentially"s);
  
  parent_step.init_sub_steps(count,functor_name);

  for_each_with_index(
    begin, end,
    [&func, functor_name, &parent_step](auto& item, std::ptrdiff_t idx)
    {
      auto& current_step = parent_step[idx];
      try {
        if constexpr (std::is_same_v<StepResult, std::invoke_result_t<Func, decltype(item)>>) {
          const auto saved_step_name = current_step.step_name;
          current_step = std::forward<Func>(func)(item);
          current_step.step_name = saved_step_name + " -> " + current_step.step_name;          
        }
        else {
          std::forward<Func>(func)(item);
          current_step = true;
        }
      }
      catch (...) {
        STORE_EXCEPTION(current_step.exception);
        current_step = false;
      }
    },
    mode);  
    parent_step.apply_sub_steps_failure_policy();
}

/**
 * @brief Exception-safe wrapper for #for_each_with_index that collects exceptions
 * This function wraps the standard #for_each_with_index function to ensure that any exceptions
 * thrown by the provided function are caught and recorded in the StepResult.
 * @tparam Iterator Random access iterator type
 * @tparam Func Function type that takes element and index
 * @param begin Iterator to the beginning of the range
 * @param end Iterator to the end of the range
 * @param func Function to apply to each element with its index
 * @param mode Execution mode (sequential or parallel)
 * @return StepResult containing sub-steps and exceptions
 */
template <std::random_access_iterator Iterator, typename Func>
auto for_each_with_index_safe(Iterator       begin, Iterator end, Func&& func,
                              execution_mode mode = execution_mode::sequential) -> StepResult {
  StepResult parent_step{};
  const auto count = static_cast<std::ptrdiff_t>(end - begin);
  const auto functor_name = get_type_name<Func>("Functor"s);
  parent_step.step_name = "Apply "s + functor_name + " for each item "s +
                          (mode == execution_mode::parallel ? " in parallel"s : " sequentially"s);    
  parent_step.init_sub_steps(count,functor_name);  

  for_each_with_index(
    begin, end,
    [&func, functor_name, &parent_step](auto& item, std::ptrdiff_t idx)
    {
      auto& current_step = parent_step[idx];
      try {
        if constexpr (std::is_same_v<StepResult, std::invoke_result_t<Func, decltype(item), std::ptrdiff_t>>) {
          
          const auto saved_step_name = current_step.step_name;
          current_step = std::forward<Func>(func)(item, idx);
          current_step.step_name = saved_step_name + " -> " + current_step.step_name;
        }
        else {
          std::forward<Func>(func)(item, idx);
          current_step = true;
        }
      }
      catch (...) {
        STORE_EXCEPTION(current_step.exception);
        current_step = false;
      }
    },
    mode);

  parent_step.apply_sub_steps_failure_policy();

  return parent_step;
}

/**
 * @brief Exception-safe wrapper for #transform that collects exceptions
 * This function wraps the standard #transform function to ensure that any exceptions
 * thrown by the provided function are caught and recorded in the StepResult.
 * @tparam InputIt Input iterator type
 * @tparam OutputIt Output iterator type
 * @tparam UnaryFunc Function type
 * @param first Iterator to the beginning of the input range
 * @param last Iterator to the end of the input range
 * @param result Iterator to the beginning of the output range
 * @param op Function to apply to each element
 * @param mode Execution mode (sequential or parallel)
 * @return StepResult containing sub-steps and exceptions
 *
 */
template <std::random_access_iterator InputIt, std::random_access_iterator OutputIt, typename UnaryFunc>
auto transform_safe(InputIt        first, InputIt last, OutputIt result, UnaryFunc&& op,
                    execution_mode mode = execution_mode::sequential) -> StepResult {

  StepResult parent_step{};  
  const auto functor_name = get_type_name<UnaryFunc>("Functor"s);  
  parent_step.step_name = "Transform vector using an unary functor "s + functor_name + (mode == execution_mode::parallel ? " in parallel"s : " sequentially"s);  
  const auto count = static_cast<std::ptrdiff_t>(last - first);
  parent_step.init_sub_steps(count,functor_name);
  
#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
    #pragma omp parallel for default(none) shared(result, op, parent_step) firstprivate(count, first)
    for (std::ptrdiff_t i = 0; i < count; ++i) {
       auto& current_step = parent_step[i];
      try {
        result[i] = std::forward<UnaryFunc>(op)(first[i]);
        current_step = true;
      }
      catch (...) {
        current_step = false;
        STORE_EXCEPTION(current_step.exception);
      }
    }
    parent_step.apply_sub_steps_failure_policy();
    return parent_step;
  }
#endif

  // Sequential fallback
  for (std::ptrdiff_t i = 0; i < count; ++i) {
       auto& current_step = parent_step[i];
      try {                                                                                                           
        result[i] = std::forward<UnaryFunc>(op)(first[i]);
         current_step = true;          
      }
      catch (...) {
        current_step  = false;
        STORE_EXCEPTION(current_step.exception);
      }
    }

  parent_step.apply_sub_steps_failure_policy();
  return parent_step;
}

/**
 * @brief Run a transform operation on input range to produce output range
 * @note This is a convenience wrapper around scheduler::transform 
 * @tparam ExecPolicy 
 * @tparam TResult 
 * @tparam T 
 * @param input 
 * @param output 
 * @param task : a wrapped task function that takes const T& and returns TResult
 * @todo integrate exception handling
 * @todo remove template arguments ExecPolicy and use argument execution_mode instead
 */
template <execution_mode ExecPolicy, typename TResult, typename T>
auto run_transform(const std::vector<T>&                      input, std::vector<TResult>& output,
                   const task_utils::Task<TResult, const T&>& task) {
  output.resize(input.size());
  scheduler::transform(
    input.begin(), input.end(), output.begin(), [&](const T& element) { return task(element); },
    ExecPolicy);
}

/**
 * @brief for_each convenience function that applies a task to each element in a vector
 * while providing the element's index.
 *
 * @tparam T The type of elements in the data vector
 * @param execution_mode execution_mode The execution mode (sequential or parallel)
 * @param data The vector of elements to process
 * @param task The task to apply to each element, accepting the index and the element
 */
template <typename T>
void for_each_indexed(scheduler::execution_mode     execution_mode, std::vector<T>& data,
                      const IndexedFilterTask<T>& task) {
  std::vector<size_t> indices(data.size());
  std::iota(indices.begin(), indices.end(), 0);
  scheduler::for_each(indices.begin(), indices.end(), [&](size_t i) { task(i, data[i]); }, execution_mode);
}


/**
 * @brief Convenience function to run a container of callable tasks returning the same type
 * @param tasks Vector or any container of callable tasks
 * @param mode Execution mode (sequential or parallel)
 * @return Vector of results from each task
 */
template <typename TaskContainer>
auto run_contained_functions(TaskContainer&& tasks, execution_mode mode = execution_mode::parallel) {
  using ResultType = std::invoke_result_t<typename std::decay_t<TaskContainer>::value_type>;
  std::vector<ResultType> results(tasks.size());

#if defined(_OPENMP)
  if (mode == execution_mode::parallel) {
#pragma omp parallel for default(none) shared(tasks, results)
    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
      results[i] = tasks[i]();
    }
    return results;
  }
#endif

  // Sequential fallback
  for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
    results[i] = tasks[i]();
  }

  return results;
}
} // namespace scheduler
} // namespace funcflow
