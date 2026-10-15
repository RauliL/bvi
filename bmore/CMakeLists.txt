add_executable(
  bmore
  bmore.cpp
  bm_unix.cpp
  ../bvi/recomp.cpp
)

target_compile_features(
  bmore
  PRIVATE
    cxx_std_20
)

target_compile_options(
  bmore
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Werror>
)

target_include_directories(
  bmore
  PRIVATE
    $<BUILD_INTERFACE:${CURSES_INCLUDE_DIR}>
)

target_link_libraries(
  bmore
  PRIVATE
    ${CURSES_LIBRARIES}
    PeeloXdg
)

install(
  TARGETS
    bmore
  RUNTIME DESTINATION
    bin
)

install(
  FILES
    bmore.1
  DESTINATION
    share/man/man1
  COMPONENT
    doc
)
