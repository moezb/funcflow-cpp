add_executable(test_flags test_flags.cpp catch2/catch.hpp)
target_include_directories(test_flags PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_flags PRIVATE funcflow)

add_executable(test_scheduler test_scheduler.cpp catch2/catch.hpp)
target_include_directories(test_scheduler PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_scheduler PRIVATE funcflow)

add_executable(test_workflow test_workflow.cpp catch2/catch.hpp)
target_include_directories(test_workflow PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_workflow PRIVATE funcflow)
