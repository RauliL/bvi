add_executable(
  bvi
  bvi.cpp
  colors.cpp
  comm.cpp
  edit.cpp
  io.cpp
  re.cpp
  recomp.cpp
  regex.cpp
  set.cpp
)

target_compile_features(
  bvi
  PRIVATE
    cxx_std_20
)

target_compile_options(
  bvi
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Werror>
)

target_include_directories(
  bvi
  PRIVATE
    $<BUILD_INTERFACE:${CURSES_INCLUDE_DIR}>
)

target_link_libraries(
  bvi
  PRIVATE
    ${CURSES_LIBRARIES}
    PeeloXdg
)

install(
  TARGETS
    bvi
  RUNTIME DESTINATION
    bin
)

install(
  FILES
    bvi.desktop
  DESTINATION
    share/applications
)

enable_testing()
add_executable(
  re_test
  tests/re_test.cpp
  recomp.cpp
  regex.cpp
)
target_compile_features(
  re_test
  PRIVATE
    cxx_std_20
)
target_compile_options(
  re_test
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Werror>
)
target_include_directories(
  re_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
  NAME
    re_test
  COMMAND
    re_test
)
