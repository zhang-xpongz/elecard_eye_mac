add_library(streameye_io INTERFACE)
target_include_directories(streameye_io INTERFACE include)
target_compile_features(streameye_io INTERFACE cxx_std_17)

if(BUILD_TESTING)
  add_executable(io_tests tests/hello_test.cpp)
  target_link_libraries(io_tests PRIVATE streameye_io GTest::gtest_main)
  gtest_discover_tests(io_tests)
endif()
