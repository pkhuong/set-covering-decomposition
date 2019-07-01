#!/bin/sh

clang-format-4.0 -i --style=google *.cc *.h *.c bench/*.h bench/*.cc
