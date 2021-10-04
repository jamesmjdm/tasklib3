# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project ("tasklib3")


add_executable (tasklib3 "src/main.cpp" "src/tasklib3.cpp")
target_compile_features(tasklib3 PUBLIC cxx_std_20)
