#pragma once

/**
 * @file context_view_range.hpp
 * @brief Implements range wrappers for context views.
 */

#include "funcflow/context_view_iterator.hpp"
#include <concepts>

namespace funcflow {
namespace context_view {
/**
 * @brief A range wrapper providing view-based iteration over a context.
 *
 * This class template allows iteration over a context object using a custom view getter.
 * It provides begin/end iterators, size retrieval, and indexed access to views.
 * It is intended to be in Pipeline closures where the TContext is captured by reference and remains valid.
 *
 * @tparam TContext The type of the context to iterate over.
 * @tparam TView The type of the view returned for each context element.
 * @tparam TGetView The type of the view getter, which must satisfy the ViewGetter concept.
 *
 * @requires ViewGetter<TGetView, TContext, TView>
 *
 * @note The context_view_range does not own the context; it only references it.
 */
template <typename TContext, typename TView, typename TGetView>
  requires ViewGetter<TGetView, TContext, TView>
class context_view_range {
public:
  using context     = TContext;
  using view        = TView;
  using view_getter = TGetView;
  using iterator    = context_view_iterator<TContext, TView, TGetView>;

  context_view_range(TContext& context)
  : context_(context) {
  }

  iterator begin() { return iterator(context_, 0); }
  iterator end() { return iterator(context_, context_.size()); }

  size_t size() const { return context_.size(); }

  TView& operator[](size_t i) {
    TGetView getter{};
    return getter(i, context_);
  }

private:
  TContext& context_;
};

/**
 * @brief A range wrapper providing read-only view-based iteration over a context.
 *
 * This class template allows read-only iteration over a context object using a custom view getter.
 * It provides begin/end const iterators, size retrieval, and indexed access to const views.
 *
 * @tparam TContext The type of the context to iterate over.
 * @tparam TView The type of the view returned for each context element.
 * @tparam TGetView The type of the view getter, which must satisfy the ConstViewGetter concept.
 *
 * @requires ConstViewGetter<TGetView, TContext, TView>
 */
template <typename TContext, typename TView, typename TGetView>
  requires ConstViewGetter<TGetView, TContext, TView>
class const_context_view_range {
public:
  using context     = TContext;
  using view        = TView;
  using view_getter = TGetView;
  using iterator    = const_context_view_iterator<TContext, TView, TGetView>;

  const_context_view_range(const TContext& context)
  : context_(context) {
  }

  iterator begin() const { return iterator(context_, 0); }
  iterator end() const { return iterator(context_, context_.size()); }

  size_t size() const { return context_.size(); }

  const TView& operator[](size_t i) const {
    TGetView getter{};
    return getter(i, context_);
  }

private:
  const TContext& context_;
};

/**
 * @brief Concept to check if a type is a specialization of context_view_range.
 *
 * This concept matches any type that is a specialization of the context_view_range
 * class template, regardless of the template parameters used.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept ContextViewRange = requires {
  typename T::context;
  typename T::view;
  typename T::view_getter;
  typename T::iterator;
} && requires(T& range) {
  range.begin();
  range.end();
  range.size();
  range[0];
};

/**
 * @brief Concept to check if a type is a specialization of const_context_view_range.
 *
 * This concept matches any type that is a specialization of the const_context_view_range
 * class template, regardless of the template parameters used.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept ConstContextViewRange = requires {
  typename T::context;
  typename T::view;
  typename T::view_getter;
  typename T::iterator;
} && requires(const T& const_range) {
  const_range.begin();
  const_range.end();
  const_range.size();
  const_range[0];
};

/**
 * @brief Concept to check if a type is any kind of context view range.
 *
 * This concept matches both mutable and const context view ranges.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept AnyContextViewRange = ContextViewRange<T> || ConstContextViewRange<T>;
} // namespace context_view
} // namespace funcflow
