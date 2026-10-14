cmake_minimum_required(VERSION 3.16)
project(bvi CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
  COMPILE_BMORE
  "Whether to also compile bmore binary or not."
  ON
)

include(CheckIncludeFileCXX)
include(FetchContent)
include(FindCurses)

if(NOT ${CURSES_FOUND})
  message(FATAL_ERROR "Could not find Curses library on your platform.")
endif()
if(${CURSES_HAVE_NCURSES_H})
  set(HAVE_NCURSES_H 1)
endif()
if(${CURSES_HAVE_CURSES_H})
  set(HAVE_CURSES_H 1)
endif()

fetchcontent_declare(
  PeeloXdg
  GIT_REPOSITORY
    https://github.com/peelonet/peelo-xdg.git
  GIT_TAG
    v1.2.1
)
fetchcontent_makeavailable(PeeloXdg)

check_include_file_cxx(fcntl.h HAVE_FCNTL_H)
check_include_file_cxx(unistd.h HAVE_UNISTD_H)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in
  ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
)

add_executable(
  bvi
  bvi.cpp
  comm.cpp
  edit.cpp
  io.cpp
  re.cpp
  recomp.cpp
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
    bvi.1
  DESTINATION
    ${CMAKE_INSTALL_MANDIR}/man1
  COMPONENT
    doc
)

if(COMPILE_BMORE)
  add_executable(
    bmore
    bmore.cpp
    bm_unix.cpp
    recomp.cpp
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
      ${CMAKE_INSTALL_MANDIR}/man1
    COMPONENT
      doc
  )
endif()
