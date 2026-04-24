cmake_minimum_required(VERSION 3.10)
project(Lab1Preprocessor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(preprocessor
  preprocessor.cpp
  source_cleaner.cpp
)

if(MSVC)
  target_compile_options(preprocessor PRIVATE /W3)
else()
  target_compile_options(preprocessor PRIVATE -Wall -Wextra)
endif()
