#pragma once
/**
 * @file task_utils.hpp
 * @brief Utility functions and concepts for task management.
 */
#include <algorithm>
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef STORE_EXCEPTION
  //#define STORE_EXCEPTION(X) X = std::make_exception_ptr(std::current_exception());
  #define STORE_EXCEPTION(X) X = std::current_exception();
#endif


using namespace std::string_literals;

namespace funcflow {
namespace task_utils {


/**
 * @brief Get the types names object
 * @tparam Types 
 * @param prefix the prefix to add to each type name
 * @remark If RTTI is not available, fallback to index based naming
 * @return std::vector<std::string> 
 * #TODO consider demangling the type names if RTTI is available
 */
template <typename... Types>
std::vector<std::string> get_types_names(const std::string& prefix) {
   std::vector<std::string> types_names;
   types_names.reserve(sizeof...(Types));
#ifdef __cpp_rtti
  using namespace std::string_literals;
  size_t i = 0;
  (types_names.push_back(prefix + " "s + typeid(Types).name() + "["s + std::to_string(i++) + "]"s), ...);
#else
  for (size_t i = 0; i < sizeof...(Types); ++i) {
    types_names.push_back(prefix + " [" + std::to_string(i) + "]");
  }
#endif
  return types_names;
}


/*
 * @brief Get the type name of a given type T
 * If RTTI is enabled, it returns the type name using typeid.
 * Otherwise, it returns the provided fallback name. *
 * @tparam T The type for which to get the name.
 * @param fallback_name The name to return if RTTI is not available.
 * @return std::string The type name or the fallback name.
 */
template <typename T>
std::string  get_type_name(const std::string& fallback_name) {
#ifdef __cpp_rtti
  return typeid(T).name();
#else
  return fallback_name;
#endif
}



// Delete all elements of a std::vector except the element of index i
template <typename T>
void reduce_to_one(std::vector<T>& v, size_t index) {
  if (index >= v.size())
    throw std::out_of_range("index out of range");
  if (index != 0)
    std::swap(v[0], v[index]);
  v.resize(1); // delete but keep only the first element
}


// A simple Task structure that can be used to encapsulate a function with its arguments
// That return a value of type Result when called with the provided arguments
template <typename Result, typename... Args>
struct Task {
  std::function<Result(Args...)> func;

  // Constructor: accepts any callable that matches Result(Args...)
  template <typename Callable>
  Task(Callable&& f) : func(std::forward<Callable>(f)) {
  }

  // Single operator() that handles all cases by accepting arguments that can be converted to Args
  // This works with the stored std::function which already handles forwarding correctly
  Result operator()(Args... args) const {
    return func(std::move(args)...);
  }
};


// A concept to check if a Task can be invoked with the provided arguments and have
// a return type convertible to Result  (equivalent to c++ 23 std::invoke_r<R>)
template <typename Task, typename Result, typename... Args>
concept invokable_with_result = requires(Task t, Args&&... args) {
  { t(std::forward<Args>(args)...) } -> std::convertible_to<Result>;
};

// A Task that applies an indexed filter to a vector of elements and runs a modify in place
// function on the filtered elements (was used in sov vs vos tests)
template <typename T>
struct IndexedFilterTask {
  std::function<bool(const T&, size_t)> filter;
  std::function<void(size_t, T&)>       func;
  // Constructor: accepts any callable that matches void(size_t, T&) 
  // and the filter by default accepts all elements
  IndexedFilterTask(
    std::function<void(size_t, T&)>       f,
    std::function<bool(const T&, size_t)> flt = [](const T&, size_t) { return true; }) :
  func(std::move(f)), filter(std::move(flt)) {
  }

  void operator()(size_t index, T& cell) const {
    if (filter(cell, index)) {
      func(index, cell);
    }
  }
};
} // namespace task_utils
} // namespace funcflow
