cmake_minimum_required(VERSION 3.21)
project(PorkchopLite)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

set(CMAKE_CXX_STANDARD 20)

add_executable(PorkchopLite
        main.cpp
        type.hpp token.hpp
        compiler.hpp compiler.cpp
        local.hpp local.cpp
        lexer.hpp lexer.cpp
        parser.hpp parser.cpp
        tree.hpp tree.cpp
        diagnostics.hpp diagnostics.cpp
        unicode/unicode.hpp unicode/unicode.cpp
        unicode/unicode-id.cpp unicode/unicode-width.cpp
        assembler.hpp
        util.hpp
        descriptor.hpp
        common.hpp
        source.hpp source.cpp
        global.hpp global.cpp
)

add_library(lib lib/lib.c)