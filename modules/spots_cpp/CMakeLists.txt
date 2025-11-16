cmake_minimum_required(VERSION 3.16)
project(SpotsCpp VERSION 1.0 LANGUAGES CXX)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

include(FetchContent)

add_subdirectory(src)
add_subdirectory(python)