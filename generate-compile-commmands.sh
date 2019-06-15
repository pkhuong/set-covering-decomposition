#!/bin/bash

if ! $(${CC:-cc} -v 2>&1 | grep -q clang);
then
    echo "Make sure CC is set to clang. Current value CC=$CC";
fi
   
CC=${CC:-cc} bazel build "$@" :compile_commands
sed -e "s|__EXEC_ROOT__|$(bazel info execution_root)|" < "$(bazel info bazel-bin)/compile_commands.json" > "$(bazel info workspace)/compile_commands.json"

echo "Compilation db built at $(bazel info workspace)/compile_commands.json. "
echo "You may have to 'CC=$CC bazel build $@ :all' to move/generate source to the right place."
