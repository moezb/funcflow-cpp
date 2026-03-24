#pragma once

/**
 * @file pipeline_stage.hpp 
 * @brief  Defines a class representing a single stage in a workflow pipeline.  
 */

#include <chrono>
#include <type_traits>
#include <utility>
#include "funcflow/scheduler.hpp"


namespace funcflow {
// Namespace for Pipeline management is workflow 
// This namespace contains classes and functions to define and manage pipelines of tasks operating
// over a context.
namespace workflow {
using namespace funcflow::scheduler;
template <typename TContext>
class Pipeline;

// Define stage class that represents a single stage in the Pipeline.
// The stage is defioned by a name and a runner function that takes a context as an argument.
// When run on the context, ti cvan be successful or not. See #run
template <typename TContext>
class pipeline_stage {
  // We forbid assigning a runner to a stage from outside the Pipeline class,
  // we have made runner_ private to ensure that the stage is only created
  // and managed by the Pipeline composition methods.  
  friend class Pipeline<TContext>;

public:
  pipeline_stage()                                           = default;
  pipeline_stage(const pipeline_stage& other)                = default;
  pipeline_stage(pipeline_stage&& other) noexcept            = default;
  pipeline_stage& operator=(const pipeline_stage& other)     = default;
  pipeline_stage& operator=(pipeline_stage&& other) noexcept = default;

  std::string name{};

  // Run the stage on the provided context.
  // If the runner is not set, it returns true by default.
  // This allows for stages to be optional and not required to have a runner.
  // Currently if stage fails, it would stop the Pipeline execution.
  bool run(TContext& context) {
    try {
      stage_result_.step_name = name;
      stage_result_ = true; // default to success
      if (runner_) {
        auto result             = runner_(context);
        stage_result_   = result.success();
        stage_result_.sub_steps = std::move(result.sub_steps);
      }
    }
    catch (...) {
      stage_result_ = false;
      STORE_EXCEPTION(stage_result_.exception);      
    }
    return stage_result_;
  }

  StepResult stage_result_{};

  // The runner function that will be executed when the stage is run.
  // It is a function that takes a context as an argument and returns a StepResult
  // holding information about the status of the execution.
  std::function<StepResult(TContext&)> runner_;
};
} // namespace workflow
} // namespace funcflow
