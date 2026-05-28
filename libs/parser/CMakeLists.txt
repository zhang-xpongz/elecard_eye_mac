add_library(streameye_parser STATIC
  src/bit_reader.cpp
  src/nal_splitter.cpp
  src/ep3_strip.cpp
)
target_include_directories(streameye_parser PUBLIC include)
target_compile_features(streameye_parser PUBLIC cxx_std_17)

if(BUILD_TESTING)
  add_executable(parser_tests
    tests/bit_reader_test.cpp
    tests/nal_splitter_test.cpp
    tests/ep3_strip_test.cpp
  )
  target_link_libraries(parser_tests PRIVATE
    streameye_parser
    GTest::gtest_main
  )
  gtest_discover_tests(parser_tests)
endif()
