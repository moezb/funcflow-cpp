#pragma once

/**
 * @file pipeline.hpp
 * @brief Pipeline class.
 */

#include <algorithm>
#include <vector>
#include <variant>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <chrono>
#include <iostream>

#include  "funcflow/stage_builder.hpp"

namespace funcflow {
// Namespace for Pipeline management
// This namespace contains classes and functions to define and manage pipelines.
// It contains the needed types to compose a Pipeline, such as stages, tasks, and execution policies.
namespace workflow {
using namespace funcflow::context_view;
using namespace funcflow::scheduler;
using namespace std::string_literals;

// Empty context for pipelines that don't need shared state
struct EmptyContext {};

// A Pipeline is a Runner that orchestrate the excution of a sequence of stages in a Pipeline.
// The Pipeline run with a context that holds the data to be processed. it also
// collects timing statistics for each stage and the total duration.
// @todo: add exception handling of the collected exceptions from stages.
template <typename TContext=EmptyContext>
class Pipeline {
public:
  struct TimingStats {
    std::unordered_map<std::string, std::chrono::nanoseconds> stage_durations;
    std::chrono::nanoseconds                                  total_duration{0};

    void clear() {
      stage_durations.clear();
      total_duration = std::chrono::nanoseconds{0};
    }

    void print_stats(std::ostream& out_stream, bool with_jobs_details = false) const {
      int line_counter_{0}; // use line counter as anchros to ease visual diffs when comparing logs					
      // of the same Pipeline
      out_stream << ++line_counter_ << " Total duration : " <<
      std::chrono::duration_cast<std::chrono::milliseconds>(total_duration).count() <<
      " for " << stage_durations.size() << " stages\n";
      if (with_jobs_details) {
        for (const auto& [stage_name, duration] : stage_durations) {
          out_stream << ++line_counter_ << " Stage: " << stage_name
          << ", Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).
          count()
          << " ms\n";
        }
      }
    }
  };

  Pipeline()                           = default;
  Pipeline(const Pipeline&)            = delete;
  Pipeline& operator=(const Pipeline&) = delete;  

      
  //Add in ordered manner a stage to the Pipeline
  void add_stage(pipeline_stage<TContext>&& stage) {
    stages_.emplace_back(std::move(stage));
  }

  //Runs the Pipeline stages in order of addition with timing measurement
  auto run(TContext& context) -> void {
    stats_.clear();
    auto total_start = std::chrono::high_resolution_clock::now();
    bool success     = true;
    for (auto& stage : stages_) {
      auto start                         = std::chrono::high_resolution_clock::now();
      success                            = stage.run(context);
      auto end                           = std::chrono::high_resolution_clock::now();
      stats_.stage_durations[stage.name] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      if (!success) {
        std::cerr << "Pipeline stage failed: " << stage.name << "\n";
        break; // Stop execution on first failure
      }
    }
    auto total_end        = std::chrono::high_resolution_clock::now();
    stats_.total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start);
  }

  // Get the timing statistics for the Pipeline execution
  const TimingStats& get_timing_stats() const {
    return stats_;
  }

  StageBuilder<TContext> stage(const std::string& stage_name) const {
    return StageBuilder<TContext>(stage_name);
  }

  std::vector<pipeline_stage<TContext>> stages_;
  TimingStats                          stats_;
};
}
} // namespace funcflow
