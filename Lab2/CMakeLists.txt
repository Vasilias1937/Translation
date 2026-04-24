cmake_minimum_required(VERSION 3.10)
project(Lab2Lexer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(lexer
  lexer_main.cpp
  lexer.cpp
  ${CMAKE_SOURCE_DIR}/../Lab1/source_cleaner.cpp
)
target_include_directories(lexer PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/../Lab1
)
if(MSVC)
  target_compile_options(lexer PRIVATE /W3)
else()
  target_compile_options(lexer PRIVATE -Wall -Wextra)
endif()
