cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --ld-path=/usr/bin/mold")
# SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --ld-path=/usr/bin/mold")

project("byodb")

function(target_compile_link_options target)
  target_compile_options(${target} ${ARGN})
  target_link_options(${target} ${ARGN})
endfunction()


add_executable(byodb
  src/main.cpp
  src/data/btree.cpp
)
target_include_directories(byodb PUBLIC src)


target_compile_link_options(byodb PUBLIC -fdiagnostics-color=always)
target_compile_options(byodb PUBLIC -O0 -g -std=c++23)
target_compile_options(byodb PUBLIC -march=native)

target_compile_options(byodb PUBLIC -Wall -Wextra -Wconversion)
target_compile_options(byodb PUBLIC -Wno-unused-parameter -Wno-missing-field-initializers -Wno-missing-braces)
# target_compile_link_options(byodb PUBLIC -fno-omit-frame-pointer)
# target_compile_link_options(byodb PUBLIC -fsanitize=address )
target_compile_link_options(byodb PUBLIC -flto)
target_compile_link_options(byodb PUBLIC -fvisibility=hidden)

target_compile_definitions(byodb PUBLIC -DDEBUG)

# set_property(TARGET byodb PROPERTY UNITY_BUILD ON)
