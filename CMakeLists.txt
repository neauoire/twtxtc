cmake_minimum_required(VERSION 3.5)
project(twtxtc C)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_STANDARD 11)

if (MSVC)
  # meh
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (MSVC)

add_executable(twtxtc cJSON/cJSON.c cJSON/cJSON_Utils.c twtxt.c)
