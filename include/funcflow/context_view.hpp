#pragma once

/**
 * @file context_view.hpp
 * @brief This file now serves as an aggregator for the more modular components
 *        All relevant types and concepts are brought in through the includes above
 */

// Include all component headers
#include "funcflow/context_view_concepts.hpp"
#include "funcflow/context_view_iterator.hpp"
#include "funcflow/context_view_range.hpp"
#include "funcflow/visitor_concepts.hpp"
#include "funcflow/visitor_runner.hpp"

namespace funcflow {
namespace context_view {

} // namespace context_view
} // namespace funcflow
