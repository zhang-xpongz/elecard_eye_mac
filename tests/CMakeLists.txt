set(FIXTURES_DIR ${CMAKE_BINARY_DIR}/fixtures)
set(FIXTURE_FILES
  ${FIXTURES_DIR}/01_idr_only.h264
  ${FIXTURES_DIR}/02_ipbb.h264
  ${FIXTURES_DIR}/03_with_b_refs.h264
  ${FIXTURES_DIR}/04_short.mp4
)

add_custom_command(
  OUTPUT ${FIXTURE_FILES}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gen_fixtures.sh ${FIXTURES_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_fixtures.sh
  COMMENT "Generating H.264 test fixtures via ffmpeg"
  VERBATIM
)

add_custom_target(fixtures ALL DEPENDS ${FIXTURE_FILES})
