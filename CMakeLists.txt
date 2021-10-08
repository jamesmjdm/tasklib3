# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project ("tasklib3")
add_library(tasklib3 STATIC "src/tasklib3.cpp")
target_include_directories(tasklib3 PUBLIC "include")

project("tasklib3_test")
add_executable (tasklib3_test "src/main.cpp")
target_include_directories(tasklib3_test PUBLIC "include")
target_link_libraries(tasklib3_test PUBLIC tasklib3)
