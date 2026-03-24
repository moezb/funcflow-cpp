#pragma once
/**
 * @file stage_builder.hpp
 * @brief hold classes and structures for task composition using a fluent API.
 */
#include <algorithm>
#include <functional>
#include <vector>
#include <tuple>
#include <variant>
#include <type_traits>
#include <concepts>
#include <utility>
#include <unordered_map>
#include "funcflow/pipeline_stage.hpp"
#include "funcflow/scheduler.hpp"
#include "funcflow/dual_range.hpp"
#include "funcflow/utils/flags.hpp"
#include "funcflow/task_utils.hpp"
#include "funcflow/visitor_concepts.hpp"
#include "funcflow/visitor_runner.hpp"


namespace funcflow {
namespace workflow {
/**
 * @brief Fluent API version of pipeline_stage with builder pattern
 * Enables composing pipeline stages using a fluent interface:
 */
template <typename TContext>
class StageBuilder;

using namespace funcflow::context_view;
using namespace funcflow::task_utils;

// Forward declarations for builder classes
template <typename TContext, ContextViewRange TViewRange>
class IterateModifierBuilder;

template <typename TContext, DualContextViewRange TDualContextViewRange>
class IterateSetupBuilder;

template <typename TContext, DualContextViewRange TDualContextViewRange, MovableResult TResult, typename... TCollectors>
class IterateCollectorBuilder;

template <typename TContext, DualContextViewRange TDualContextViewRange, typename TFlagEnum, typename... TFlagVisitors>
class IterateFlagCollectorBuilder;

/**
 * @brief Main entry point for fluent stage building
 */
template <typename TContext>
class StageBuilder {
private:
  std::string name_;

public:
  explicit StageBuilder(std::string stage_name)
  : name_(std::move(stage_name)) {
  }

  /**
   * @brief Start building an iteration stage with sequential execution
   * @tparam TViewRange The context view range type
   */
  template <ContextViewRange TViewRange>
  IterateModifierBuilder<TContext, TViewRange> iterate(
    execution_mode mode = execution_mode::sequential) && {
    return IterateModifierBuilder<TContext, TViewRange>(std::move(name_), mode);
  }

  /**
   * @brief Start building an iteration stage with parallel execution
   * @tparam TViewRange The context view range type
   */
  template <ContextViewRange TViewRange>
  IterateModifierBuilder<TContext, TViewRange> parallel_iterate() && {
    return IterateModifierBuilder<TContext, TViewRange>(
      std::move(name_), execution_mode::parallel);
  }

  /**
   * @brief Start building a dual range iteration stage (supports modifiers or visitors)
   * @tparam TDualContextViewRange The dual context view range type
   */
  template <DualContextViewRange TDualContextViewRange>
  IterateSetupBuilder<TContext, TDualContextViewRange> iterate(
    execution_mode mode = execution_mode::sequential) && {
    return IterateSetupBuilder<TContext, TDualContextViewRange>(
      std::move(name_), mode);
  }

  /**
   * @brief Start building a dual range iteration stage with parallel execution
   * @tparam TDualContextViewRange The dual context view range type
   */
  template <DualContextViewRange TDualContextViewRange>
  IterateSetupBuilder<TContext, TDualContextViewRange> parallel_iterate() && {
    return IterateSetupBuilder<TContext, TDualContextViewRange>(
      std::move(name_), execution_mode::parallel);
  }

  /// @brief Create a stage with a sequence of operations from lambda instances (passed as arguments)
  /// @tparam ...Tasks types of the lambda instances that will be executed in order.
  /// @details Execution will be interrupted on first failure (false return or exception).
  ///          Bool-returning tasks use their return value; non-bool tasks succeed if no exception.
  ///          Tasks can be freely mixed (bool and non-bool) in the same sequence.
  /// @param ...tasks the lambda instances that will be executed in order.
  /// @return The stage that can be added to the Pipeline.
  template <typename... Tasks>
      requires(std::invocable<Tasks, TContext&> && ...)
  auto sequence(Tasks&&... tasks) -> pipeline_stage<TContext>
  {
      pipeline_stage<TContext> stage{};
      stage.name = name_;
      auto task_tuple = std::make_tuple(std::forward<Tasks>(tasks)...);
      stage.runner_ = [task_tuple = std::move(task_tuple)](TContext& context) mutable
      {
          StepResult runner_result{};
          runner_result  = true;
          try
          {
              size_t task_index = 0;
              std::apply(
                  [&context, &runner_result, &task_index](auto&&... tasks)
                  { (safe_invoke(std::forward<decltype(tasks)>(tasks), context, runner_result, task_index), ...); },
                  task_tuple);
          }
          catch (...)
          {
              runner_result = false;
              STORE_EXCEPTION(runner_result.exception);
          }
          return runner_result;
      };
      return stage;
  }

  /// @brief Create a stage with a sequence of operations from functor types (template-based)
  /// @tparam ...Functors types of the default constructable functors that will be executed in order.
  /// @details Functors must be default constructable. Execution stops on first failure (false return or exception).
  ///          Bool-returning functors use their return value; non-bool functors succeed if no exception.
  ///          Functors can be freely mixed (bool and non-bool) in the same sequence.
  /// @return The stage that can be added to the Pipeline.
  template <typename... Functors>
      requires(std::invocable<Functors, TContext&> && ...) &&
              (std::is_default_constructible_v<Functors> && ...)
  auto sequence() -> pipeline_stage<TContext>
  {
      pipeline_stage<TContext> stage{};
      stage.name = name_;
      std::tuple<Functors...> the_functors_tuple{}; // default-constructed functors
      constexpr bool fail_fast = true; // Stop on first failure always for this version
      const auto functors_names = get_types_names<Functors...>("functor"s);
      stage.runner_ = [functors_names](TContext& context)
      {
          StepResult runner_result{};
          runner_result = true;
          try
          {
              std::tuple<Functors...> functors_tuple{}; // default-constructed functors
              std::apply(
                  [functors_names,&context, &runner_result](auto&&... tasks)
                  {
                      size_t index = 0;
                      (
                          [functors_names,&context, &runner_result, index](auto&& task)
                          {
                              auto& current_step = runner_result.sub_steps.emplace_back();
                              current_step.step_name = functors_names[index];
                              try
                              {
                                  if (fail_fast && !runner_result)
                                      return; // Skip execution if fail_fast and an exception was already caught

                                  using task_type = std::decay_t<decltype(task)>;
                                  if constexpr (std::is_invocable_r_v<bool, task_type, TContext&>)
                                  {
                                      // Task returns bool - use return value to determine success
                                      bool result = std::invoke(std::forward<decltype(task)>(task), context);                                      
                                      current_step = result;
                                      runner_result = result;
                                  }
                                  else
                                  {
                                      // Task returns void or other type - assume success
                                      std::invoke(std::forward<decltype(task)>(task), context);
                                      current_step = true;
                                  }
                              }
                              catch (...)
                              {
                                STORE_EXCEPTION(current_step.exception);
                                current_step = false;
                                runner_result = false;
                              }
                          }(tasks),
                          ...);
                      ++index;
                  },
                  functors_tuple);
          }
          catch (...)
          {
              runner_result = false;
              STORE_EXCEPTION(runner_result.exception);
          }
          return runner_result;
      };
      return stage;
  }


template <typename TFlagEnum>
  auto collect_flags(const std::vector<Task<Flags<TFlagEnum>, const TContext&>>& tasks,
                     Flags<TFlagEnum> TContext::* member_ptr, execution_mode mode)
  {
    pipeline_stage<TContext> stage{};
    stage.name               = name_;    
    stage.runner_            = [&tasks,mode, member_ptr](TContext& context) 
    {
        StepResult runner_result{};
        std::vector<std::optional<Flags<TFlagEnum>>> result{};
        try
        {
            runner_result = true;
            result = scheduler::run_contained_tasks(tasks, runner_result, mode, std::as_const(context));
            if (!runner_result)
                return runner_result;
            for (const auto& trait : result)
            {
                context.*member_ptr |= trait.value_or(Flags<TFlagEnum>{}) ;
            }
        }
        catch (...)
        {
            runner_result = false;
            STORE_EXCEPTION(runner_result.exception);
        }
        return runner_result;
    };
    return stage;
  }

  template <typename TFlagEnum>
  auto collect_flags_and_validate(const std::vector<Task<Flags<TFlagEnum>,const TContext&>>& collectors_tasks,
    bool fail_fast,
    Flags<TFlagEnum> TContext::* member_ptr,
    std::function<bool(Flags<TFlagEnum>)>&& predicate)
  {
      static_assert(std::is_copy_constructible_v<Task<Flags<TFlagEnum>, const TContext&>>);
      pipeline_stage<TContext> stage{};
      stage.name = name_;
      stage.runner_ = [&collectors_tasks, member_ptr,fail_fast,predicate](TContext& context) mutable
      {
          StepResult runner_result{};
          runner_result = true;
          std::vector<Flags<TFlagEnum>> result{};
          try
          {
              int collector_index = 0; 
              for (const auto& task: collectors_tasks)
              {
                  StepResult& collector_result = runner_result.sub_steps.emplace_back();
                  constexpr std::string_view flag_type_name = typeid(TFlagEnum).name();                  
                  collector_result.step_name = std::string(flag_type_name) + " flag collector index:"s + std::to_string(collector_index++);
                  collector_result = false;
                  Flags<TFlagEnum> collected_flags = task(context);                  
                  context.*member_ptr |= collected_flags;                
                  if (fail_fast) {
                    collector_result = predicate(std::move(collected_flags));
                    runner_result = (bool) collector_result;
                  }
                  else {
                    collector_result = true;
                  }
                  if (!runner_result)
                      return runner_result;
              }
              if (!fail_fast)
                  runner_result = predicate(context.*member_ptr);
          }
          catch (...)
          {
            runner_result = false;
            STORE_EXCEPTION(runner_result.exception);
          }
          return runner_result;
      };
      return stage;
  }
  

  /**
   * @brief Return a stage that runs a set of tasks in parallel.
   * Use the scheduler::for_each to run the tasks in parallel.
   * @tparam TContext The type of the context passed to each task.
   * @param name  Stage friendly name
   * @param tasks A vector of tasks to run in parallel
   * @pre Each task must be thread-safe and handle concurrent access to context
   * @pre Tasks must not depend on execution order of other tasks
   * @pre Tasks should either synchronize context access or operate on independent context parts
   * @return pipeline_stage<TContext>
   */
  auto parallel(const std::vector<std::function<void(TContext&)>>& tasks)
      -> pipeline_stage<TContext>
  {
      pipeline_stage<TContext> stage{};
      stage.name = name_;
      stage.runner_ = [tasks = std::move(tasks), stage](TContext& context) mutable
      {
          StepResult runner_result{};
          runner_result = true;
          try
          {
              runner_result = scheduler::for_each_safe(
                  tasks.begin(), tasks.end(), [&context](const auto& task) { task(context); },
                  execution_mode::parallel);
          }
          catch (...)
          {
            runner_result = false;
            STORE_EXCEPTION(runner_result.exception);
          }
          return runner_result;
      };
      return stage;
  }
  

  private:
  // helpers for iterating over a tuple of tasks
  // This is used to execute a sequence of tasks in order, skipping the rest if one fails.
  template <typename Tuple>
  static StepResult execute_tuple_tasks_in_sequence(Tuple& task_tuple, TContext& context)
  {
      StepResult parent_result{};
      parent_result =
          execute_tuple_tasks_in_sequence_impl(task_tuple, context, parent_result,
                             std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
      return parent_result;
  }

  template <typename Tuple, std::size_t... I>
  static bool execute_tuple_tasks_in_sequence_impl(Tuple& task_tuple, TContext& context, StepResult& parent_result,
                                 std::index_sequence<I...>)
  {
      const int dummy[] = {(invoke_tuple_task<I>(task_tuple, context, parent_result), 0)...};
      (void)dummy; // To avoid unused variable warning
      return parent_result;
  }

  template <std::size_t Index, typename Tuple>
  static void invoke_tuple_task(Tuple& task_tuple, TContext& context, StepResult& parent_step_result)
  {
      if (!parent_step_result)
          return; // Skip if previous failed

      auto& current_step = parent_step_result.sub_steps.emplace_back();

      current_step.step_name = get_type_name<std::tuple_element_t<Index, Tuple>>("Task"s) + "["s + std::to_string(Index) + "]"s;

      try
      {
          auto& task = std::get<Index>(task_tuple);
          current_step = task(context); // Or std::invoke(Task, context) for more flexibility
          parent_step_result = current_step;
      }
      catch (...)
      {
          STORE_EXCEPTION(current_step.exception);
          parent_step_result = false;
          current_step = false;
      }
  }

  template <typename Task>
      requires(std::invocable<Task, TContext&>)
  static void safe_invoke(Task task, TContext& context, StepResult& parent_step, size_t& task_index)
  {
      if (!parent_step)
          return; // Skip if previous failed

      auto& current_step = parent_step.sub_steps.emplace_back();
      current_step.step_name = get_type_name<Task>("Task"s) + "["s + std::to_string(task_index) + "]"s;
      try
      {
          if constexpr (std::is_invocable_r_v<bool, Task, TContext&>)
          {
              // Task returns bool - use the return value to determine success
              bool result = std::invoke(task, context);
              current_step = result;
              parent_step = result;
          }
          else
          {
              // Task returns void or other type - assume success if no exception thrown
              std::invoke(task, context);
              current_step = true;
              parent_step = true;
          }
      }
      catch (...)
      {
          STORE_EXCEPTION(current_step.exception);
          parent_step = false;
          current_step = false;          
      }
      task_index++;
  }
};

/**
 * @brief Builder for iteration with modifiers (mutable operations)
 */
template <typename TContext, ContextViewRange TViewRange>
class IterateModifierBuilder {
private:
  std::string    name_;
  execution_mode iterate_mode_;

  friend class StageBuilder<TContext>;

  IterateModifierBuilder(std::string name, execution_mode mode)
  : name_(std::move(name)), iterate_mode_(mode) {
  }

public:
  /**
   * @brief Apply modifier functors to each element in the range
   * @tparam TModifiers Functor types that modify elements
   */
  template <typename... TModifiers>
    requires (VoidModifierFunctor<TModifiers, typename TViewRange::view> && ...) &&
             (std::is_default_constructible_v<TModifiers> && ...)
  pipeline_stage<TContext> with_modifiers() && {
    pipeline_stage<TContext> stage;
    stage.name    = std::move(name_);
    stage.runner_ = [iterate_mode = iterate_mode_](TContext& context)
    {
      StepResult runner_result{};
      runner_result = true;
      try {
        TViewRange range{context};
        scheduler::for_each_safe(
          range.begin(), range.end(),
          [](typename TViewRange::view& element)
          {
            // Apply each modifier in sequence
            (TModifiers{}(element), ...);
          },
          runner_result,
          iterate_mode);
      }
      catch (...) {
        STORE_EXCEPTION(runner_result.exception);
        runner_result = false;
      }
      return runner_result;
    };
    return stage;
  }
};

/**
 * @brief Builder for setting up iteration operations on dual ranges (modifiers or visitors)
 */
template <typename TContext, DualContextViewRange TDualContextViewRange>
class IterateSetupBuilder {
private:
  std::string    name_;
  execution_mode iterate_mode_;

  friend class StageBuilder<TContext>;

  IterateSetupBuilder(std::string name, execution_mode mode)
  : name_(std::move(name)), iterate_mode_(mode) {
  }

public:
  /**
   * @brief Apply modifier functors to each element in the mutable range
   * @tparam TModifiers Functor types that modify elements
   */
  template <typename... TModifiers>
    requires (VoidModifierFunctor<TModifiers, typename TDualContextViewRange::view> && ...) &&
             (std::is_default_constructible_v<TModifiers> && ...)
  pipeline_stage<TContext> with_modifiers() && {
    pipeline_stage<TContext> stage;
    stage.name    = std::move(name_);
    stage.runner_ = [iterate_mode = iterate_mode_](TContext& context)
    {
      StepResult runner_result{};
      runner_result = true;
      try {
        auto dual_range    = TDualContextViewRange{context};
        auto mutable_range = dual_range.get_mutable_range();
        scheduler::for_each_safe(
          mutable_range.begin(), mutable_range.end(),
          [](typename TDualContextViewRange::view& element)
          {
            // Apply each modifier in sequence
            (TModifiers{}(element), ...);
          },
          runner_result, iterate_mode);
      }
      catch (...) {
        STORE_EXCEPTION(runner_result.exception);
        runner_result = false;
      }
      return runner_result;
    };
    return stage;
  }


  /**
   * @brief Configure collectors with sequential execution mode
   * @tparam TResult The result type that collectors return in vectors
   * @tparam TCollectors Collector functor types (return std::vector<TResult>)
   */
  template <MovableResult TResult, typename... TCollectors>
    requires (Collector<TCollectors, typename TDualContextViewRange::view, TResult> && ...)
  IterateCollectorBuilder<TContext, TDualContextViewRange, TResult, TCollectors...>
  with_collectors(execution_mode collector_mode = execution_mode::sequential) && {
    return IterateCollectorBuilder<TContext, TDualContextViewRange, TResult, TCollectors...>(
      std::move(name_), iterate_mode_, collector_mode);
  }

  /**
   * @brief Configure collectors with parallel execution mode
   * @tparam TResult The result type that collectors return in vectors
   * @tparam TCollectors Collector functor types (return std::vector<TResult>)
   */
  template <MovableResult TResult, typename... TCollectors>
    requires (Collector<TCollectors, typename TDualContextViewRange::view, TResult> && ...)
  IterateCollectorBuilder<TContext, TDualContextViewRange, TResult, TCollectors...>
  with_parallel_collectors() && {
    return IterateCollectorBuilder<TContext, TDualContextViewRange, TResult, TCollectors...>(
      std::move(name_), iterate_mode_, execution_mode::parallel);
  }

  /**
   * @brief Configure flag collectors with sequential execution mode
   * @tparam TFlagEnum The enum type used by the Flags
   * @tparam TFlagVisitors Visitor functor types (return Flags<TFlagEnum>)
   */
  template <typename TFlagEnum, typename... TFlagVisitors>
  IterateFlagCollectorBuilder<TContext, TDualContextViewRange, TFlagEnum, TFlagVisitors...>
  with_flag_collectors(execution_mode visitor_mode = execution_mode::sequential) && {
    return IterateFlagCollectorBuilder<TContext, TDualContextViewRange, TFlagEnum, TFlagVisitors...>(
      std::move(name_), iterate_mode_, visitor_mode);
  }

  /**
   * @brief Configure flag collectors with parallel execution mode
   * @tparam TFlagEnum The enum type used by the Flags
   * @tparam TFlagVisitors Visitor functor types (return Flags<TFlagEnum>)
   */
  template <typename TFlagEnum, typename... TFlagVisitors>
  IterateFlagCollectorBuilder<TContext, TDualContextViewRange, TFlagEnum, TFlagVisitors...>
  with_parallel_flag_collectors() && {
    return IterateFlagCollectorBuilder<TContext, TDualContextViewRange, TFlagEnum, TFlagVisitors...>(
      std::move(name_), iterate_mode_, execution_mode::parallel);
  }
};


/**
 * @brief Builder for iteration with collectors and optional merge
 *
 * Collectors are functors that return std::vector<TResult> from const views.
 * All collector results are flattened into a single vector before being
 * passed to the merge callback, providing a cleaner API than visitors.
 */
template <typename TContext, DualContextViewRange TDualContextViewRange,
          MovableResult TResult, typename... TCollectors>
class IterateCollectorBuilder {
private:
  std::string    name_;
  execution_mode iterate_mode_;
  execution_mode collector_mode_;

  friend class IterateSetupBuilder<TContext, TDualContextViewRange>;

  IterateCollectorBuilder(std::string name, execution_mode iter_mode, execution_mode col_mode)
  : name_(std::move(name)), iterate_mode_(iter_mode), collector_mode_(col_mode) {
  }

public:
  /**
   * @brief Finalize stage with a merge callback for flattened collector results
   * @param callback Function to process flattened results: void(TView&, const std::vector<TResult>&)
   */
  pipeline_stage<TContext>
  merge(collector_merge_callback_t<typename TDualContextViewRange::view, TResult>&& callback) && {
    pipeline_stage<TContext> stage;
    stage.name    = std::move(name_);
    stage.runner_ = [cb = std::move(callback),
      iterate_mode = iterate_mode_,
      collector_mode = collector_mode_](TContext& context)
    {
      StepResult runner_result{};
        runner_result = true;
      try {
        TDualContextViewRange dual_range(context);
        auto mutable_range = dual_range.get_mutable_range();
        view_collector_runner<typename TDualContextViewRange::view, TResult, TCollectors...> collector_runner{
            collector_mode};

        scheduler::for_each_safe(
          mutable_range.begin(), mutable_range.end(),
          [&collector_runner, &cb](typename TDualContextViewRange::view& view)
          {
            StepResult step_result{};
            collector_runner(view, cb, step_result);
            return step_result;
          },
          runner_result,
          iterate_mode);
      }
      catch (...) {
        STORE_EXCEPTION(runner_result.exception);
        runner_result = false;
      }
      return runner_result;
    };
    return stage;
  }

  /**
   * @brief Finalize stage without merge callback (just run collectors)
   */
  pipeline_stage<TContext> build() && {
    pipeline_stage<TContext> stage;
    stage.name    = std::move(name_);
    stage.runner_ = [iterate_mode = iterate_mode_,
      collector_mode = collector_mode_](TContext& context)
    {
      StepResult runner_result{};
        runner_result = true;
      try {
        typename TDualContextViewRange::const_range iterator(context);
        view_collector_runner<typename TDualContextViewRange::view, TResult, TCollectors...> collector_runner{collector_mode};

        scheduler::for_each_safe(
          iterator.begin(), iterator.end(),
          [&collector_runner](const typename TDualContextViewRange::view& view)
          {
            // Run collectors without merge callback
            StepResult step_result{};
            collector_runner.run_collectors_only(view, step_result);
            return step_result;
          },
          runner_result,
          iterate_mode);
      }
      catch (...) {
        STORE_EXCEPTION(runner_result.exception);
        runner_result = false;
      }
      return runner_result;
    };
    return stage;
  }
};

/**
 * @brief Builder for iteration with flag collectors
 *
 * Flag collectors are visitors that return Flags<TFlagEnum> from const views.
 * All returned flags are accumulated using bitwise OR and stored in a specified
 * member field of each view.
 */
template <typename TContext, DualContextViewRange TDualContextViewRange,
          typename TFlagEnum, typename... TFlagVisitors>
class IterateFlagCollectorBuilder {
private:
  std::string    name_;
  execution_mode iterate_mode_;
  execution_mode visitor_mode_;

  friend class IterateSetupBuilder<TContext, TDualContextViewRange>;

  IterateFlagCollectorBuilder(std::string name, execution_mode iter_mode, execution_mode vis_mode)
  : name_(std::move(name)), iterate_mode_(iter_mode), visitor_mode_(vis_mode) {
  }

public:
  /**
   * @brief Finalize stage by storing accumulated flags into a member field
   * @param member_ptr Pointer to the Flags member where flags should be accumulated
   */
  pipeline_stage<TContext>
  store_in(Flags<TFlagEnum> TDualContextViewRange::view::* member_ptr) && {
    pipeline_stage<TContext> stage;
    stage.name    = std::move(name_);
    stage.runner_ = [member_ptr,
      iterate_mode = iterate_mode_,
      visitor_mode = visitor_mode_](TContext& context)
    {
      StepResult runner_result{};
        runner_result = true;
      try {
        TDualContextViewRange dual_range(context);
        auto                  mutable_range = dual_range.get_mutable_range();

        // Run the entire operation: collect flags AND assign to members
        scheduler::for_each_safe(
          mutable_range.begin(), mutable_range.end(),
          [member_ptr, visitor_mode](typename TDualContextViewRange::view& view)
          {
            StepResult visitors_step{};
            visitors_step.step_name = "Running flag visitors";
            try {
              // For visitors: cast to const at usage level (semantic const)
              const auto& const_view = view;

              // Collect flags from all visitors (within each view)
              Flags<TFlagEnum> accumulated_flags;

              // Run visitors and collect flags
              if constexpr (sizeof...(TFlagVisitors) > 0) {
                auto visitor_results =
                  scheduler::run_functors_typed<typename TDualContextViewRange::view, Flags<TFlagEnum>,
                  TFlagVisitors...>(
                    visitor_mode, const_view, visitors_step);

                // Accumulate all visitor results
                for (const auto& flag_result : visitor_results) {
                  accumulated_flags |= flag_result;
                }
              }

              // Assign accumulated flags to the member
              view.*member_ptr |= accumulated_flags;
            }
            catch (...) {
              visitors_step = false;
              STORE_EXCEPTION(visitors_step.exception);
            }
            return visitors_step;
          },
          runner_result,
          iterate_mode);
      }
      catch (...) {
        STORE_EXCEPTION(runner_result.exception);
        runner_result = false;
      }
      return runner_result;
    };
    return stage;
  }
};

} // namespace workflow
} // namespace funcflow
