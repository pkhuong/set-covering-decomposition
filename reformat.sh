#!/bin/sh

clang-format-4.0 -i --style=google *.cc *.h *.c        \
                 $(find bench bench/internal perf-test \
                        -type f                        \
                        \( -name '*\.c'                \
                        -o -name '*\.cc'               \
                        -o -name '*\.h'                \
                        \))
