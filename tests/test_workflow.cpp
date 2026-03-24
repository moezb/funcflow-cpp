#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "funcflow/stage_builder.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include "funcflow/pipeline.hpp"

using namespace funcflow::workflow;
using namespace funcflow::context_view;
using namespace funcflow::task_utils;


// Test context and view structures
enum class TestFlags : uint32_t {
    None = 0,
    Flag1 = 1,
    Flag2 = 2,
    Flag3 = 4,
    Flag4 = 8
};

struct TestItem {
    int value = 0;
    std::string name;
    bool processed = false;
    Flags<TestFlags> flags{};

    TestItem(int v = 0, const std::string& n = "", bool p = false)
        : value(v), name(n), processed(p) {}
};

struct TestContext {
    std::vector<TestItem> items;

    TestContext() = default;
    explicit TestContext(size_t count) {
        items.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            items.emplace_back(static_cast<int>(i), "test_" + std::to_string(i));
        }
    }

    size_t size() const { return items.size(); }
};

// View getter for TestItem - proper signature with index and context
struct TestItemViewGetter {
    TestItem& operator()(size_t idx, TestContext& ctx) const {
        return ctx.items[idx];
    }
    const TestItem& operator()(size_t idx, const TestContext& ctx) const {
        return ctx.items[idx];
    }
};

// Context view range for mutable operations
using TestItemRange = context_view_range<TestContext, TestItem, TestItemViewGetter>;

// Const context view range
using ConstTestItemRange = const_context_view_range<TestContext, TestItem, TestItemViewGetter>;

// Dual context view range for visitors (supports both const and mutable)
using DualTestItemRange = dual_context_view_range<ConstTestItemRange, TestItemRange>;

// ============================================================================
// Test Modifiers
// ============================================================================

struct DoubleValue {
    void operator()(TestItem& item) const {
        item.value *= 2;
    }
};

struct MarkProcessed {
    void operator()(TestItem& item) const {
        item.processed = true;
    }
};

struct IncrementValue {
    void operator()(TestItem& item) const {
        item.value += 1;
    }
};

struct AppendModified {
    void operator()(TestItem& item) const {
        item.name += "_modified";
    }
};

// ============================================================================
// Test Visitors
// ============================================================================

struct ValueExtractor {
    int operator()(const TestItem& item) const {
        return item.value;
    }
};

struct NameLengthExtractor {
    int operator()(const TestItem& item) const {
        return static_cast<int>(item.name.length());
    }
};

struct ValueSquarer {
    int operator()(const TestItem& item) const {
        return item.value * item.value;
    }
};

struct StringResultVisitor {
    std::string operator()(const TestItem& item) const {
        return "processed_" + std::to_string(item.value);
    }
};

// ============================================================================
// Test Collectors
// ============================================================================

struct IDCollector {
    std::vector<int> operator()(const TestItem& item) const {
        return {item.value};
    }
};

struct RangeCollector {
    std::vector<int> operator()(const TestItem& item) const {
        return {item.value, item.value + 1, item.value + 2};
    }
};

struct NameLengthCollector {
    std::vector<int> operator()(const TestItem& item) const {
        return {static_cast<int>(item.name.length())};
    }
};

struct MultipleValuesCollector {
    std::vector<int> operator()(const TestItem& item) const {
        std::vector<int> results;
        for (int i = 0; i < item.value; ++i) {
            results.push_back(i);
        }
        return results;
    }
};

// ============================================================================
// Test Flag Visitors
// ============================================================================

struct HighValueFlagVisitor {
    Flags<TestFlags> operator()(const TestItem& item) const {
        return item.value > 5 ? Flags<TestFlags>(TestFlags::Flag1)
                              : Flags<TestFlags>();
    }
};

struct EmptyNameFlagVisitor {
    Flags<TestFlags> operator()(const TestItem& item) const {
        return item.name.empty() ? Flags<TestFlags>(TestFlags::Flag2)
                                 : Flags<TestFlags>();
    }
};

struct ProcessedFlagVisitor {
    Flags<TestFlags> operator()(const TestItem& item) const {
        return item.processed ? Flags<TestFlags>(TestFlags::Flag3)
                              : Flags<TestFlags>();
    }
};

struct EvenValueFlagVisitor {
    Flags<TestFlags> operator()(const TestItem& item) const {
        return (item.value % 2 == 0) ? Flags<TestFlags>(TestFlags::Flag4)
                                     : Flags<TestFlags>();
    }
};

// ============================================================================
// TESTS: Modifier 
// ============================================================================

TEST_CASE("Workflow modifiers - fluent API", "[workflow][modifiers]")
{
    SECTION("Sequential iteration with single modifier")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}};

        auto stage =
            StageBuilder<TestContext>("double_values")
            .iterate<DualTestItemRange>()
            .with_modifiers<DoubleValue>();

        bool success = stage.run(ctx);

        REQUIRE(success);
        CHECK(ctx.items[0].value == 2);
        CHECK(ctx.items[1].value == 4);
        CHECK(ctx.items[2].value == 6);
    }

    SECTION("Sequential iteration with multiple modifiers")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}};

        auto stage = StageBuilder<TestContext>("process_items")
            .iterate<TestItemRange>()
            .with_modifiers<DoubleValue, MarkProcessed>();

        bool success = stage.run(ctx);

        REQUIRE(success);
        // values doubles and marked as processed
        CHECK(ctx.items[0].value == 2);
        CHECK(ctx.items[0].processed == true);
        CHECK(ctx.items[1].value == 4);
        CHECK(ctx.items[1].processed == true);
        CHECK(ctx.items[2].value == 6);
        CHECK(ctx.items[2].processed == true);
    }

    SECTION("Parallel iteration with modifiers")
    {
        TestContext ctx;
        ctx.items.resize(100);
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            ctx.items[i].value = static_cast<int>(i + 1);
        }

        auto stage = StageBuilder<TestContext>("parallel_process")
            .parallel_iterate<TestItemRange>()
            .with_modifiers<DoubleValue, MarkProcessed>();

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Check all values doubled and marked as processed
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            CHECK(ctx.items[i].value == static_cast<int>((i + 1) * 2));
            CHECK(ctx.items[i].processed == true);
        }
    }

    SECTION("Three modifiers chained")
    {
        TestContext ctx;
        ctx.items = {{5, "a"}, {10, "b"}};

        auto stage = StageBuilder<TestContext>("three_modifiers")
            .iterate<TestItemRange>()
            .with_modifiers<IncrementValue, DoubleValue, MarkProcessed>();

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Value incremented by 1, then doubled, and marked as processed
        REQUIRE(ctx.items[0].value == 12);  // (5 + 1) * 2
        REQUIRE(ctx.items[0].processed == true);
        REQUIRE(ctx.items[1].value == 22);  // (10 + 1) * 2
        REQUIRE(ctx.items[1].processed == true);
    }

    SECTION("Empty context with modifiers")
    {
        TestContext ctx;  // Empty items

        auto stage = StageBuilder<TestContext>("empty")
            .iterate<TestItemRange>()
            .with_modifiers<DoubleValue>();

        bool success = stage.run(ctx);
        // No items to process, but should succeed  
        REQUIRE(success);
    }
}

 // ============================================================================
 // TESTS: Collectors
 // ============================================================================

TEST_CASE("Workflow collectors - fluent API", "[workflow][collectors]")
{
    SECTION("Sequential iteration with single collector")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}};

        std::vector<int> collected;
        auto merge_callback = [&](TestItem& item, const std::vector<int>& results) {
            collected.insert(collected.end(), results.begin(), results.end());
        };

        auto stage = StageBuilder<TestContext>("collect_values")
            .iterate<DualTestItemRange>()
            .with_collectors<int, IDCollector>()
            .merge(std::move(merge_callback));

        bool success = stage.run(ctx);

        REQUIRE(success);
        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0] == 1);
        REQUIRE(collected[1] == 2);
        REQUIRE(collected[2] == 3);
    }

    SECTION("Sequential iteration with multiple collectors")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "bb"}};

        std::vector<int> collected;
        auto merge_callback = [&](TestItem& item, const std::vector<int>& results) {
            collected.insert(collected.end(), results.begin(), results.end());
        };

        auto stage = StageBuilder<TestContext>("collect_multi")
            .iterate<DualTestItemRange>()
            .with_collectors<int, IDCollector, RangeCollector, NameLengthCollector>()
            .merge(std::move(merge_callback));

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Each item produces 1 from IDCollector, 3 from RangeCollector, and 1 from NameLengthCollector
        // Item 0: IDCollector={1}, RangeCollector={1,2,3}, NameLengthCollector={1}
        // Item 1: IDCollector={2}, RangeCollector={2,3,4}, NameLengthCollector={2}
        // Total: 10 elements
        REQUIRE(collected.size() == 10);
    }

    SECTION("Parallel iteration with collectors")
    {
        TestContext ctx;
        ctx.items.resize(100);
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            ctx.items[i].value = static_cast<int>(i);
            ctx.items[i].name = "item";
        }

        std::vector<int> collected;
        std::mutex mutex;

        auto merge_callback = [&](TestItem& item, const std::vector<int>& results) {
            std::lock_guard<std::mutex> lock(mutex);
            collected.insert(collected.end(), results.begin(), results.end());
        };

        auto stage = StageBuilder<TestContext>("parallel_collect")
            .parallel_iterate<DualTestItemRange>()
            .with_collectors<int, IDCollector>()
            .merge(std::move(merge_callback));

        bool success = stage.run(ctx);

        REQUIRE(success);
        REQUIRE(collected.size() == 100);
    }

    SECTION("Parallel collectors with parallel iteration")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}};

        std::vector<int> collected;
        std::mutex mutex;

        auto merge_callback = [&](TestItem& item, const std::vector<int>& results) {
            std::lock_guard<std::mutex> lock(mutex);
            collected.insert(collected.end(), results.begin(), results.end());
        };

        auto stage = StageBuilder<TestContext>("full_parallel_collect")
            .parallel_iterate<DualTestItemRange>()
            .with_parallel_collectors<int, IDCollector, RangeCollector>()
            .merge(std::move(merge_callback));

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Each item: 1 from IDCollector + 3 from RangeCollector = 4 values
        // 3 items * 4 values = 12 total
        REQUIRE(collected.size() == 12);
    }

    SECTION("Collectors without merge callback")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}};

        auto stage = StageBuilder<TestContext>("no_merge_collect")
            .iterate<DualTestItemRange>()
            .with_collectors<int, IDCollector>()
            .build();

        bool success = stage.run(ctx);

        REQUIRE(success);
    }

    SECTION("Collector with variable-length results")
    {
        TestContext ctx;
        ctx.items = {{0, "a"}, {1, "b"}, {2, "c"}, {3, "d"}};

        std::vector<int> collected;
        auto merge_callback = [&](TestItem& item, const std::vector<int>& results) {
            collected.insert(collected.end(), results.begin(), results.end());
        };

        auto stage = StageBuilder<TestContext>("variable_collect")
            .iterate<DualTestItemRange>()
            .with_collectors<int, MultipleValuesCollector>()
            .merge(std::move(merge_callback));

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Item 0 (value=0): {}
        // Item 1 (value=1): {0}
        // Item 2 (value=2): {0,1}
        // Item 3 (value=3): {0,1,2}
        // Total: 0 + 1 + 2 + 3 = 6 elements
        REQUIRE(collected.size() == 6);
    }

    SECTION("Empty context with collectors")
    {
        TestContext ctx;  // Empty items

        int callback_count = 0;
        auto merge = [&](TestItem&, const std::vector<int>& ) {
            callback_count++;
        };

        auto stage = StageBuilder<TestContext>("empty_collect")
            .iterate<DualTestItemRange>()
            .with_collectors<int, IDCollector>()
            .merge(std::move(merge));

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(callback_count == 0);
    }

    SECTION("Collector concept verification")
    {
        using namespace funcflow::context_view;

        static_assert(Collector<IDCollector, TestItem, int>,
                     "IDCollector should satisfy Collector concept");
        static_assert(Collector<RangeCollector, TestItem, int>,
                     "RangeCollector should satisfy Collector concept");
        static_assert(Collector<MultipleValuesCollector, TestItem, int>,
                     "MultipleValuesCollector should satisfy Collector concept");
    }
}

// ============================================================================
// TESTS: Flag Collectors
// ============================================================================

TEST_CASE("Workflow flag collectors - fluent API", "[workflow][flags]")
{
    SECTION("Sequential iteration with single flag collector")
    {
        TestContext ctx;
        ctx.items = {{10, "a"}, {3, "b"}, {8, "c"}};

        auto stage = StageBuilder<TestContext>("collect_high_value_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag1));  // value=10 > 5
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag1)); // value=3 <= 5
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag1));  // value=8 > 5
    }

    SECTION("Sequential iteration with multiple flag collectors")
    {
        TestContext ctx;
        ctx.items = {{10, ""}, {3, "b"}, {6, ""}};

        auto stage = StageBuilder<TestContext>("collect_multiple_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor, EmptyNameFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);
        // Item 0: value=10>5 (Flag1), name="" (Flag2)
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[0].flags.has_all_flags({TestFlags::Flag2,TestFlags::Flag1}));
        REQUIRE(ctx.items[0].flags.has_all_flags(TestFlags::Flag2|TestFlags::Flag1));

        // Item 1: value=3<=5 (no Flag1), name="b" (no Flag2)
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag1));
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag2));

        // Item 2: value=6>5 (Flag1), name="" (Flag2)
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag2));
    }

    SECTION("Parallel iteration with flag collectors")
    {
        TestContext ctx;
        ctx.items.resize(50);
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            ctx.items[i].value = static_cast<int>(i);
            ctx.items[i].name = (i % 3 == 0) ? "" : "item";
        }

        auto stage = StageBuilder<TestContext>("parallel_flag_collect")
            .parallel_iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor, EmptyNameFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);

        // Verify flags set correctly
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            bool should_have_flag1 = ctx.items[i].value > 5;
            bool should_have_flag2 = ctx.items[i].name.empty();

            REQUIRE(ctx.items[i].flags.testFlag(TestFlags::Flag1) == should_have_flag1);
            REQUIRE(ctx.items[i].flags.testFlag(TestFlags::Flag2) == should_have_flag2);
        }
    }

    SECTION("Parallel flag collectors with parallel iteration")
    {
        TestContext ctx;
        ctx.items = {{4, "a"}, {7, ""}, {2, "c"}, {10, ""}};

        auto stage = StageBuilder<TestContext>("full_parallel_flags")
            .parallel_iterate<DualTestItemRange>()
            .with_parallel_flag_collectors<TestFlags, HighValueFlagVisitor, EmptyNameFlagVisitor, EvenValueFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);

        // Item 0: value=4 (no Flag1, Flag4), name="a" (no Flag2)
        REQUIRE(!ctx.items[0].flags.testFlag(TestFlags::Flag1));
        REQUIRE(!ctx.items[0].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag4));

        // Item 1: value=7 (Flag1, no Flag4), name="" (Flag2)
        REQUIRE(ctx.items[1].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[1].flags.testFlag(TestFlags::Flag2));
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag4));

        // Item 2: value=2 (no Flag1, Flag4), name="c" (no Flag2)
        REQUIRE(!ctx.items[2].flags.testFlag(TestFlags::Flag1));
        REQUIRE(!ctx.items[2].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag4));

        // Item 3: value=10 (Flag1, Flag4), name="" (Flag2)
        REQUIRE(ctx.items[3].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[3].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[3].flags.testFlag(TestFlags::Flag4));
    }

    SECTION("Flag accumulation with pre-existing flags")
    {
        TestContext ctx;
        ctx.items = {{10, "a"}, {3, "b"}};

        // Pre-set Flag3 on first item
        ctx.items[0].flags.set_flag(TestFlags::Flag3);

        auto stage = StageBuilder<TestContext>("accumulate_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);

        // Item 0 should have both Flag3 (pre-existing) and Flag1 (from visitor)
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag3));

        // Item 1 should have neither
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag1));
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag3));
    }

    SECTION("All flag collectors combined")
    {
        TestContext ctx;
        ctx.items = {{10, "", false}, {3, "b", true}, {8, "", true}};

        auto stage = StageBuilder<TestContext>("all_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor, EmptyNameFlagVisitor,
                                  ProcessedFlagVisitor, EvenValueFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);

        REQUIRE(success);

        // Item 0: value=10>5 (Flag1), name="" (Flag2), processed=false (no Flag3), even (Flag4)
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag2));
        REQUIRE(!ctx.items[0].flags.testFlag(TestFlags::Flag3));
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag4));

        // Item 1: value=3<=5 (no Flag1), name="b" (no Flag2), processed=true (Flag3), odd (no Flag4)
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag1));
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[1].flags.testFlag(TestFlags::Flag3));
        REQUIRE(!ctx.items[1].flags.testFlag(TestFlags::Flag4));

        // Item 2: value=8>5 (Flag1), name="" (Flag2), processed=true (Flag3), even (Flag4)
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag1));
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag2));
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag3));
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag4));
    }

    SECTION("Empty context with flag collectors")
    {
        TestContext ctx;  // Empty items

        auto stage = StageBuilder<TestContext>("empty_flag_collect")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor>()
            .store_in(&TestItem::flags);

        bool success = stage.run(ctx);
        REQUIRE(success);
    }
}

// ============================================================================
// TESTS: Pipeline Integration
// ============================================================================

TEST_CASE("Workflow Pipeline integration", "[workflow][Pipeline]")
{
    SECTION("Multiple stages in Pipeline")
    {
        TestContext ctx(5);

        Pipeline<TestContext> pipe;

        // Stage 1: Double all values
        auto stage1 = StageBuilder<TestContext>("double_values")
            .iterate<TestItemRange>()
            .with_modifiers<DoubleValue>();

        // Stage 2: Mark as processed
        auto stage2 = StageBuilder<TestContext>("mark_processed")
            .iterate<TestItemRange>()
            .with_modifiers<MarkProcessed>();

        // Stage 3: Collect flags
        auto stage3 = StageBuilder<TestContext>("collect_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, HighValueFlagVisitor>()
            .store_in(&TestItem::flags);

        pipe.add_stage(std::move(stage1));
        pipe.add_stage(std::move(stage2));
        pipe.add_stage(std::move(stage3));

        pipe.run(ctx);

        // Verify all stages executed
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            REQUIRE(ctx.items[i].value == static_cast<int>(i * 2));
            REQUIRE(ctx.items[i].processed == true);

            // After doubling, items 3 and 4 should have value > 5
            if (i >= 3) {
                REQUIRE(ctx.items[i].flags.testFlag(TestFlags::Flag1));
            } else {
                REQUIRE(!ctx.items[i].flags.testFlag(TestFlags::Flag1));
            }
        }
    }

    SECTION("Mixed execution modes in Pipeline")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};

        Pipeline<TestContext> pipe;

        // Sequential modification
        auto stage1 = StageBuilder<TestContext>("seq_modify")
            .iterate<TestItemRange>()
            .with_modifiers<IncrementValue>();

        // Parallel modification
        auto stage2 = StageBuilder<TestContext>("par_modify")
            .parallel_iterate<TestItemRange>()
            .with_modifiers<DoubleValue>();

        // Sequential flag collection
        auto stage3 = StageBuilder<TestContext>("seq_flags")
            .iterate<DualTestItemRange>()
            .with_flag_collectors<TestFlags, EvenValueFlagVisitor>()
            .store_in(&TestItem::flags);

        pipe.add_stage(std::move(stage1));
        pipe.add_stage(std::move(stage2));
        pipe.add_stage(std::move(stage3));

        pipe.run(ctx);

        // Verify: (value + 1) * 2, and flag if even
        REQUIRE(ctx.items[0].value == 4);  // (1+1)*2
        REQUIRE(ctx.items[0].flags.testFlag(TestFlags::Flag4));

        REQUIRE(ctx.items[1].value == 6);  // (2+1)*2
        REQUIRE(ctx.items[1].flags.testFlag(TestFlags::Flag4));

        REQUIRE(ctx.items[2].value == 8);  // (3+1)*2
        REQUIRE(ctx.items[2].flags.testFlag(TestFlags::Flag4));

        REQUIRE(ctx.items[3].value == 10); // (4+1)*2
        REQUIRE(ctx.items[3].flags.testFlag(TestFlags::Flag4));
    }
}

// ============================================================================
// TESTS: Edge Cases
// ============================================================================

TEST_CASE("Workflow edge cases", "[workflow][edge_cases]")
{
    SECTION("Single item processing")
    {
        TestContext ctx;
        ctx.items = {{42, "single"}};

        auto stage = StageBuilder<TestContext>("single")
            .parallel_iterate<TestItemRange>()
            .with_modifiers<DoubleValue>();

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].value == 84);
    }

    SECTION("Large dataset stress test")
    {
        TestContext ctx;
        ctx.items.resize(10000);
        for (size_t i = 0; i < ctx.items.size(); ++i) {
            ctx.items[i].value = static_cast<int>(i);
            ctx.items[i].name = "item" + std::to_string(i);
        }

        auto stage = StageBuilder<TestContext>("stress")
            .parallel_iterate<TestItemRange>()
            .with_modifiers<IncrementValue, DoubleValue>();

        bool success = stage.run(ctx);
        REQUIRE(success);

        for (size_t i = 0; i < ctx.items.size(); ++i) {
            REQUIRE(ctx.items[i].value == static_cast<int>((i + 1) * 2));
        }
    }

    SECTION("Empty stage succeeds")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}};

        pipeline_stage<TestContext> stage;
        stage.name = "empty_stage";

        bool success = stage.run(ctx);
        REQUIRE(success);
    }
}

// ============================================================================
// TESTS: Sequence API
// ============================================================================

TEST_CASE("Workflow sequence API", "[workflow][sequence]")
{
    SECTION("Single lambda Task")
    {
        TestContext ctx;
        ctx.items = {{1, "a"}, {2, "b"}};

        auto stage = StageBuilder<TestContext>("lambda_task")
            .sequence([](TestContext& context) {
                for (auto& item : context.items) {
                    item.value *= 10;
                }
            });

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].value == 10);
        REQUIRE(ctx.items[1].value == 20);
    }

    SECTION("Multiple lambda tasks")
    {
        TestContext ctx;
        ctx.items = {{5, "test"}};

        auto stage = StageBuilder<TestContext>("multi_lambda")
            .sequence(
                [](TestContext& context) {
                    context.items[0].value += 10;
                },
                [](TestContext& context) {
                    context.items[0].value *= 2;
                },
                [](TestContext& context) {
                    context.items[0].processed = true;
                }
            );

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].value == 30);  // (5 + 10) * 2
        REQUIRE(ctx.items[0].processed == true);
    }

    SECTION("Sequence with bool-returning predicate")
    {
        TestContext ctx;
        ctx.items = {{10, "a"}};

        auto stage = StageBuilder<TestContext>("conditional")
            .sequence([](TestContext& context) -> bool {
                return context.items[0].value > 5;
            });

        bool success = stage.run(ctx);
        REQUIRE(success);
    }

    SECTION("All-bool sequence - stops on first false")
    {
        TestContext ctx;
        ctx.items = {{3, "test"}};

        auto stage = StageBuilder<TestContext>("all_bool_fail_fast")
            .sequence(
                [](TestContext& context) -> bool {
                    return context.items[0].value > 10;  // Will return false
                },
                [](TestContext& context) -> bool {
                    context.items[0].processed = true; // Won't be executed
                    return true;
                }
            );

        bool success = stage.run(ctx);
        REQUIRE_FALSE(success);
        REQUIRE_FALSE(ctx.items[0].processed);  // Second lambda not executed
    }

    SECTION("All-bool sequence - all tasks succeed")
    {
        TestContext ctx;
        ctx.items = {{15, "test"}};

        auto stage = StageBuilder<TestContext>("all_bool_succeed")
            .sequence(
                [](TestContext& context) -> bool {
                    return context.items[0].value > 10;  // Returns true
                },
                [](TestContext& context) -> bool {
                    context.items[0].processed = true;
                    return true;
                }
            );

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].processed);  // Both lambdas executed
    }

    SECTION("All-void sequence - always succeeds")
    {
        TestContext ctx;
        ctx.items = {{5, "test"}};

        auto stage = StageBuilder<TestContext>("all_void")
            .sequence(
                [](TestContext& context) {
                    context.items[0].value += 10;  // No return value
                },
                [](TestContext& context) {
                    context.items[0].processed = true;  // No return value
                }
            );

        bool success = stage.run(ctx);
        REQUIRE(success);  // All void tasks assume success
        REQUIRE(ctx.items[0].value == 15);
        REQUIRE(ctx.items[0].processed);
    }

    SECTION("Mixed bool+void sequence - bool checked, void assumes success")
    {
        TestContext ctx;
        ctx.items = {{20, "test"}};

        auto stage = StageBuilder<TestContext>("mixed_success")
            .sequence(
                [](TestContext& context) -> bool {
                    return context.items[0].value > 10;  // Returns true
                },
                [](TestContext& context) {
                    context.items[0].value += 5;  // Void - assumes success
                },
                [](TestContext& context) -> bool {
                    return context.items[0].value == 25;  // Validates: 20 + 5 = 25
                }
            );

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].value == 25);
    }

    SECTION("Mixed bool+void sequence - bool fails, stops execution")
    {
        TestContext ctx;
        ctx.items = {{3, "test"}};

        auto stage = StageBuilder<TestContext>("mixed_fail")
            .sequence(
                [](TestContext& context) -> bool {
                    return context.items[0].value > 10;  // Returns false
                },
                [](TestContext& context) {
                    context.items[0].processed = true;  // Won't execute
                }
            );

        bool success = stage.run(ctx);
        REQUIRE_FALSE(success);
        REQUIRE_FALSE(ctx.items[0].processed);  // Second Task not executed
    }

    SECTION("Mixed sequence - functor types")
    {
        struct AddFiveFunctor {
            void operator()(TestContext& ctx) const {
                ctx.items[0].value += 5;
            }
        };

        struct DoubleValueFunctor {
            void operator()(TestContext& ctx) const {
                ctx.items[0].value *= 2;
            }
        };

        TestContext ctx;
        ctx.items = {{10, "test"}};

        auto stage = StageBuilder<TestContext>("functors")
            .sequence<AddFiveFunctor, DoubleValueFunctor>();

        bool success = stage.run(ctx);
        REQUIRE(success);
        REQUIRE(ctx.items[0].value == 30);  // (10 + 5) * 2
    }

    SECTION("Boolean-returning functors")
    {
        struct ValidatorFunctor {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value >= 10;
            }
        };

        struct FailingValidatorFunctor {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value >= 100;
            }
        };

        struct ModifierFunctor {
            void operator()(TestContext& ctx) const {
                ctx.items[0].value += 5;
            }
        };

        SECTION("Boolean functor returning true")
        {
            TestContext ctx;
            ctx.items = {{15, "test"}};

            auto stage = StageBuilder<TestContext>("validator_true")
                .sequence<ValidatorFunctor>();

            bool success = stage.run(ctx);
            REQUIRE(success);  // Functor returns true
            REQUIRE(ctx.items[0].value == 15);  // Value unchanged
        }

        SECTION("Boolean functor returning false")
        {
            TestContext ctx;
            ctx.items = {{5, "test"}};

            auto stage = StageBuilder<TestContext>("validator_false")
                .sequence<FailingValidatorFunctor>();

            bool success = stage.run(ctx);
            REQUIRE_FALSE(success);
            REQUIRE(ctx.items[0].value == 5);  // Value unchanged
        }

        SECTION("All-bool functors - fail-fast on first false")
        {
            TestContext ctx;
            ctx.items = {{5, "test"}};

            auto stage = StageBuilder<TestContext>("all_bool_functors_fail")
                .sequence<FailingValidatorFunctor, ValidatorFunctor>();

            bool success = stage.run(ctx);
            REQUIRE_FALSE(success);  // First functor fails
            REQUIRE(stage.stage_result_.sub_steps.size() == 2);  // Only first functor executed
            CHECK_FALSE(stage.stage_result_.sub_steps.front()); // Only first functor executed
            CHECK_FALSE(stage.stage_result_.sub_steps.back()); // Only first functor executed
        }

        SECTION("All-void functors - always succeed")
        {
            struct AddTenFunctor {
                void operator()(TestContext& ctx) const {
                    ctx.items[0].value += 10;
                }
            };

            struct MarkProcessedFunctor {
                void operator()(TestContext& ctx) const {
                    ctx.items[0].processed = true;
                }
            };

            TestContext ctx;
            ctx.items = {{5, "test"}};

            auto stage = StageBuilder<TestContext>("all_void_functors")
                .sequence<AddTenFunctor, MarkProcessedFunctor>();

            bool success = stage.run(ctx);
            REQUIRE(success);  // All void functors assume success
            REQUIRE(ctx.items[0].value == 15);
            REQUIRE(ctx.items[0].processed);
        }

        SECTION("Mixed boolean and void functors - bool returns now respected")
        {
            TestContext ctx;
            ctx.items = {{20, "test"}};

            // With the updated implementation using if constexpr, bool return values
            // ARE now respected even when mixing with void functors!
            auto stage = StageBuilder<TestContext>("mixed")
                .sequence<ValidatorFunctor, ModifierFunctor, ValidatorFunctor>();

            bool success = stage.run(ctx);
            REQUIRE(success);  // All functors succeed: 20>=10, modify to 25, 25>=10
            REQUIRE(ctx.items[0].value == 25);  // 20 + 5
        }

        SECTION("Mixed boolean and void functors - bool functor fails")
        {
            TestContext ctx;
            ctx.items = {{5, "test"}};

            // First validator returns false (5 < 10), should fail and stop execution
            auto stage = StageBuilder<TestContext>("mixed_fail")
                .sequence<ValidatorFunctor, ModifierFunctor>();

            bool success = stage.run(ctx);
            REQUIRE(!success);  // First functor fails: 5 < 10
            REQUIRE(ctx.items[0].value == 5);  // Second functor not executed due to fail_fast
        }

        SECTION("Boolean lambda and functor combination")
        {
            TestContext ctx;
            ctx.items = {{50, "test"}};

            auto stage = StageBuilder<TestContext>("lambda_and_functor")
                .sequence(
                    [](TestContext& context) -> bool {
                        return context.items[0].value > 40;  // Returns true
                    },
                    [](TestContext& context) {
                        context.items[0].value *= 2;  // Modifies value
                    },
                    [](TestContext& context) -> bool {
                        return context.items[0].value == 100;  // Validates result
                    }
                );

            bool success = stage.run(ctx);
            REQUIRE(success);
            REQUIRE(ctx.items[0].value == 100);  // 50 * 2
        }
    }

    SECTION("Real-world pattern: Validation gates")
    {
        struct PreConditionCheck {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value >= 0 && !ctx.items[0].name.empty();
            }
        };

        struct ProcessData {
            void operator()(TestContext& ctx) const {
                ctx.items[0].value = ctx.items[0].value * 2 + 10;
            }
        };

        struct PostConditionCheck {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value > 0;
            }
        };

        SECTION("Valid input passes all gates")
        {
            TestContext ctx;
            ctx.items = {{5, "data"}};

            auto stage = StageBuilder<TestContext>("validation_pipeline")
                .sequence<PreConditionCheck, ProcessData, PostConditionCheck>();

            bool success = stage.run(ctx);
            REQUIRE(success);
            REQUIRE(ctx.items[0].value == 20);  // (5 * 2) + 10
        }

        SECTION("Invalid input fails pre-condition")
        {
            TestContext ctx;
            ctx.items = {{5, ""}};  // Empty name

            auto stage = StageBuilder<TestContext>("validation_pipeline")
                .sequence<PreConditionCheck, ProcessData, PostConditionCheck>();

            bool success = stage.run(ctx);
            REQUIRE_FALSE(success);
            REQUIRE(ctx.items[0].value == 5);  // Unchanged - processing never ran
        }

        SECTION("Post-condition can catch invariant violations")
        {
            struct CorruptData {
                void operator()(TestContext& ctx) const {
                    ctx.items[0].value = -100;  // Corrupt the value
                }
            };

            TestContext ctx;
            ctx.items = {{5, "data"}};

            auto stage = StageBuilder<TestContext>("validation_pipeline")
                .sequence<PreConditionCheck, CorruptData, PostConditionCheck>();

            bool success = stage.run(ctx);
            REQUIRE_FALSE(success);  // Post-condition fails
            REQUIRE(ctx.items[0].value == -100);  // Corruption detected
        }
    }

    SECTION("Real-world pattern: Complex mixed sequences")
    {
        struct CheckPermissions {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value > 0;  // Simulate permission check
            }
        };

        struct LoadResource {
            void operator()(TestContext& ctx) const {
                ctx.items[0].name += "_loaded";
            }
        };

        struct Transform {
            void operator()(TestContext& ctx) const {
                ctx.items[0].value *= 3;
            }
        };

        struct ValidateResult {
            bool operator()(TestContext& ctx) const {
                return ctx.items[0].value < 1000;
            }
        };

        struct Commit {
            void operator()(TestContext& ctx) const {
                ctx.items[0].processed = true;
            }
        };

        SECTION("Full Pipeline succeeds")
        {
            TestContext ctx;
            ctx.items = {{10, "resource"}};

            auto stage = StageBuilder<TestContext>("complex_pipeline")
                .sequence<CheckPermissions, LoadResource, Transform, ValidateResult, Commit>();

            bool success = stage.run(ctx);
            REQUIRE(success);
            REQUIRE(ctx.items[0].value == 30);
            REQUIRE(ctx.items[0].name == "resource_loaded");
            REQUIRE(ctx.items[0].processed);
        }

        SECTION("Pipeline fails validation")
        {
            TestContext ctx;
            ctx.items = {{500, "resource"}};  // Will be 1500 after transform

            auto stage = StageBuilder<TestContext>("complex_pipeline")
                .sequence<CheckPermissions, LoadResource, Transform, ValidateResult, Commit>();

            bool success = stage.run(ctx);
            REQUIRE_FALSE(success);  // Validation fails
            REQUIRE(ctx.items[0].value == 1500);  // Transform completed
            REQUIRE(ctx.items[0].name == "resource_loaded");  // Load completed
            REQUIRE_FALSE(ctx.items[0].processed);  // Commit never ran
        }
    }
}

// ============================================================================
// TESTS: Pipeline with EmptyContext
// ============================================================================

TEST_CASE("Workflow Pipeline with empty context", "[workflow][EmptyContext]")
{
    SECTION("Pipeline without shared state")
    {
        int counter = 0;

        auto pipe = Pipeline<>();
        auto stage = pipe.stage("increment")
            .sequence([&](EmptyContext&) {
                counter += 10;
            });

        pipe.add_stage(std::move(stage));

        EmptyContext ctx;
        pipe.run(ctx);

        REQUIRE(counter == 10);
    }

    SECTION("Multiple stages with empty context")
    {
        std::vector<int> results;

        auto pipe = Pipeline<>();

        auto stage1 = pipe.stage("stage1")
            .sequence([&](EmptyContext&) {
                results.push_back(1);
            });

        auto stage2 = pipe.stage("stage2")
            .sequence([&](EmptyContext&) {
                results.push_back(2);
            });

        auto stage3 = pipe.stage("stage3")
            .sequence([&](EmptyContext&) {
                results.push_back(3);
            });

        pipe.add_stage(std::move(stage1));
        pipe.add_stage(std::move(stage2));
        pipe.add_stage(std::move(stage3));

        EmptyContext ctx;
        pipe.run(ctx);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == 1);
        REQUIRE(results[1] == 2);
        REQUIRE(results[2] == 3);
    }
}

// ============================================================================
// TESTS: Type Verification
// ============================================================================

TEST_CASE("Workflow type verification", "[workflow][types]")
{
    SECTION("Modifier callability")
    {
        // Verify that modifiers can be invoked with TestItem
        constexpr bool double_value_ok = std::is_invocable_v<DoubleValue, TestItem&>;
        constexpr bool mark_processed_ok = std::is_invocable_v<MarkProcessed, TestItem&>;
        constexpr bool increment_ok = std::is_invocable_v<IncrementValue, TestItem&>;

        REQUIRE(double_value_ok);
        REQUIRE(mark_processed_ok);
        REQUIRE(increment_ok);
    }

    SECTION("Collector result types")
    {
        // Verify that collectors return expected types (they return std::vector<int>)
        constexpr bool id_collector_ok = std::is_invocable_r_v<std::vector<int>, IDCollector, const TestItem&>;
        constexpr bool range_collector_ok = std::is_invocable_r_v<std::vector<int>, RangeCollector, const TestItem&>;

        REQUIRE(id_collector_ok);
        REQUIRE(range_collector_ok);
    }

    SECTION("Range type properties")
    {
        // Verify that our custom context view range types are constructible
        // Note: These may not satisfy std::ranges::range due to custom iterator semantics
        constexpr bool test_range_constructible = std::is_constructible_v<TestItemRange, TestContext&>;
        constexpr bool const_range_constructible = std::is_constructible_v<ConstTestItemRange, const TestContext&>;
        constexpr bool dual_range_constructible = std::is_constructible_v<DualTestItemRange,TestContext&>;

        REQUIRE(test_range_constructible);
        REQUIRE(const_range_constructible);
        REQUIRE(dual_range_constructible);
    }
}
