#!/bin/sh

CC=clang-4.0 bazel test -c opt --copt='-fsanitize=address,undefined' --linkopt='-fsanitize=address,undefined' --linkopt=-lubsan "$@"
