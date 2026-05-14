cmake_minimum_required(VERSION 3.10)
project(Lab4Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(compiler
  compiler_main.cpp
  semantic.cpp
  ${CMAKE_SOURCE_DIR}/../Lab3/ast.cpp
  ${CMAKE_SOURCE_DIR}/../Lab3/parser.cpp
  ${CMAKE_SOURCE_DIR}/../Lab2/lexer.cpp
  ${CMAKE_SOURCE_DIR}/../Lab1/source_cleaner.cpp
)

target_include_directories(compiler PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/../Lab3
  ${CMAKE_SOURCE_DIR}/../Lab2
  ${CMAKE_SOURCE_DIR}/../Lab1
)

if(MSVC)
  target_compile_options(compiler PRIVATE /W3 /utf-8)
else()
  target_compile_options(compiler PRIVATE -Wall -Wextra)
endif()
