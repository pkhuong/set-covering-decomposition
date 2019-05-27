#!/bin/bash

set -e

DAT_FILES=$(BAZEL_USE_LLVM_NATIVE_COVERAGE=1 GCOV=llvm-profdata-4.0 CC=clang-4.0 bazel coverage --cache_test_results=no "$@" | egrep '/coverage.dat$')

TEMP_DIR=$(mktemp -d "bazel-coverage.XXXXXXXXXX")

touch "$TEMP_DIR/merged.dat"

echo "Merging profiles $DAT_FILES"

for FILE in $DAT_FILES;
do
    mv "$TEMP_DIR/merged.dat" "$TEMP_DIR/tmp.dat";
    llvm-profdata-4.0 merge -sparse "$FILE" "$TEMP_DIR/tmp.dat" -o "$TEMP_DIR/merged.dat";
done

BIN_FILES=$(for FILE in $DAT_FILES;
           do
               dirname $FILE | sed -e 's|testlogs/|bin/|'
           done)

LIBRARIES=$(for BIN in $BIN_FILES;
            do
                cat $BIN.runfiles_manifest | cut -d' ' -f 2 |
                    egrep "\.so$" | grep -v '/_solib_k8/lib_external_';
            done | sort | uniq);

echo "Generating report"

# When we upgrade to > 4.0, add  -ignore-filename-regex='(.*_test\.cc$)|(^external/)' 

llvm-cov-4.0 show -format=html -output-dir=coverage/-instr-profile "$TEMP_DIR/merged.dat" $BIN_FILES $(echo "$LIBRARIES" |  xargs -n 1 -I xxx echo -n "-object xxx ") 

rm "$TEMP_DIR/merged.dat"
rm "$TEMP_DIR/tmp.dat"
rmdir "$TEMP_DIR"
