add_library(streameye_model STATIC
  src/poc_calculator.cpp
  src/stream_session.cpp
)
target_include_directories(streameye_model PUBLIC include)
target_compile_features(streameye_model PUBLIC cxx_std_17)
target_link_libraries(streameye_model PUBLIC streameye_parser)

if(BUILD_TESTING)
  add_executable(model_tests
    tests/poc_calculator_test.cpp
    tests/stream_session_test.cpp
  )
  target_link_libraries(model_tests PRIVATE
    streameye_model
    GTest::gtest_main
  )
  target_compile_definitions(model_tests PRIVATE
    FIXTURES_DIR="${CMAKE_BINARY_DIR}/fixtures")
  add_dependencies(model_tests fixtures)
  gtest_discover_tests(model_tests)
endif()
