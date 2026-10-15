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

check_include_file_cxx(fcntl.h HAVE_FCNTL_H)
check_include_file_cxx(unistd.h HAVE_UNISTD_H)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in
  ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
)

fetchcontent_declare(
  PeeloXdg
  GIT_REPOSITORY
    https://github.com/peelonet/peelo-xdg.git
  GIT_TAG
    v1.2.1
)
fetchcontent_makeavailable(PeeloXdg)

add_subdirectory(bvi)
if(COMPILE_BMORE)
  add_subdirectory(bmore)
endif()
add_subdirectory(icons)
