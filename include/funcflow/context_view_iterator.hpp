#pragma once

/**
 * @file context_view_iterator.hpp
 * @brief Implements iterators for context views.
 */

#include <iterator>
#include "funcflow/context_view_concepts.hpp"

namespace funcflow {
namespace context_view {
/**
 * @brief A random-access iterator for views extracted from a context using a getter.
 * 
 * @tparam TContext The type of the context object from which views are extracted.
 * @tparam TView The type of the view returned by the iterator.
 * @tparam TGetView The type of the getter functor, which must satisfy the ViewGetter concept.
 * 
 * This iterator allows random-access traversal over views derived from a context object.
 * The view at a given position is obtained by invoking the getter functor with the position
 * and the context. The iterator supports standard random-access operations such as increment,
 * decrement, addition, subtraction, comparison, and indexing.
 * The iterator returns a reference (NOT A const ref) to the view at the current position so it can
 * be used to modify the view in place.
 * @note This is intended to be in pipeline closures where the TContext ref passed in the constructor
 * remains valid.
 
 * Example usage:
 * @code
 * context_view_iterator<MyContext, MyView, MyGetter> it(context, 0);
 * MyView view = *it;
 * ++it;
 * @endcode
 */
template <typename TContext, typename TView, typename TGetView>
  requires ViewGetter<TGetView, TContext, TView>
class context_view_iterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type        = TView;
  using difference_type   = std::ptrdiff_t;
  using pointer           = TView*;
  using reference         = TView&;

  context_view_iterator() : context_(nullptr), position_(0) {
  }

  context_view_iterator(TContext& context, size_t position = 0)
  : context_(&context), position_(position) {
  }

  reference operator*() const {
    TGetView getter{};
    return getter(position_, *context_);
  }

  pointer operator->() const {
    return &(**this);
  }

  context_view_iterator& operator++() {
    ++position_;
    return *this;
  }

  context_view_iterator operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }

  context_view_iterator& operator--() {
    --position_;
    return *this;
  }

  context_view_iterator operator--(int) {
    auto tmp = *this;
    --(*this);
    return tmp;
  }

  context_view_iterator& operator+=(difference_type n) {
    position_ += n;
    return *this;
  }

  context_view_iterator operator+(difference_type n) const {
    auto tmp = *this;
    tmp += n;
    return tmp;
  }

  friend context_view_iterator operator+(difference_type n, const context_view_iterator& it) {
    return it + n;
  }

  context_view_iterator& operator-=(difference_type n) {
    position_ -= n;
    return *this;
  }

  context_view_iterator operator-(difference_type n) const {
    auto tmp = *this;
    tmp -= n;
    return tmp;
  }

  difference_type operator-(const context_view_iterator& other) const {
    return position_ - other.position_;
  }

  reference operator[](difference_type n) const {
    TGetView getter{};
    return getter(position_ + n, *context_);
  }

  bool operator==(const context_view_iterator& other) const {
    return position_ == other.position_ && context_ == other.context_;
  }

  bool operator!=(const context_view_iterator& other) const {
    return !(*this == other);
  }

  bool operator<(const context_view_iterator& other) const {
    return position_ < other.position_;
  }

  bool operator>(const context_view_iterator& other) const {
    return other < *this;
  }

  bool operator<=(const context_view_iterator& other) const {
    return !(other < *this);
  }

  bool operator>=(const context_view_iterator& other) const {
    return !(*this < other);
  }

private:
  TContext* context_;
  size_t    position_;
};

/**
 * @brief A random-access iterator for const views extracted from a context.
 * 
 * @tparam TContext The type of the context object from which views are extracted.
 * @tparam TView The type of the view returned by the iterator.
 * @tparam TGetView The type of the getter functor, which must satisfy the ConstViewGetter concept.
 * 
 * This iterator allows random-access traversal over constant views derived from a context object.
 * Similar to context_view_iterator but provides read-only access.
 */
template <typename TContext, typename TView, typename TGetView>
  requires ConstViewGetter<TGetView, TContext, TView>
class const_context_view_iterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type        = TView;
  using difference_type   = std::ptrdiff_t;
  using pointer           = const TView*;
  using reference         = const TView&;

  const_context_view_iterator() : context_(nullptr), position_(0) {
  }

  const_context_view_iterator(const TContext& context, size_t position = 0)
  : context_(&context), position_(position) {
  }

  reference operator*() const {
    TGetView getter{};
    return getter(position_, *context_);
  }

  pointer operator->() const {
    return &(**this);
  }

  const_context_view_iterator& operator++() {
    ++position_;
    return *this;
  }

  const_context_view_iterator operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }

  const_context_view_iterator& operator--() {
    --position_;
    return *this;
  }

  const_context_view_iterator operator--(int) {
    auto tmp = *this;
    --(*this);
    return tmp;
  }

  const_context_view_iterator& operator+=(difference_type n) {
    position_ += n;
    return *this;
  }

  const_context_view_iterator operator+(difference_type n) const {
    auto tmp = *this;
    tmp += n;
    return tmp;
  }

  friend const_context_view_iterator operator+(difference_type n, const const_context_view_iterator& it) {
    return it + n;
  }

  const_context_view_iterator& operator-=(difference_type n) {
    position_ -= n;
    return *this;
  }

  const_context_view_iterator operator-(difference_type n) const {
    auto tmp = *this;
    tmp -= n;
    return tmp;
  }

  difference_type operator-(const const_context_view_iterator& other) const {
    return position_ - other.position_;
  }

  reference operator[](difference_type n) const {
    TGetView getter{};
    return getter(position_ + n, *context_);
  }

  bool operator==(const const_context_view_iterator& other) const {
    return position_ == other.position_ && context_ == other.context_;
  }

  bool operator!=(const const_context_view_iterator& other) const {
    return !(*this == other);
  }

  bool operator<(const const_context_view_iterator& other) const {
    return position_ < other.position_;
  }

  bool operator>(const const_context_view_iterator& other) const {
    return other < *this;
  }

  bool operator<=(const const_context_view_iterator& other) const {
    return !(other < *this);
  }

  bool operator>=(const const_context_view_iterator& other) const {
    return !(*this < other);
  }

private:
  const TContext* context_;
  size_t          position_;
};
} // namespace context_view
} // namespace funcflow

// Specialize std::iterator_traits for our custom iterators
// This is necessary to satisfy the requirements of the C++ Standard Library
// algorithms and containers that work with random access iterators.
namespace std {
template <typename TContext, typename TView, typename TGetView>
struct iterator_traits<funcflow::context_view::context_view_iterator<TContext, TView, TGetView>> {
  using iterator_category = std::random_access_iterator_tag;
  using value_type        = TView;
  using difference_type   = std::ptrdiff_t;
  using pointer           = TView*;
  using reference         = TView&;
};

template <typename TContext, typename TView, typename TGetView>
struct iterator_traits<funcflow::context_view::const_context_view_iterator<TContext, TView, TGetView>> {
  using iterator_category = std::random_access_iterator_tag;
  using value_type        = TView;
  using difference_type   = std::ptrdiff_t;
  using pointer           = const TView*;
  using reference         = const TView&;
};
} // namespace std
