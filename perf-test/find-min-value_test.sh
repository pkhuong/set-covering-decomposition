#!/bin/bash

set -e

CHECKOUT_A="$1"
CHECKOUT_B="${2:-$(git rev-parse HEAD)}"

if [ $# -ge 1 ];
then
    shift;
fi
if [ $# -ge 1 ];
then
    shift;
fi

bazel build -c opt "$@" perf-test:find-min-value_test

rm -r perf-test-worktrees/find-min-value-a || true;
mkdir -p perf-test-worktrees/find-min-value-a;

if [ -z "$CHECKOUT_A" -o "z$CHECKOUT_A" = 'z-' ];
then
    CHECKOUT_A="current"
    bazel build -c opt "$@" perf-test:libbase-find-min-value.so
    LIB_A=$(readlink -f bazel-bin/perf-test/libbase-find-min-value.so)
else
    git clone --shared . perf-test-worktrees/find-min-value-a;

    pushd perf-test-worktrees/find-min-value-a
    git reset --hard "$CHECKOUT_A"
    bazel --batch build -c opt "$@" perf-test:libbase-find-min-value.so
    LIB_A=$(readlink -f bazel-bin/perf-test/libbase-find-min-value.so)
    popd
fi

rm -r perf-test-worktrees/find-min-value-b || true;
mkdir -p perf-test-worktrees/find-min-value-b;
git clone --shared . perf-test-worktrees/find-min-value-b

pushd perf-test-worktrees/find-min-value-b
git reset --hard "$CHECKOUT_B"
bazel --batch build -c opt "$@" perf-test:libbase-find-min-value.so
LIB_B=$(readlink -f bazel-bin/perf-test/libbase-find-min-value.so)
popd

bazel shutdown;

echo "A: ${CHECKOUT_A} B: ${CHECKOUT_B}"

(set -x; time bazel-bin/perf-test/find-min-value_test \
              --lib_a="$LIB_A" \
              --fn_a=MakeFindMinValue \
              --lib_b="$LIB_B" \
              --fn_b=MakeFindMinValue \
              --fn_a_lte=true)
RET=$?

rm -r perf-test-worktrees/find-min-value-a perf-test-worktrees/find-min-value-b;

exit $RET
