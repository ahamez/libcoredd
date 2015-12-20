include_directories("${PROJECT_SOURCE_DIR}/tests/gtest/include")

include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/tests")
add_library(gtest "${PROJECT_SOURCE_DIR}/tests/gtest/gtest-all.cc")
set_target_properties(gtest PROPERTIES COMPILE_FLAGS "-w")

set(SOURCES
  tests.cc
  test_cache.cc
  detail/test_hash_table.cc
  test_ptr.cc
  test_unique_table.cc
  test_variant.cc
  detail/test_next_power.cc
  detail/test_typelist.cc
  )

add_executable(tests ${SOURCES})
target_link_libraries(tests gtest ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_test("UnitTests" tests)
