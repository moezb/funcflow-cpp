# FuncFlow

[![Build & Test](https://github.com/moezb/funcflow-cpp/actions/workflows/build.yml/badge.svg)](https://github.com/moezb/funcflow-cpp/actions/workflows/build.yml)

A simple header-only C++20 library for building type-safe, functor-based data processing pipelines over ranges.

FuncFlow lets you compose multi-stage workflows from small, reusable functors — modifiers that transform data, collectors that extract results, and flag visitors that accumulate state — all wired together with a fluent builder API and optional parallel execution using OpenMP.

## Features

- **Fluent Builder API** — compose pipeline stages with a readable, chainable syntax
- **Type-Safe at Compile Time** — C++20 concepts validate functor signatures, view getters, and range types before any code runs
- **Lazy View Extraction** — iterate over logical "views" of a context without copying data; views are computed on-demand via custom getters
- **Multiple Processing Patterns** — modifiers (mutate elements), collectors (extract and merge results), flag visitors (accumulate bitwise flags), and sequences (ordered task chains)
- **Sequential & Parallel Execution** — switch between execution modes with a single parameter; parallel mode uses OpenMP when available, falls back to sequential otherwise
- **Exception-Safe Execution** — every operation at every level (stage, element, functor, merge callback) is wrapped in try-catch; exceptions are captured in a hierarchical `StepResult` tree so the pipeline never terminates unexpectedly — remaining elements and stages continue, and all errors are available for deferred inspection
- **Per-Stage Timing** — automatic nanosecond-precision timing statistics for every stage

## Requirements

- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- OpenMP (optional, for parallel execution) which is usually already included for GCC.


## Installation

FuncFlow is header-only. Copy the `include/funcflow/` directory into your project, or add it via CMake:

```cmake
add_subdirectory(path/to/funcflow)
target_link_libraries(your_target PRIVATE funcflow)
```

## Quick Start

### 1. Define Your Context and Views

A **context** holds the data your pipeline processes. A **view** is a logical element extracted from the context via a **getter**.

```cpp
#include "funcflow/pipeline.hpp"
#include "funcflow/stage_builder.hpp"

using namespace funcflow::workflow;
using namespace funcflow::context_view;

struct Item {
    int value = 0;
    std::string name;
    bool processed = false;
};

struct MyContext {
    std::vector<Item> items;
    size_t size() const { return items.size(); }
};

// View getter — tells FuncFlow how to extract a view from the context
struct ItemGetter {
    Item& operator()(size_t idx, MyContext& ctx) const {
        return ctx.items[idx];
    }
    const Item& operator()(size_t idx, const MyContext& ctx) const {
        return ctx.items[idx];
    }
};

// Define range types
using ItemRange = context_view_range<MyContext, Item, ItemGetter>;
```

### 2. Write Functors

Functors are small, stateless structs with `operator()`. Their signature determines their role:

```cpp
// Modifier — mutates each element (void return)
struct DoubleValue {
    void operator()(Item& item) const {
        item.value *= 2;
    }
};

struct MarkProcessed {
    void operator()(Item& item) const {
        item.processed = true;
    }
};
```

### 3. Build and Run a Pipeline

```cpp
Pipeline<MyContext> pipeline;

// Stage 1: apply modifiers to each item sequentially
auto stage1 = pipeline.stage("double_and_mark")
    .iterate<ItemRange>()
    .with_modifiers<DoubleValue, MarkProcessed>();

pipeline.add_stage(std::move(stage1));

// Run
MyContext ctx;
ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}};
pipeline.run(ctx);

// ctx.items[0].value == 2, ctx.items[0].processed == true
// ctx.items[1].value == 4, ctx.items[1].processed == true
```

## Pipeline Patterns

### Modifiers

Mutate each element in a range. Modifiers are applied in template-parameter order.

```cpp
pipeline.stage("transform")
    .iterate<ItemRange>()
    .with_modifiers<DoubleValue, MarkProcessed>();
```

### Collectors

Extract results from each element (read-only), flatten them, and merge back. Collectors require a **dual range** to provide both const access (for reading) and mutable access (for merging).

```cpp
struct FindTags {
    std::vector<std::string> operator()(const Item& item) const {
        // return tags extracted from this item
    }
};

pipeline.stage("collect_tags")
    .iterate<DualItemRange>()
    .with_collectors<std::string, FindTags>()
    .merge([](Item& item, const std::vector<std::string>& tags) {
        // merge collected tags back into the item
    });
```

Collectors can also run without a merge callback — use `.build()` instead of `.merge(...)` to just execute the collectors and discard the results.

Collectors support independent parallelism control for the collection phase:

```cpp
// Parallel element iteration with parallel collectors
pipeline.stage("parallel_collect")
    .parallel_iterate<DualItemRange>()
    .with_parallel_collectors<std::string, FindTags>()
    .merge([](Item& item, const std::vector<std::string>& tags) { /* ... */ });

// Or explicitly pass execution mode
pipeline.stage("collect")
    .iterate<DualItemRange>()
    .with_collectors<std::string, FindTags>(execution_mode::parallel)
    .merge([](Item& item, const std::vector<std::string>& tags) { /* ... */ });
```

### Flag Visitors

Accumulate bitwise flags from read-only visits and store them in a member field.

```cpp
enum class Status : uint32_t { None = 0, Valid = 1, Active = 2, Flagged = 4 };

struct CheckValidity {
    Flags<Status> operator()(const Item& item) const {
        return item.value > 0 ? Flags{Status::Valid} : Flags<Status>{};
    }
};

pipeline.stage("check_flags")
    .iterate<DualItemRange>()
    .with_flag_collectors<Status, CheckValidity>()
    .store_in(&Item::status_flags);
```

Like collectors, flag visitors support independent parallelism control:

```cpp
pipeline.stage("parallel_flags")
    .iterate<DualItemRange>()
    .with_parallel_flag_collectors<Status, CheckValidity>()
    .store_in(&Item::status_flags);
```

### Sequences

Run a series of tasks on the context in order. Execution stops on first failure. Tasks can freely mix `bool`-returning (use return value) and `void`-returning (succeed if no exception) signatures.

```cpp
// From lambda instances
pipeline.stage("setup")
    .sequence(
        [](MyContext& ctx) { /* step 1 */ },
        [](MyContext& ctx) -> bool { /* step 2, return false to abort */ }
    );

// From functor types (default-constructed)
pipeline.stage("validate")
    .sequence<ValidateStep, PrepareStep, FinalizeStep>();
```

### Parallel Tasks

Run a set of independent tasks in parallel on the same context:

```cpp
pipeline.stage("parallel_work")
    .parallel({
        [](MyContext& ctx) { /* task A */ },
        [](MyContext& ctx) { /* task B */ },
        [](MyContext& ctx) { /* task C */ }
    });
```

### Parallel Execution

Switch any iteration to parallel mode:

```cpp
// Parallel iteration over elements
pipeline.stage("parallel_transform")
    .parallel_iterate<ItemRange>()
    .with_modifiers<DoubleValue>();

// Or pass execution mode explicitly
pipeline.stage("parallel_transform")
    .iterate<ItemRange>(execution_mode::parallel)
    .with_modifiers<DoubleValue>();
```

## Dual Ranges

When a stage needs both read-only access (for collectors/visitors) and mutable access (for merging results), use a **dual range**:

```cpp
using ConstItemRange = const_context_view_range<MyContext, Item, ItemGetter>;
using MutableItemRange = context_view_range<MyContext, Item, ItemGetter>;
using DualItemRange = dual_context_view_range<ConstItemRange, MutableItemRange>;

pipeline.stage("collect_and_merge")
    .iterate<DualItemRange>()
    .with_collectors<int, ValueExtractor>()
    .merge([](Item& item, const std::vector<int>& values) {
        // merge extracted values back
    });
```

## Flags Utility

`Flags<Enum>` is a type-safe wrapper for bitwise flag operations on enum types. It provides full bitwise operator support (`|`, `&`, `^`, `~` and their compound assignment forms) along with convenience methods:

```cpp
#include "funcflow/utils/flags.hpp"

enum class Perms : uint32_t { None = 0, Read = 1, Write = 2, Exec = 4 };

Flags<Perms> perms = Perms::Read | Perms::Write;

perms.testFlag(Perms::Read);       // true
perms.set_flag(Perms::Exec);       // set a flag
perms.clearFlag(Perms::Write);     // clear a flag
perms.toggle_flag(Perms::Read);    // toggle a flag
perms.has_any_flag(Perms::Read | Perms::Write);  // true if any match
perms.has_all_flags({Perms::Read, Perms::Write}); // true if all match
perms.value();                     // raw underlying value
perms.clear();                     // reset to zero
```

For flags with power-of-2 values, additional introspection methods are available:

```cpp
perms.get_set_flags();           // std::vector<Perms> of individual set flags
perms.get_set_bit_positions();   // std::vector<int> of set bit positions
perms.to_string();               // "Read | Write" (requires enum_to_string specialization)
```

Helper macros for common declarations:

```cpp
// Define a type alias
DECLARE_FLAGS(PermFlags, Perms)

// Specialize FlagsMaxValue to enable get_all_flag_values() (must be in global namespace)
DECLARE_MAX_FLAGS_VALUE(Perms, Perms::Exec)

auto all = Flags<Perms>::get_all_flag_values(); // {None, Read, Write, Exec}
```

## EmptyContext

For pipelines that don't need shared state, `Pipeline` defaults to `EmptyContext`:

```cpp
Pipeline<> pipeline;  // equivalent to Pipeline<EmptyContext>
```

## Timing Statistics

Every pipeline run collects per-stage timing:

```cpp
pipeline.run(ctx);

const auto& stats = pipeline.get_timing_stats();
stats.print_stats(std::cout);                // total duration + stage count
stats.print_stats(std::cout, true);          // with per-stage breakdown
```

## Exception Handling

Exception safety is a core design principle of FuncFlow. Rather than letting a single failing element crash the entire pipeline, FuncFlow captures exceptions at every level and records them in a hierarchical `StepResult` tree. This allows the pipeline to **continue processing remaining elements** and lets you **defer error handling** to after execution completes.

### How It Works

Exceptions are caught and recorded at four levels, forming a tree:

```
Pipeline::run()
 └── pipeline_stage::run()                    ← catches stage-level exceptions
      └── scheduler::for_each_safe()          ← catches per-element exceptions
           ├── modifier / collector functor   ← catches per-functor exceptions
           └── merge callback                 ← catches merge exceptions
```

Every try-catch block stores the exception via `std::current_exception()` into the corresponding `StepResult` node. No exception is ever silently swallowed — they are all preserved for later inspection.

### The StepResult Tree

`StepResult` is the fundamental building block for exception tracking. Each node records:

- **`success()`** / **`operator bool()`** — whether this step succeeded
- **`exception`** — the captured `std::exception_ptr` (if any)
- **`sub_steps`** — child `StepResult` nodes (one per element, functor, or sub-operation)
- **`step_name`** — a human-readable label for diagnostics

```cpp
struct StepResult {
    std::vector<StepResult> sub_steps;
    std::string             step_name;
    std::exception_ptr      exception;

    operator bool() const;                        // true if step succeeded
    bool success() const;
    bool executed() const;

    void init_sub_steps(size_t count);                       // pre-allocate sub-steps
    void init_sub_steps(size_t count, const std::string& prefix); // "prefix[0]", "prefix[1]", ...
    StepResult& operator[](size_t index);                    // indexed access
    bool apply_sub_steps_failure_policy();                    // mark parent failed if any child failed
};
```

### Inspecting Results After Execution

After `pipeline.run(ctx)`, every stage exposes its `stage_result_` — a `StepResult` containing the full exception tree:

```cpp
pipeline.run(ctx);

for (auto& stage : pipeline.stages_) {
    auto& result = stage.stage_result_;

    if (!result) {
        std::cerr << "Stage '" << stage.name << "' failed\n";

        // Walk the sub-steps tree (one per element processed)
        for (size_t i = 0; i < result.sub_steps.size(); ++i) {
            auto& element_step = result.sub_steps[i];

            if (!element_step) {
                std::cerr << "  Element " << i << " (" << element_step.step_name << ") failed\n";

                // Element-level exception
                if (element_step.exception) {
                    try { std::rethrow_exception(element_step.exception); }
                    catch (const std::exception& e) {
                        std::cerr << "    Exception: " << e.what() << "\n";
                    }
                }

                // Functor-level or merge-callback exceptions (nested sub-steps)
                for (auto& functor_step : element_step.sub_steps) {
                    if (functor_step.exception) {
                        try { std::rethrow_exception(functor_step.exception); }
                        catch (const std::exception& e) {
                            std::cerr << "    " << functor_step.step_name
                                      << ": " << e.what() << "\n";
                        }
                    }
                }
            }
        }
    }
}
```

### Exception Behavior by Pattern

| Pattern | Behavior on exception |
|---|---|
| **Modifiers** | Exception is caught per-element; remaining elements continue processing |
| **Collectors** | Per-functor exceptions are caught; merge callback exceptions are caught separately |
| **Flag Visitors** | Per-visitor exceptions are caught; flag accumulation continues for other visitors |
| **Sequences** | Fail-fast — execution stops at the first failing task; exception is recorded in that task's sub-step |
| **Parallel tasks** | All tasks run; exceptions are collected per-task via `for_each_safe` |
| **Pipeline** | Stages run in order; if a stage fails, the pipeline stops at that stage |

### Exception Handling at the Scheduler Level

The `_safe` variants of scheduler functions (`for_each_safe`, `for_each_with_index_safe`, `transform_safe`) are the workhorses behind pipeline exception safety. They wrap each element operation in try-catch and record results in a `StepResult`:

```cpp
using namespace funcflow::scheduler;

std::vector<int> data = {1, 0, 3, 0, 5};

StepResult result;
for_each_safe(data.begin(), data.end(),
    [](int& x) {
        if (x == 0) throw std::runtime_error("division by zero");
        x = 100 / x;
    },
    result, execution_mode::sequential);

// result is false (some elements failed), but all elements were attempted
// result.sub_steps[0] is true  — element 0 processed (100/1 = 100)
// result.sub_steps[1] is false — element 1 threw, exception captured
// result.sub_steps[2] is true  — element 2 processed (100/3 = 33)
// result.sub_steps[3] is false — element 3 threw, exception captured
// result.sub_steps[4] is true  — element 4 processed (100/5 = 20)
```

Similarly, `run_functors_typed` and `run_contained_tasks` catch per-functor/per-task exceptions and record them as sub-steps, calling `apply_sub_steps_failure_policy()` to propagate failure upward.

## Scheduler

The `funcflow::scheduler` namespace provides low-level execution primitives that the pipeline uses internally. These can also be used directly:

```cpp
using namespace funcflow::scheduler;

std::vector<int> data = {1, 2, 3, 4, 5};

// Apply a function to each element (sequential or parallel)
for_each(data.begin(), data.end(), [](int& x) { x *= 2; }, execution_mode::parallel);

// With index
for_each_with_index(data.begin(), data.end(),
    [](int& x, std::ptrdiff_t idx) { x += idx; }, execution_mode::parallel);

// Transform input to output
std::vector<int> output(data.size());
transform(data.begin(), data.end(), output.begin(),
    [](int x) { return x * 2; }, execution_mode::parallel);
```

Exception-safe variants record per-element exceptions in a `StepResult`:

```cpp
StepResult result;
for_each_safe(data.begin(), data.end(), [](int& x) { x *= 2; },
    result, execution_mode::parallel);

// Returns StepResult directly
auto result2 = for_each_with_index_safe(data.begin(), data.end(),
    [](int& x, std::ptrdiff_t idx) { x += idx; }, execution_mode::parallel);

auto result3 = transform_safe(data.begin(), data.end(), output.begin(),
    [](int x) { return x * 2; }, execution_mode::parallel);
```

Higher-level utilities (not part of the library interface but could be handy if you understand the internal concepts):

```cpp
// Run multiple functors on the same input, collect typed results
StepResult step;
auto results = run_functors_typed<InputType, ResultType, Functor1, Functor2>(
    execution_mode::parallel, input, step);

// Run a vector of Task<> objects with result collection
auto results = run_contained_tasks(tasks, step, execution_mode::parallel, args...);

// Run simple function tasks
auto results = run_function_tasks<int>(function_vector, execution_mode::parallel);

// Run any container of callables
auto results = run_contained_functions(callable_vector, execution_mode::parallel);
```

## Architecture Overview

```
Pipeline<TContext>
 └── pipeline_stage<TContext>        // named stage with a runner closure
      └── StageBuilder<TContext>     // fluent entry point
           ├── .iterate<Range>()     ──→ IterateModifierBuilder
           │    └── .with_modifiers<...>()
           ├── .iterate<DualRange>() ──→ IterateSetupBuilder
           │    ├── .with_modifiers<...>()
           │    ├── .with_collectors<Result, ...>()         ──→ IterateCollectorBuilder
           │    │    ├── .merge(callback)
           │    │    └── .build()
           │    ├── .with_parallel_collectors<Result, ...>() ──→ IterateCollectorBuilder
           │    ├── .with_flag_collectors<Enum, ...>()       ──→ IterateFlagCollectorBuilder
           │    │    └── .store_in(&member)
           │    └── .with_parallel_flag_collectors<Enum, ...>()
           ├── .sequence(lambdas...)
           ├── .sequence<Functors...>()
           └── .parallel(tasks)

Scheduler (scheduler.hpp)
 ├── for_each / for_each_with_index            // raw parallel loops
 ├── transform                                 // parallel input→output mapping
 ├── for_each_safe / for_each_with_index_safe  // exception-collecting variants
 ├── transform_safe                            // exception-collecting transform
 ├── run_functors_typed<...>()                 // run multiple functors on same input
 ├── run_contained_tasks(vector<Task>)         // run task vector with result collection
 ├── run_function_tasks(vector<function>)      // run simple function tasks
 ├── run_contained_functions(container)        // run any container of callables
 ├── run_transform(input, output, task)        // convenience transform wrapper
 └── for_each_indexed(mode, data, task)        // indexed element processing
```

## Roadmap

- **Scheduler parallelism backends** — extend the scheduler beyond OpenMP to support additional parallelism strategies (e.g., thread pools, `std::execution` policies, TBB)

## License

This project is licensed under the [MIT License](LICENSE).
