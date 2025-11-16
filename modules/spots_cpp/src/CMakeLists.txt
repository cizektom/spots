file(GLOB_RECURSE HEADER_SPOTS_CORE_LIST CONFIGURE_DEPENDS "${SpotsCpp_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE SOURCE_SPOTS_CORE_LIST CONFIGURE_DEPENDS "${SpotsCpp_SOURCE_DIR}/src/*.cpp")

find_package(Threads REQUIRED)


add_library(spots_core ${SOURCE_SPOTS_CORE_LIST} ${HEADER_SPOTS_CORE_LIST})
target_include_directories(spots_core PUBLIC ../include)
target_link_libraries(spots_core PRIVATE Threads::Threads)
target_compile_features(spots_core PUBLIC cxx_std_20)
set_target_properties(spots_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_options(spots_core PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)