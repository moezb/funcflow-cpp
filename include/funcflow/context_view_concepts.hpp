#pragma once

/**
 * @file context_view_concepts.hpp
 * @brief Defines concepts for context views and getters.
 */

#include <concepts>
#include <type_traits>

namespace funcflow {
namespace context_view {
// A marco to define a concept that checks if a class has a member of a specific
// type
#ifndef DEFINE_HAS_MEMBER_OF_TYPE
#define DEFINE_HAS_MEMBER_OF_TYPE(concept_name, member_name, member_type)      \
  template <typename T>                                                        \
  concept concept_name = requires(T t) {                                       \
    { t.member_name } -> std::same_as<member_type>;                            \
  };
#endif // DEFINE_HAS_MEMBER_OF_TYPE

// A concept to be used to ensure a type is movable and move assignable.
// This is used to impose how an operation (functor of simple method/function)
// can return its result.
template <typename T>
concept MovableResult = std::is_move_constructible_v<T> &&
                        std::is_move_assignable_v<T>;

// Concept for a function that can retrieve a view from a context at a specific index
template <typename F, typename TContext, typename TView>
concept ViewGetter = requires(const F getter, std::size_t idx, TContext& ctx) {
  { getter(idx, ctx) } -> std::convertible_to<TView&>;
};

// Concept for a function that can retrieve a const view from a context at a specific index
template <typename F, typename TContext, typename TView>
concept ConstViewGetter = requires(const F getter, std::size_t idx, TContext& ctx) {
  { getter(idx, ctx) } -> std::convertible_to<const TView&>;
};

// Concept for a void modifier functor that modifies a view in place
// This is used to apply a transformation or modification to each view in the context
// without returning a value.
// It is used in the `iterate_and_visit` method to apply a transformation
// to each view in the context.
// The functor should have a method that takes a view and modifies it in place
// and returns void.
template <typename T, typename TView>
concept VoidModifierFunctor = requires(T void_modifier_functor, TView& view) {
  { void_modifier_functor(view) } -> std::same_as<void>;
  T{};
};

// Helper to detect if a method with a specific signature exists
// This is used to check if a class has a method with a specific signature
// and can be used to create concepts that check for the presence of such methods.
template <typename T, typename Ret, typename... Args>
concept HasMethodWithSignature = requires(T obj, Args... args) {
  { obj.someMethod(args...) } -> std::same_as<Ret>;
};
} // namespace context_view
} // namespace funcflow
