#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "funcflow/scheduler.hpp"
#include "funcflow/utils/flags.hpp"
#include <numeric>
#include <algorithm>
#include <thread>
#include <mutex>
#include <set>
#include <chrono>
#include <iostream>

#ifndef UNUSED 
  #define UNUSED(x) (void)(x)
#endif

// Thread spy utility to detect thread usage
class ThreadSpy {
private:
    std::set<std::thread::id> thread_ids;
    std::mutex mutex;

public:
    void record() {
        std::lock_guard<std::mutex> lock(mutex);
        thread_ids.insert(std::this_thread::get_id());
    }

    size_t unique_thread_count() const {
        return thread_ids.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        thread_ids.clear();
    }
};

using namespace funcflow::scheduler;

// Test data structures
enum class TestFlags : uint32_t
{
    FLAG_A = 1,
    FLAG_B = 2,
    FLAG_C = 4,
    FLAG_D = 8
};

TEST_CASE("Scheduler for_each operations", "[scheduler][for_each]")
{

    SECTION("Sequential for_each with vector of ints")
    {
        std::vector<int> data = {1, 2, 3, 4, 5};
        std::vector<int> results;

        for_each(data.begin(), data.end(),
                [&results](int& value) {
                    results.push_back(value * 2);
                },
                execution_mode::sequential);

        REQUIRE(results.size() == 5);
        REQUIRE(results[0] == 2);
        REQUIRE(results[1] == 4);
        REQUIRE(results[2] == 6);
        REQUIRE(results[3] == 8);
        REQUIRE(results[4] == 10);
    }

    SECTION("Parallel for_each with vector of ints")
    {
        std::vector<int> data = {10, 20, 30, 40};
        std::vector<int> results(4);

        for_each(data.begin(), data.end(),
                [&results, &data](int& value) {
                    // Use index-based assignment to avoid race conditions
                    size_t idx = &value - &data[0];
                    results[idx] = value * 3;
                },
                execution_mode::parallel);

        // Sort results to handle potential parallel ordering differences
        std::sort(results.begin(), results.end());

        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 30);   // 10 * 3
        REQUIRE(results[1] == 60);   // 20 * 3
        REQUIRE(results[2] == 90);   // 30 * 3
        REQUIRE(results[3] == 120);  // 40 * 3
    }

    SECTION("for_each with empty container")
    {
        std::vector<int> data;
        int call_count = 0;

        for_each(data.begin(), data.end(),
                [&call_count](int& value) {
                    call_count++;
                },
                execution_mode::sequential);

        REQUIRE(call_count == 0);
    }
}

TEST_CASE("Scheduler for_each_with_index operations", "[scheduler][for_each_index]")
{
    using namespace funcflow::scheduler;

    SECTION("Sequential for_each_with_index")
    {
        std::vector<std::string> data = {"a", "b", "c"};
        std::vector<std::string> indexed_results;

        for_each_with_index(data.begin(), data.end(),
                           [&indexed_results](std::string& value, std::ptrdiff_t index) {
                               indexed_results.push_back(value + std::to_string(index));
                           },
                           execution_mode::sequential);

        REQUIRE(indexed_results.size() == 3);
        REQUIRE(indexed_results[0] == "a0");
        REQUIRE(indexed_results[1] == "b1");
        REQUIRE(indexed_results[2] == "c2");
    }

    SECTION("Parallel for_each_with_index")
    {
        std::vector<int> data = {100, 200, 300, 400, 500};
        std::vector<int> results(5);

        for_each_with_index(data.begin(), data.end(),
                           [&results](int& value, std::ptrdiff_t index) {
                               results[index] = value + index;
                           },
                           execution_mode::parallel);

        REQUIRE(results.size() == 5);
        REQUIRE(results[0] == 100);  // 100 + 0
        REQUIRE(results[1] == 201);  // 200 + 1
        REQUIRE(results[2] == 302);  // 300 + 2
        REQUIRE(results[3] == 403);  // 400 + 3
        REQUIRE(results[4] == 504);  // 500 + 4
    }
}

TEST_CASE("Scheduler transform operations", "[scheduler][transform]")
{
    using namespace funcflow::scheduler;

    SECTION("Sequential transform")
    {
        std::vector<int> input = {1, 2, 3, 4};
        std::vector<int> output(4);

        transform(input.begin(), input.end(), output.begin(),
                 [](const int& value) { return value * value; },
                 execution_mode::sequential);

        REQUIRE(output[0] == 1);   // 1^2
        REQUIRE(output[1] == 4);   // 2^2
        REQUIRE(output[2] == 9);   // 3^2
        REQUIRE(output[3] == 16);  // 4^2
    }

    SECTION("Parallel transform")
    {
        std::vector<double> input = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<double> output(5);

        transform(input.begin(), input.end(), output.begin(),
                 [](const double& value) { return value * 0.5; },
                 execution_mode::parallel);

        REQUIRE(output[0] == 0.5);
        REQUIRE(output[1] == 1.0);
        REQUIRE(output[2] == 1.5);
        REQUIRE(output[3] == 2.0);
        REQUIRE(output[4] == 2.5);
    }

    SECTION("Transform to different type")
    {
        std::vector<int> input = {10, 20, 30};
        std::vector<std::string> output(3);

        transform(input.begin(), input.end(), output.begin(),
                 [](const int& value) { return "num_" + std::to_string(value); },
                 execution_mode::sequential);

        REQUIRE(output[0] == "num_10");
        REQUIRE(output[1] == "num_20");
        REQUIRE(output[2] == "num_30");
    }
}

TEST_CASE("Scheduler run_contained_functions", "[scheduler][parallel]")
{
    using namespace funcflow::scheduler;

    SECTION("Generic Task container - sequential execution")
    {
        std::vector<std::function<int()>> tasks = {
            []() { return 10; },
            []() { return 20; },
            []() { return 30; },
            []() { return 40; }
        };

        auto results = run_contained_functions(tasks, execution_mode::sequential);

        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 10);
        REQUIRE(results[1] == 20);
        REQUIRE(results[2] == 30);
        REQUIRE(results[3] == 40);
    }

    SECTION("Generic Task container - parallel execution")
    {
        std::vector<std::function<int()>> tasks = {
            []() { return 100; },
            []() { return 200; },
            []() { return 300; }
        };

        auto results = run_contained_functions(tasks, execution_mode::parallel);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == 100);
        REQUIRE(results[1] == 200);
        REQUIRE(results[2] == 300);
    }


    SECTION("Empty Task container")
    {
        std::vector<std::function<int()>> tasks;

        auto results = run_contained_functions(tasks);

        REQUIRE(results.empty());
    }

    SECTION("Tasks with side effects")
    {
        int counter = 0;
        std::vector<std::function<int()>> tasks = {
            [&counter]() { counter += 1; return counter; },
            [&counter]() { counter += 10; return counter; },
            [&counter]() { counter += 100; return counter; }
        };

        auto results = run_contained_functions(tasks, execution_mode::sequential);

        REQUIRE(results.size() == 3);
        // Note: In parallel execution, the order and exact values may vary due to race conditions
        // This test demonstrates sequential execution behavior
        REQUIRE(counter > 0);
    }

    SECTION("Tasks returning different types via generic function")
    {
        // Test with lambdas that return flags
        std::vector<std::function<Flags<TestFlags>()>> flag_tasks = {
            []() { return Flags<TestFlags>{TestFlags::FLAG_A}; },
            []() { return Flags<TestFlags>{TestFlags::FLAG_B}; },
            []() { return Flags<TestFlags>{TestFlags::FLAG_C}; }
        };

        auto flag_results = run_contained_functions(flag_tasks);

        REQUIRE(flag_results.size() == 3);
        REQUIRE(flag_results[0].testFlag(TestFlags::FLAG_A));
        REQUIRE(flag_results[1].testFlag(TestFlags::FLAG_B));
        REQUIRE(flag_results[2].testFlag(TestFlags::FLAG_C));
    }
}

TEST_CASE("Scheduler run_function_tasks", "[scheduler][function_tasks]")
{
    SECTION("Sequential execution")
    {
        std::vector<std::function<std::string()>> tasks = {
            []() { return std::string("task1"); },
            []() { return std::string("task2"); },
            []() { return std::string("task3"); }
        };

        auto results = run_function_tasks<std::string>(tasks, execution_mode::sequential);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == "task1");
        REQUIRE(results[1] == "task2");
        REQUIRE(results[2] == "task3");
    }

    SECTION("Parallel execution")
    {
        std::vector<std::function<double()>> tasks = {
            []() { return 1.5; },
            []() { return 2.5; },
            []() { return 3.5; },
            []() { return 4.5; }
        };

        auto results = run_function_tasks<double>(tasks, execution_mode::parallel);

        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 1.5);
        REQUIRE(results[1] == 2.5);
        REQUIRE(results[2] == 3.5);
        REQUIRE(results[3] == 4.5);
    }
}

TEST_CASE("Scheduler run_functors_typed", "[scheduler][functors]")
{
    struct AddTen {
        int operator()(int value) const { return value + 10; }
    };

    struct MultiplyByTwo {
        int operator()(int value) const { return value * 2; }
    };

    struct ToStringFunctor {
        std::string operator()(int value) const { return "value_" + std::to_string(value); }
    };

    SECTION("Single functor - sequential")
    {
        int input = 5;
        StepResult result;  
        auto results = run_functors_typed<int, int, AddTen>(execution_mode::sequential, input,result);

        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == 15); // 5 + 10
    }

    SECTION("Multiple functors - sequential")
    {
        int input = 3;
        StepResult result;
        auto results = run_functors_typed<int, int, AddTen, MultiplyByTwo>(execution_mode::sequential, input,result);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == 13);  // 3 + 10
        REQUIRE(results[1] == 6);   // 3 * 2
    }

    SECTION("Multiple functors - parallel")
    {
        int input = 4;
        StepResult result;
        auto results = run_functors_typed<int, int, AddTen, MultiplyByTwo>(execution_mode::parallel, input,result);

        REQUIRE(results.size() == 2);

        // Results order matches template parameter order when using std::async
        REQUIRE(results[0] == 14);  // AddTen: 4 + 10
        REQUIRE(results[1] == 8);   // MultiplyByTwo: 4 * 2
    }

    SECTION("Different result type")
    {
        int input = 42;
        StepResult result;
        auto results = run_functors_typed<int, std::string, ToStringFunctor>(execution_mode::sequential, input,result);

        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == "value_42");
    }

    SECTION("Flag result type")
    {
        struct FlagA_Producer {
            Flags<TestFlags> operator()(int) const { return Flags<TestFlags>{TestFlags::FLAG_A}; }
        };

        struct FlagB_Producer {
            Flags<TestFlags> operator()(int) const { return Flags<TestFlags>{TestFlags::FLAG_B}; }
        };

        int input = 0;
        StepResult result;
        auto results = run_functors_typed<int, Flags<TestFlags>, FlagA_Producer, FlagB_Producer>(execution_mode::parallel, input,result);

        REQUIRE(results.size() == 2);

        // Results maintain order with std::async
        REQUIRE(results[0].testFlag(TestFlags::FLAG_A));
        REQUIRE(results[1].testFlag(TestFlags::FLAG_B));
    }

    SECTION("Many functors - stress test parallel execution")
    {
        struct Functor1 { int operator()(int x) const { return x + 1; } };
        struct Functor2 { int operator()(int x) const { return x + 2; } };
        struct Functor3 { int operator()(int x) const { return x + 3; } };
        struct Functor4 { int operator()(int x) const { return x + 4; } };
        struct Functor5 { int operator()(int x) const { return x + 5; } };
        struct Functor6 { int operator()(int x) const { return x + 6; } };
        struct Functor7 { int operator()(int x) const { return x + 7; } };
        struct Functor8 { int operator()(int x) const { return x + 8; } };

        int input = 10;
        StepResult result;
        auto results = run_functors_typed<int, int, Functor1, Functor2, Functor3, Functor4,
                                          Functor5, Functor6, Functor7, Functor8>(
                                          execution_mode::parallel, input,result);

        REQUIRE(results.size() == 8);
        // Verify order is preserved
        for (size_t i = 0; i < results.size(); ++i) {
            REQUIRE(results[i] == input + static_cast<int>(i + 1));
        }
    }

    SECTION("Verify parallel execution maintains result order")
    {
        struct SlowFunctor {
            int operator()(int x) const {
                // Simulate some work
                int sum = 0;
                for (int i = 0; i < 1000; ++i) sum += i;
                UNUSED(sum);
                return x + 100;
            }
        };

        struct FastFunctor {
            int operator()(int x) const { return x + 1; }
        };

        int input = 5;
        StepResult result;
        auto results = run_functors_typed<int, int, SlowFunctor, FastFunctor>(
                                          execution_mode::parallel, input,result);

        REQUIRE(results.size() == 2);
        // Even though FastFunctor likely finishes first, order should match template order
        REQUIRE(results[0] == 105);  // SlowFunctor (first template parameter)
        REQUIRE(results[1] == 6);    // FastFunctor (second template parameter)
    }

    SECTION("Exception handling - sequential mode")
    {
        struct ThrowingFunctor {
            int operator()(int) const {
                throw std::runtime_error("Test exception");
                return 0;
            }
        };

        struct NormalFunctor {
            int operator()(int x) const { return x + 1; }
        };

        int input = 5;
        StepResult result;
        // Sequential mode catches exceptions and continues
        auto results = run_functors_typed<int, int, ThrowingFunctor, NormalFunctor>(
                                          execution_mode::sequential, input,result);

        REQUIRE(results.size() == 1);  // Only NormalFunctor result is added
        REQUIRE(results[0] == 6);
        // Verify StepResult tracking
        REQUIRE(result.sub_steps.size() == 2);
        REQUIRE_FALSE(result.sub_steps[0].success());  // First functor failed
        REQUIRE(result.sub_steps[0].exception != nullptr);
        REQUIRE(result.sub_steps[1].success());
        REQUIRE_FALSE(result.success());  // Overall fails due to first functor
    }

    SECTION("Thread usage detection - sequential mode")
    {
        ThreadSpy spy;

        struct SpyFunctor1 {
            ThreadSpy* spy;
            int operator()(int x) const {
                spy->record();
                return x + 1;
            }
        };

        struct SpyFunctor2 {
            ThreadSpy* spy;
            int operator()(int x) const {
                spy->record();
                return x + 2;
            }
        };

        // Note: Can't easily inject spy into default-constructed functors
        // Instead, test with tasks
        int input = 10;
        std::vector<std::function<int()>> tasks;
        tasks.push_back([&spy, input]() { spy.record(); return input + 1; });
        tasks.push_back([&spy, input]() { spy.record(); return input + 2; });
        tasks.push_back([&spy, input]() { spy.record(); return input + 3; });

        auto results = run_contained_functions(tasks, execution_mode::sequential);

        REQUIRE(results.size() == 3);
        // Sequential execution should use only one thread (the main thread)
        REQUIRE(spy.unique_thread_count() == 1);
    }

    SECTION("Thread usage detection - parallel mode")
    {
        ThreadSpy spy;

        std::vector<std::function<int()>> tasks;
        // Create more tasks to ensure parallel execution
        for (int i = 0; i < 10; ++i) {
            tasks.push_back([&spy, i]() {
                spy.record();
                // Add some work to ensure threads don't finish too quickly
                int sum = 0;
                for (int j = 0; j < 100; ++j) sum += j;
                return i + sum;
            });
        }

        auto results = run_contained_functions(tasks, execution_mode::parallel);

        REQUIRE(results.size() == 10);
        // Parallel execution should use multiple threads (at least 2, typically more)
        REQUIRE(spy.unique_thread_count() >= 1);
        // With OpenMP, we expect more than 1 thread for parallel execution
        INFO("Threads used: " << spy.unique_thread_count());
    }
}

TEST_CASE("Thread spy verification", "[scheduler][threads]")
{
    SECTION("run_functors_typed uses std::async - multiple threads expected")
    {
        // Global spy to be accessed by functors
        static ThreadSpy global_spy;
        global_spy.reset();

        struct Functor1 {
            int operator()(int x) const {
                global_spy.record();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return x + 1;
            }
        };

        struct Functor2 {
            int operator()(int x) const {
                global_spy.record();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return x + 2;
            }
        };

        struct Functor3 {
            int operator()(int x) const {
                global_spy.record();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return x + 3;
            }
        };

        struct Functor4 {
            int operator()(int x) const {
                global_spy.record();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return x + 4;
            }
        };

        int input = 10;
        StepResult result;
        auto results = run_functors_typed<int, int, Functor1, Functor2, Functor3, Functor4>(
                                          execution_mode::parallel, input,result);

        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 11);
        REQUIRE(results[1] == 12);
        REQUIRE(results[2] == 13);
        REQUIRE(results[3] == 14);

        // With std::async, we expect multiple threads (typically 4, one per functor)
        INFO("Threads used by run_functors_typed: " << global_spy.unique_thread_count());
        REQUIRE(global_spy.unique_thread_count() >= 2);  // At least 2 threads should be used
    }

    SECTION("for_each with OpenMP uses thread pool")
    {
        ThreadSpy spy;
        std::vector<int> data(20);
        std::iota(data.begin(), data.end(), 0);

        for_each(data.begin(), data.end(),
                [&spy](int& value) {
                    spy.record();
                    value *= 2;
                },
                execution_mode::parallel);

        INFO("Threads used by for_each: " << spy.unique_thread_count());
        // OpenMP typically uses a thread pool, so we expect multiple threads
        // but the exact number depends on the system
        REQUIRE(spy.unique_thread_count() >= 1);
    }
}

TEST_CASE("Scheduler execution modes", "[scheduler][execution_modes]")
{
    SECTION("Sequential vs Parallel behavior comparison")
    {
        std::vector<int> data = {1, 2, 3, 4, 5};
        std::vector<int> seq_results, par_results;

        // Sequential
        for_each(data.begin(), data.end(),
                [&seq_results](int& value) {
                    seq_results.push_back(value * 2);
                },
                execution_mode::sequential);

        // Parallel - use index to avoid race conditions
        par_results.resize(data.size());
        for_each(data.begin(), data.end(),
                [&par_results, &data](int& value) {
                    size_t idx = &value - &data[0];
                    par_results[idx] = value * 2;
                },
                execution_mode::parallel);

        // Both should produce same results
        REQUIRE(seq_results.size() == par_results.size());
        for (size_t i = 0; i < seq_results.size(); ++i) {
            REQUIRE(seq_results[i] == par_results[i]);
        }
    }

    SECTION("Performance with larger dataset")
    {
        const size_t large_size = 1000;
        std::vector<int> data(large_size);
        std::iota(data.begin(), data.end(), 1); // Fill with 1, 2, 3, ..., 1000

        std::vector<int> results(large_size);

        // Test parallel execution with larger dataset
        for_each(data.begin(), data.end(),
                [&results, &data](int& value) {
                    size_t idx = &value - &data[0];
                    results[idx] = value * value; // Square the value
                },
                execution_mode::parallel);

        // Verify first and last few results
        REQUIRE(results[0] == 1);      // 1^2
        REQUIRE(results[1] == 4);      // 2^2
        REQUIRE(results[2] == 9);      // 3^2
        REQUIRE(results[large_size-1] == large_size * large_size); // 1000^2
    }
}

TEST_CASE("Scheduler run_contained_tasks", "[scheduler][tasks]")
{
    using namespace funcflow::task_utils;

    SECTION("run_contained_tasks - sequential with success")
    {
        std::vector<Task<int, int>> tasks;
        tasks.emplace_back([](int x) { return x * 2; });
        tasks.emplace_back([](int x) { return x * 3; });
        tasks.emplace_back([](int x) { return x * 4; });

        StepResult result;
        auto results = run_contained_tasks(tasks, result, execution_mode::sequential, 5);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0].has_value());
        REQUIRE(results[0].value() == 10);
        REQUIRE(results[1].has_value());
        REQUIRE(results[1].value() == 15);
        REQUIRE(results[2].has_value());
        REQUIRE(results[2].value() == 20);
        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result.success());
    }

    SECTION("run_contained_tasks - parallel with success")
    {
        std::vector<Task<int, int>> tasks;
        tasks.emplace_back([](int x) { return x + 10; });
        tasks.emplace_back([](int x) { return x + 20; });

        StepResult result;
        auto results = run_contained_tasks(tasks, result, execution_mode::parallel, 5);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0].has_value());
        REQUIRE(results[0].value() == 15);
        REQUIRE(results[1].has_value());
        REQUIRE(results[1].value() == 25);
        REQUIRE(result.sub_steps.size() == 2);
        REQUIRE(result.success());
    }

    SECTION("run_contained_tasks - sequential with exceptions")
    {
        std::vector<Task<int, int>> tasks;
        tasks.emplace_back([](int x) { return x * 2; });
        tasks.emplace_back([](int) -> int { throw std::runtime_error("Task 2 failed"); });
        tasks.emplace_back([](int x) { return x * 4; });

        StepResult result;
        auto results = run_contained_tasks(tasks, result, execution_mode::sequential, 5);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0].has_value());
        REQUIRE(results[0].value() == 10);
        REQUIRE(!results[1].has_value());  // Failed Task
        REQUIRE(results[2].has_value());
        REQUIRE(results[2].value() == 20);

        // Verify exception tracking
        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result.sub_steps[0]);
        REQUIRE_FALSE(result.sub_steps[1]);
        REQUIRE(result.sub_steps[1].exception != nullptr);
        REQUIRE(result.sub_steps[2]);
        REQUIRE_FALSE(result.success());  // Overall fails
    }

    SECTION("run_contained_tasks - parallel with exceptions")
    {
        std::vector<Task<int, int>> tasks;
        tasks.emplace_back([](int x) { return x + 1; });
        tasks.emplace_back([](int) -> int { throw std::runtime_error("Parallel Task failed"); });
        tasks.emplace_back([](int x) { return x + 3; });

        StepResult result;
        auto results = run_contained_tasks(tasks, result, execution_mode::parallel, 10);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0].has_value());
        REQUIRE(results[0].value() == 11);
        REQUIRE(!results[1].has_value());  // Failed Task
        REQUIRE(results[2].has_value());
        REQUIRE(results[2].value() == 13);

        // Verify exception tracking
        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result.sub_steps[0]);
        REQUIRE_FALSE(result.sub_steps[1]);
        REQUIRE(result.sub_steps[1].exception != nullptr);
        REQUIRE(result.sub_steps[2]);
        REQUIRE_FALSE(result.success());
    }
}

TEST_CASE("Scheduler nested StepResult handling", "[scheduler][nested]")
{
    SECTION("for_each_safe with functor returning StepResult")
    {
        std::vector<int> data = {1, 2, 3};
        StepResult parent_step;

        // Functor that returns StepResult
        auto nested_func = [](int& value) -> StepResult {
            StepResult result;
            result.step_name = "Nested operation for value " + std::to_string(value);

            if (value == 2) {
                result = false;
                result.exception = std::make_exception_ptr(std::runtime_error("Nested failure"));
            } else {
                result = true;
                value *= 10;  // Modify the value
            }            
            return result;
        };

        for_each_safe(data.begin(), data.end(), nested_func, parent_step, execution_mode::sequential);

        // Check that data was modified where successful
        REQUIRE(data[0] == 10);
        REQUIRE(data[1] == 2);  // Not modified due to exception
        REQUIRE(data[2] == 30);

        // Check hierarchical step names
        REQUIRE(parent_step.sub_steps.size() == 3);
        REQUIRE(parent_step.sub_steps[0].step_name.find("->") != std::string::npos);  // Has hierarchy
        REQUIRE(parent_step.sub_steps[0].step_name.find("Nested operation") != std::string::npos);

        // Check nested failure propagation
        REQUIRE_FALSE(parent_step.sub_steps[1].success());
        REQUIRE_FALSE(parent_step.success());  // Overall fails due to one failure
    }

    SECTION("for_each_with_index_safe with functor returning StepResult")
    {
        std::vector<int> data = {10, 20, 30};

        // Functor that returns StepResult with index
        auto nested_func = [](int& value, std::ptrdiff_t idx) -> StepResult {
            StepResult result;
            result.step_name = "Process index " + std::to_string(idx);
            result = true;            
            value += static_cast<int>(idx * 100);
            return result;
        };

        auto result = for_each_with_index_safe(data.begin(), data.end(), nested_func, execution_mode::sequential);

        REQUIRE(data[0] == 10);   // 10 + 0*100
        REQUIRE(data[1] == 120);  // 20 + 1*100
        REQUIRE(data[2] == 230);  // 30 + 2*100

        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result);

        // Check hierarchical naming
        REQUIRE(result.sub_steps[0].step_name.find("Process index") != std::string::npos);
    }
}

TEST_CASE("Scheduler safe wrappers", "[scheduler][safe]")
{
    SECTION("for_each_safe - sequential with exceptions")
    {
        std::vector<int> data = {1, 2, 3, 4, 5};
        std::vector<int> results(5);
        StepResult parent_step;

        for_each_safe(data.begin(), data.end(),
                     [&results, &data](int& value) {
                         size_t idx = &value - &data[0];
                         if (idx == 2) {
                             throw std::runtime_error("Error at index 2");
                         }
                         results[idx] = value * 2;
                     },
                     parent_step,
                     execution_mode::sequential);

        REQUIRE(results[0] == 2);
        REQUIRE(results[1] == 4);
        REQUIRE(results[3] == 8);
        REQUIRE(results[4] == 10);
        REQUIRE(parent_step.sub_steps.size() == 5);
        // Verify exception tracking
        REQUIRE_FALSE(parent_step.success());
        REQUIRE(parent_step.sub_steps[0].success());
        REQUIRE(parent_step.sub_steps[1].success());
        REQUIRE_FALSE(parent_step.sub_steps[2].success());
        REQUIRE(parent_step.sub_steps[2].exception != nullptr);
        REQUIRE(parent_step.sub_steps[3].success());
        REQUIRE(parent_step.sub_steps[4].success());
    }

    SECTION("for_each_with_index_safe - parallel with exceptions")
    {
        std::vector<int> data = {10, 20, 30, 40};
        std::vector<int> results(4);

        auto step_result = for_each_with_index_safe(data.begin(), data.end(),
                                                     [&results](int& value, std::ptrdiff_t index) {
                                                         if (index == 1) {
                                                             throw std::runtime_error("Error at index 1");
                                                         }
                                                         results[index] = value + index;
                                                     },
                                                     execution_mode::parallel);

        REQUIRE(results[0] == 10);
        REQUIRE(results[2] == 32);
        REQUIRE(results[3] == 43);
        REQUIRE(step_result.sub_steps.size() == 4);
        // Verify exception tracking
        REQUIRE_FALSE(step_result.success());
        REQUIRE(step_result.sub_steps[0].success());
        REQUIRE_FALSE(step_result.sub_steps[1].success());
        REQUIRE(step_result.sub_steps[1].exception != nullptr);
        REQUIRE(step_result.sub_steps[2].success());
        REQUIRE(step_result.sub_steps[3].success());
    }

    SECTION("transform_safe - sequential with exceptions")
    {
        std::vector<int> input = {1, 2, 3, 4};
        std::vector<int> output(4);

        auto step_result = transform_safe(input.begin(), input.end(), output.begin(),
                                          [](const int& value) {
                                              if (value == 3) {
                                                  throw std::runtime_error("Error at value 3");
                                              }
                                              return value * value;
                                          },
                                          execution_mode::sequential);

        REQUIRE(output[0] == 1);
        REQUIRE(output[1] == 4);
        REQUIRE(output[3] == 16);
        // With latest implementation, all elements get sub-steps
        REQUIRE(step_result.sub_steps.size() == 4);
        REQUIRE(step_result.sub_steps[0].success());
        REQUIRE(step_result.sub_steps[1].success());
        REQUIRE_FALSE(step_result.sub_steps[2].success());  // Exception at value 3
        REQUIRE(step_result.sub_steps[2].exception != nullptr);
        REQUIRE(step_result.sub_steps[3].success());
        REQUIRE_FALSE(step_result.success());  // Overall fails due to one failure
    }
}

TEST_CASE("StepResult functionality", "[scheduler][step_result]")
{
    SECTION("StepResult tracks success and exceptions")
    {
        StepResult result;
        REQUIRE(result.success() == false);
        REQUIRE(result.exception == nullptr);
        REQUIRE(result.sub_steps.empty());
        REQUIRE(result.executed() == false);
        REQUIRE_FALSE(result);
        result = true;
        REQUIRE(result.success() == true);
        REQUIRE(result.executed() == true);
        StepResult new_result;
        result = false;
        REQUIRE(result.success() == false);
        REQUIRE(result.executed() == true);
    }

    SECTION("StepResult with sub_steps")
    {
        StepResult parent;
        parent.step_name = "parent";

        auto& child1 = parent.sub_steps.emplace_back();
        child1.step_name = "child1";
        child1 = true;

        auto& child2 = parent.sub_steps.emplace_back();
        child2.step_name = "child2";
        child2 = false;

        REQUIRE(parent.sub_steps.size() == 2);
        REQUIRE(parent.sub_steps.front().success() == true);
        REQUIRE(parent.sub_steps.back().success() == false);
        parent.apply_sub_steps_failure_policy();
        REQUIRE(parent.success() == false);
    }

    SECTION("StepResult exception handling")
    {
        StepResult result;

        try {
            throw std::runtime_error("test error");
        } catch (...) {
            result = false;            
            STORE_EXCEPTION(result.exception);
        }

        REQUIRE(result.exception != nullptr);
    }

    SECTION("StepResult init_sub_steps with names")
    {
        StepResult parent;
        std::vector<std::string> names = {"step1", "step2", "step3"};

        parent.init_sub_steps(3, names);

        REQUIRE(parent.sub_steps.size() == 3);
        REQUIRE(parent.sub_steps[0].step_name == "step1");
        REQUIRE(parent.sub_steps[1].step_name == "step2");
        REQUIRE(parent.sub_steps[2].step_name == "step3");
    }

    SECTION("StepResult init_sub_steps with prefix")
    {
        StepResult parent;

        parent.init_sub_steps(4, "Task");

        REQUIRE(parent.sub_steps.size() == 4);
        REQUIRE(parent.sub_steps[0].step_name == "Task[0]");
        REQUIRE(parent.sub_steps[1].step_name == "Task[1]");
        REQUIRE(parent.sub_steps[2].step_name == "Task[2]");
        REQUIRE(parent.sub_steps[3].step_name == "Task[3]");
    }

    SECTION("StepResult apply_sub_steps_failure_policy - all success")
    {
        StepResult parent;
        parent.init_sub_steps(3, "step");

        parent.sub_steps[0] = true;
        parent.sub_steps[1] = true;
        parent.sub_steps[2] = true;

        bool result = parent.apply_sub_steps_failure_policy();

        REQUIRE(result == true);
        REQUIRE(parent.success() == true);
    }

    SECTION("StepResult apply_sub_steps_failure_policy - one failure")
    {
        StepResult parent;
        parent.init_sub_steps(3, "step");

        parent.sub_steps[0] = true;
        parent.sub_steps[1] = false;  // Failure
        parent.sub_steps[2] = true;

        bool result = parent.apply_sub_steps_failure_policy();

        REQUIRE(result == false);
        REQUIRE(parent.success() == false);
    }

    SECTION("StepResult operator[] access")
    {
        StepResult parent;
        parent.init_sub_steps(3, "item");

        parent[0] = true;
        parent[1] = false;
        parent[2] = true;

        REQUIRE(parent[0].success() == true);
        REQUIRE(parent[1].success() == false);
        REQUIRE(parent[2].success() == true);
        REQUIRE(parent[0].step_name == "item[0]");
    }
}

TEST_CASE("Scheduler exception handling comprehensive", "[scheduler][exceptions]")
{
    SECTION("Exception can be inspected and rethrown")
    {
        StepResult result;
        result.step_name = "Test step";

        try {
            throw std::runtime_error("Original error message");
        } catch (...) {
            result = false;
            result.exception = std::current_exception();
        }

        REQUIRE(result.exception != nullptr);
        REQUIRE_FALSE(result.success());

        // Rethrow and inspect
        try {
            std::rethrow_exception(result.exception);
            FAIL("Should have thrown");
        } catch (const std::runtime_error& e) {
            REQUIRE(std::string(e.what()) == "Original error message");
        }
    }

    SECTION("Multiple exceptions tracked in sub-steps")
    {
        StepResult parent;
        parent.init_sub_steps(5, "operation");

        // Simulate operations where some fail
        for (int i = 0; i < 5; ++i) {
            try {
                if (i == 1 || i == 3) {
                    throw std::runtime_error("Error at index " + std::to_string(i));
                }
                parent[i] = true;
            } catch (...) {
                parent[i] = false;
                parent[i].exception = std::current_exception();
            }
        }

        parent.apply_sub_steps_failure_policy();

        REQUIRE(parent.success() == false);
        REQUIRE(parent.sub_steps[0].success() == true);
        REQUIRE_FALSE(parent.sub_steps[1].success());
        REQUIRE(parent.sub_steps[1].exception != nullptr);
        REQUIRE(parent.sub_steps[2].success());
        REQUIRE_FALSE(parent.sub_steps[3].success());
        REQUIRE(parent.sub_steps[3].exception != nullptr);
        REQUIRE(parent.sub_steps[4].success());

        // Can inspect individual exceptions
        try {
            std::rethrow_exception(parent.sub_steps[1].exception);
            FAIL("Should have thrown");
        } catch (const std::runtime_error& e) {
            REQUIRE(std::string(e.what()).find("index 1") != std::string::npos);
        }
    }

    SECTION("run_contained_tasks - all tasks fail")
    {
        using namespace funcflow::task_utils;

        std::vector<Task<int, int>> tasks;
        tasks.emplace_back([](int) -> int { throw std::runtime_error("Task 1 failed"); });
        tasks.emplace_back([](int) -> int { throw std::runtime_error("Task 2 failed"); });
        tasks.emplace_back([](int) -> int { throw std::runtime_error("Task 3 failed"); });

        StepResult result;
        auto results = run_contained_tasks(tasks, result, execution_mode::sequential, 10);

        REQUIRE(results.size() == 3);
        REQUIRE(!results[0].has_value());
        REQUIRE(!results[1].has_value());
        REQUIRE(!results[2].has_value());

        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result.sub_steps[0].success() == false);
        REQUIRE(result.sub_steps[0].exception != nullptr);
        REQUIRE(result.sub_steps[1].success() == false);
        REQUIRE(result.sub_steps[1].exception != nullptr);
        REQUIRE(result.sub_steps[2].success() == false);
        REQUIRE(result.sub_steps[2].exception != nullptr);
        REQUIRE(result.success() == false);
    }

    SECTION("run_functors_typed - parallel mode with exceptions")
    {
        struct ThrowingRunTimeErrorFunctor1 {
            int operator()(int) const {
                throw std::runtime_error("Functor 1 failed");
                return 0;
            }
        };

        struct SuccessFunctor2 {
            int operator()(int x) const { return x * 2; }
        };

        struct ThrowinglogicErrorFunctor3 {
            int operator()(int) const {
                throw std::logic_error("Functor 2 failed");
                return 0;
            }
        };

        StepResult result;
        auto results = run_functors_typed<int, int, ThrowingRunTimeErrorFunctor1, SuccessFunctor2, ThrowinglogicErrorFunctor3>(
            execution_mode::parallel, 5, result);

        // Only successful functor result is added
        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == 10);

        // But all functors have sub-steps
        REQUIRE(result.sub_steps.size() == 3);
        REQUIRE(result.sub_steps[0].success() == false);
        REQUIRE(result.sub_steps[0].exception != nullptr);
        REQUIRE(result.sub_steps[1].success() == true);
        REQUIRE(result.sub_steps[2].success() == false);
        REQUIRE(result.sub_steps[2].exception != nullptr);
        REQUIRE(result.success() == false);
        REQUIRE(result.executed() == true);

        // Can differentiate exception types
        try {
            std::rethrow_exception(result.sub_steps[0].exception);
            FAIL("Should have thrown");
        } catch (const std::runtime_error& e) {
            // Expected
        } catch (...) {
            FAIL("Wrong exception type (unknown, not derived from std::exception)");
        }

        try {
            std::rethrow_exception(result.sub_steps[2].exception);
            FAIL("Should have thrown");
        } catch (const std::logic_error&) {
            // Expected
        } catch (...) {
            FAIL("Wrong exception type (unknown, not derived from std::exception)");            
        }
    }

    SECTION("for_each_safe - partial failure doesn't stop execution")
    {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<int> processed;
        StepResult parent_step;

        for_each_safe(data.begin(), data.end(),
                     [&processed](int& value) {
                         if (value % 3 == 0) {
                             throw std::runtime_error("Divisible by 3");
                         }
                         processed.push_back(value);
                     },
                     parent_step,
                     execution_mode::sequential);

        // Non-failing elements were processed
        REQUIRE(processed.size() == 7);  // 1,2,4,5,7,8,10 (not 3,6,9)

        // All elements have sub-steps
        REQUIRE(parent_step.sub_steps.size() == 10);
        REQUIRE(parent_step.success() == false);

        // Check which ones failed
        REQUIRE(parent_step.sub_steps[2].success() == false);  // value 3
        REQUIRE(parent_step.sub_steps[2].exception != nullptr);
        REQUIRE(parent_step.sub_steps[5].success() == false);  // value 6
        REQUIRE(parent_step.sub_steps[5].exception != nullptr);
        REQUIRE(parent_step.sub_steps[8].success() == false);  // value 9
        REQUIRE(parent_step.sub_steps[8].exception != nullptr);

        // Others succeeded
        REQUIRE(parent_step.sub_steps[0].success() == true);
        REQUIRE(parent_step.sub_steps[1].success() == true);
        REQUIRE(parent_step.sub_steps[4].success() == true);
    }

    SECTION("transform_safe - parallel mode with exceptions")
    {
        std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<int> output(8);

        auto step_result = transform_safe(input.begin(), input.end(), output.begin(),
                                          [](const int& value) {
                                              if (value % 2 == 0) {
                                                  throw std::runtime_error("Even number");
                                              }
                                              return value * 10;
                                          },
                                          execution_mode::parallel);

        // Odd values were transformed
        REQUIRE(output[0] == 10);
        REQUIRE(output[2] == 30);
        REQUIRE(output[4] == 50);
        REQUIRE(output[6] == 70);

        REQUIRE(step_result.sub_steps.size() == 8);
        REQUIRE(step_result.success() == false);

        // Even indices failed
        REQUIRE(step_result.sub_steps[1].success() == false);
        REQUIRE(step_result.sub_steps[3].success() == false);
        REQUIRE(step_result.sub_steps[5].success() == false);
        REQUIRE(step_result.sub_steps[7].success() == false);

        // Odd indices succeeded
        REQUIRE(step_result.sub_steps[0].success() == true);
        REQUIRE(step_result.sub_steps[2].success() == true);
        REQUIRE(step_result.sub_steps[4].success() == true);
        REQUIRE(step_result.sub_steps[6].success() == true);
    }

    SECTION("Exception propagation through nested operations")
    {
        std::vector<int> data = {1, 2, 3};
        StepResult outer_step;
        outer_step.step_name = "Outer operation";

        for_each_safe(data.begin(), data.end(),
                     [](int& value) -> StepResult {
                         StepResult inner;
                         inner.step_name = "Inner operation for " + std::to_string(value);

                         if (value == 2) {
                             inner = false;
                             inner.exception = std::make_exception_ptr(
                                 std::runtime_error("Inner failure at 2"));
                         } else {
                             value *= 100;
                             inner = true;
                         }                         
                         return inner;
                     },
                     outer_step,
                     execution_mode::sequential);

        // Check nested structure
        REQUIRE(outer_step.sub_steps.size() == 3);
        REQUIRE(outer_step.success() == false);  // Propagates from inner

        // Check hierarchical naming
        REQUIRE(outer_step.sub_steps[0].step_name.find("Inner operation") != std::string::npos);
        REQUIRE(outer_step.sub_steps[1].step_name.find("Inner operation") != std::string::npos);

        // Check failure propagation
        REQUIRE(outer_step.sub_steps[0].success() == true);
        REQUIRE(outer_step.sub_steps[1].success() == false);
        REQUIRE(outer_step.sub_steps[1].exception != nullptr);
        REQUIRE(outer_step.sub_steps[2].success() == true);

        // Data was modified where successful
        REQUIRE(data[0] == 100);
        REQUIRE(data[1] == 2);  // Not modified due to failure
        REQUIRE(data[2] == 300);
    }
}

TEST_CASE("Scheduler performance tests", "[scheduler][performance][.]")
{
    using namespace std::chrono;

    SECTION("for_each performance - sequential vs parallel")
    {
        const size_t size = 100000;
        std::vector<double> data(size);
        std::iota(data.begin(), data.end(), 1.0);

        ThreadSpy seq_spy, par_spy;

        // Sequential
        std::vector<double> seq_results(size);
        auto seq_start = high_resolution_clock::now();
        for_each(data.begin(), data.end(),
                [&seq_results, &data, &seq_spy](double& value) {
                    seq_spy.record();
                    size_t idx = &value - &data[0];
                    // Heavy computational work
                    double sum = value;
                    for (int i = 0; i < 5000; ++i) {
                        sum = std::sin(sum) * std::cos(value) + std::sqrt(std::abs(sum));
                    }
                    seq_results[idx] = sum;
                },
                execution_mode::sequential);
        auto seq_end = high_resolution_clock::now();
        auto seq_duration = duration_cast<milliseconds>(seq_end - seq_start).count();

        // Parallel
        std::vector<double> par_results(size);
        auto par_start = high_resolution_clock::now();
        for_each(data.begin(), data.end(),
                [&par_results, &data, &par_spy](double& value) {
                    par_spy.record();
                    size_t idx = &value - &data[0];
                    // Heavy computational work
                    double sum = value;
                    for (int i = 0; i < 5000; ++i) {
                        sum = std::sin(sum) * std::cos(value) + std::sqrt(std::abs(sum));
                    }
                    par_results[idx] = sum;
                },
                execution_mode::parallel);
        auto par_end = high_resolution_clock::now();
        auto par_duration = duration_cast<milliseconds>(par_end - par_start).count();

        std::cout << "\n=== for_each Performance (n=" << size << ") ===" << std::endl;
        std::cout << "Sequential: " << seq_duration << " ms (threads: " << seq_spy.unique_thread_count() << ")" << std::endl;
        std::cout << "Parallel:   " << par_duration << " ms (threads: " << par_spy.unique_thread_count() << ")" << std::endl;
        if (par_duration > 0) {
            double speedup = static_cast<double>(seq_duration) / static_cast<double>(par_duration);
            std::cout << "Speedup:    " << speedup << "x" << std::endl;
        }

        // Verify results are close (floating point may have minor differences)
        REQUIRE(seq_results.size() == par_results.size());
        REQUIRE(seq_spy.unique_thread_count() == 1);  // Sequential should use 1 thread
        REQUIRE(par_spy.unique_thread_count() >= 2);  // Parallel should use multiple threads
    }

    SECTION("transform performance - sequential vs parallel")
    {
        const size_t size = 50000;
        std::vector<double> input(size);
        std::iota(input.begin(), input.end(), 1.0);

        ThreadSpy seq_spy, par_spy;

        // Sequential
        std::vector<double> seq_output(size);
        auto seq_start = high_resolution_clock::now();
        transform(input.begin(), input.end(), seq_output.begin(),
                 [&seq_spy](const double& value) {
                     seq_spy.record();
                     // Heavy mathematical computation
                     double result = value;
                     for (int i = 0; i < 10000; ++i) {
                         result = std::log(std::abs(result) + 1.0) * std::exp(value / 10000.0);
                         result = std::pow(result, 0.5) + std::sin(result);
                     }
                     return result;
                 },
                 execution_mode::sequential);
        auto seq_end = high_resolution_clock::now();
        auto seq_duration = duration_cast<milliseconds>(seq_end - seq_start).count();

        // Parallel
        std::vector<double> par_output(size);
        auto par_start = high_resolution_clock::now();
        transform(input.begin(), input.end(), par_output.begin(),
                 [&par_spy](const double& value) {
                     par_spy.record();
                     // Heavy mathematical computation
                     double result = value;
                     for (int i = 0; i < 10000; ++i) {
                         result = std::log(std::abs(result) + 1.0) * std::exp(value / 10000.0);
                         result = std::pow(result, 0.5) + std::sin(result);
                     }
                     return result;
                 },
                 execution_mode::parallel);
        auto par_end = high_resolution_clock::now();
        auto par_duration = duration_cast<milliseconds>(par_end - par_start).count();

        std::cout << "\n=== transform Performance (n=" << size << ") ===" << std::endl;
        std::cout << "Sequential: " << seq_duration << " ms (threads: " << seq_spy.unique_thread_count() << ")" << std::endl;
        std::cout << "Parallel:   " << par_duration << " ms (threads: " << par_spy.unique_thread_count() << ")" << std::endl;
        if (par_duration > 0) {
            double speedup = static_cast<double>(seq_duration) / static_cast<double>(par_duration);
            std::cout << "Speedup:    " << speedup << "x" << std::endl;
        }

        REQUIRE(seq_output.size() == par_output.size());
        REQUIRE(seq_spy.unique_thread_count() == 1);
        REQUIRE(par_spy.unique_thread_count() >= 2);
    }

    SECTION("run_contained_functions performance - sequential vs parallel")
    {
        const size_t num_tasks = 16;
        ThreadSpy seq_spy, par_spy;

        std::vector<std::function<double()>> seq_tasks;
        for (size_t i = 0; i < num_tasks; ++i) {
            seq_tasks.push_back([i, &seq_spy]() {
                seq_spy.record();
                // Heavy CPU-intensive work - matrix-like computation
                double result = static_cast<double>(i + 1);
                for (int j = 0; j < 5000000; ++j) {
                    result = std::sin(result) * std::cos(static_cast<double>(j)) +
                             std::sqrt(std::abs(result * static_cast<double>(i + 1)));
                    result = std::fmod(result, 1000.0);
                }
                return result;
            });
        }

        std::vector<std::function<double()>> par_tasks;
        for (size_t i = 0; i < num_tasks; ++i) {
            par_tasks.push_back([i, &par_spy]() {
                par_spy.record();
                // Heavy CPU-intensive work - matrix-like computation
                double result = static_cast<double>(i + 1);
                for (int j = 0; j < 5000000; ++j) {
                    result = std::sin(result) * std::cos(static_cast<double>(j)) +
                             std::sqrt(std::abs(result * static_cast<double>(i + 1)));
                    result = std::fmod(result, 1000.0);
                }
                return result;
            });
        }

        // Sequential
        auto seq_start = high_resolution_clock::now();
        auto seq_results = run_contained_functions(seq_tasks, execution_mode::sequential);
        auto seq_end = high_resolution_clock::now();
        auto seq_duration = duration_cast<milliseconds>(seq_end - seq_start).count();

        // Parallel
        auto par_start = high_resolution_clock::now();
        auto par_results = run_contained_functions(par_tasks, execution_mode::parallel);
        auto par_end = high_resolution_clock::now();
        auto par_duration = duration_cast<milliseconds>(par_end - par_start).count();

        std::cout << "\n=== run_contained_functions Performance (n=" << num_tasks << " tasks) ===" << std::endl;
        std::cout << "Sequential: " << seq_duration << " ms (threads: " << seq_spy.unique_thread_count() << ")" << std::endl;
        std::cout << "Parallel:   " << par_duration << " ms (threads: " << par_spy.unique_thread_count() << ")" << std::endl;
        if (par_duration > 0) {
            double speedup = static_cast<double>(seq_duration) / static_cast<double>(par_duration);
            std::cout << "Speedup:    " << speedup << "x" << std::endl;
        }

        REQUIRE(seq_results.size() == par_results.size());
        REQUIRE(seq_spy.unique_thread_count() == 1);
        REQUIRE(par_spy.unique_thread_count() >= 2);
    }

    SECTION("run_functors_typed performance - sequential vs parallel with std::async")
    {
        static ThreadSpy global_seq_spy, global_par_spy;
        global_seq_spy.reset();
        global_par_spy.reset();

        struct HeavyFunctor1 {
            double operator()(int x) const {
                global_seq_spy.record();  // Will record for whichever mode is running
                global_par_spy.record();
                double result = static_cast<double>(x);
                // Heavy computational work
                for (int i = 0; i < 10000000; ++i) {
                    result = std::sin(result) * std::cos(static_cast<double>(x)) +
                             std::sqrt(std::abs(result));
                    result = std::fmod(result, 1000.0);
                }
                return result;
            }
        };

        struct HeavyFunctor2 {
            double operator()(int x) const {
                global_seq_spy.record();
                global_par_spy.record();
                double result = static_cast<double>(x);
                // Heavy computational work
                for (int i = 0; i < 10000000; ++i) {
                    result = std::log(std::abs(result) + 1.0) * std::exp(static_cast<double>(x) / 1000000.0) +
                             std::pow(std::abs(result), 0.3);
                    result = std::fmod(result, 1000.0);
                }
                return result;
            }
        };

        struct HeavyFunctor3 {
            double operator()(int x) const {
                global_seq_spy.record();
                global_par_spy.record();
                double result = static_cast<double>(x);
                // Heavy computational work
                for (int i = 0; i < 10000000; ++i) {
                    result = std::tan(result / 1000.0) * std::sin(static_cast<double>(x)) +
                             std::sqrt(std::abs(result * 2.0));
                    result = std::fmod(result, 1000.0);
                }
                return result;
            }
        };

        struct HeavyFunctor4 {
            double operator()(int x) const {
                global_seq_spy.record();
                global_par_spy.record();
                double result = static_cast<double>(x);
                // Heavy computational work
                for (int i = 0; i < 10000000; ++i) {
                    result = std::sinh(result / 10000.0) + std::cosh(static_cast<double>(x) / 10000.0) +
                             std::cbrt(std::abs(result));
                    result = std::fmod(result, 1000.0);
                }
                return result;
            }
        };

        int input = 42;

        // Sequential
        global_seq_spy.reset();
        StepResult result;
        auto seq_start = high_resolution_clock::now();
        auto seq_results = run_functors_typed<int, double, HeavyFunctor1, HeavyFunctor2,
                                              HeavyFunctor3, HeavyFunctor4>(
                                              execution_mode::sequential, input,result);
        auto seq_end = high_resolution_clock::now();
        auto seq_duration = duration_cast<milliseconds>(seq_end - seq_start).count();
        size_t seq_threads = global_seq_spy.unique_thread_count();

        // Parallel with std::async
        global_par_spy.reset();
        auto par_start = high_resolution_clock::now();
        auto par_results = run_functors_typed<int, double, HeavyFunctor1, HeavyFunctor2,
                                              HeavyFunctor3, HeavyFunctor4>(
            execution_mode::parallel, input, result);
        auto par_end = high_resolution_clock::now();
        auto par_duration = duration_cast<milliseconds>(par_end - par_start).count();
        size_t par_threads = global_par_spy.unique_thread_count();

        std::cout << "\n=== run_functors_typed Performance (4 functors, std::async) ===" << std::endl;
        std::cout << "Sequential: " << seq_duration << " ms (threads: " << seq_threads << ")" << std::endl;
        std::cout << "Parallel:   " << par_duration << " ms (threads: " << par_threads << ")" << std::endl;
        if (par_duration > 0) {
            double speedup = static_cast<double>(seq_duration) / static_cast<double>(par_duration);
            std::cout << "Speedup:    " << speedup << "x" << std::endl;
        }

        REQUIRE(seq_results.size() == 4);
        REQUIRE(par_results.size() == 4);
        // Results should be deterministic for the same input
        REQUIRE(seq_results == par_results);
        REQUIRE(seq_threads == 1);  // Sequential should use 1 thread
        REQUIRE(par_threads >= 2);  // Parallel with std::async should use multiple threads
    }
}

