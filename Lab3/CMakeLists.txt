cmake_minimum_required(VERSION 3.10)
project(Lab3Parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(parser
  parser_main.cpp
  parser.cpp
  ast.cpp
  ${CMAKE_SOURCE_DIR}/../Lab2/lexer.cpp
  ${CMAKE_SOURCE_DIR}/../Lab1/source_cleaner.cpp
)

target_include_directories(parser PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/../Lab2
  ${CMAKE_SOURCE_DIR}/../Lab1
)

if(MSVC)
  target_compile_options(parser PRIVATE /W3 /utf-8)
else()
  target_compile_options(parser PRIVATE -Wall -Wextra)
endif()
