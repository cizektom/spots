FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.13.1
)
FetchContent_MakeAvailable(pybind11)
pybind11_add_module(_cpp wrapper.cpp)

target_link_libraries(_cpp PRIVATE spots_core)
target_compile_features(_cpp PUBLIC cxx_std_20)
target_compile_options(_cpp PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
target_compile_definitions(_cpp PRIVATE VERSION_INFO=${SPROUTS_VERSION_INFO})