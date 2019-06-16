#!/bin/bash
# This script runs in the kythe container.
#
# The kzip container format is a zip file of files and compilation
# units. The compilation units are the unit of analysis; the files are
# only there so that analyses can know what goes in the compilation
# units.  We'll speed up the analysis and improve crash resilience by
# splitting up all the compilation units in their own kzip.
#
# We'll first unzip all the kzip in the same location:
# https://kythe.io/docs/kythe-kzip.html filenames are sha256 hashes of
# their contents.
#
# We'll then bundle together all the files, and create, for each
# compilation unit a new zip archive with the file and the unit.  The
# first pre-compression step saves time: there's a bunch of
# compilation units, and zip is an appendable format.

set -e

# Extract all source kzips in extracted_roots.
mkdir -p /tmp/extracted_roots;
pushd /tmp/extracted_roots;
find /compilations -name '*\.kzip' -exec unzip -o {} \;;
popd

# Copy the roots' contents in consolidated_root
mkdir -p /tmp/consolidated_root/root;
pushd /tmp/consolidated_root
find /tmp/extracted_roots/* -maxdepth 0 -type d \
     -exec mv {}/units {}/files /tmp/consolidated_root/root \;;
popd

# Build the base `files` archive.
SCRIPT=$(cat <<'EOF'
cp /tmp/split_kzip/base_files.zip "/tmp/split_kzip/$(basename $0).kzip" && 
zip "/tmp/split_kzip/$(basename $0).kzip" "$0";
EOF
)

echo "Building split .kzip."
mkdir -p /tmp/split_kzip;
pushd /tmp/consolidated_root;
zip /tmp/split_kzip/base_files.zip root/files/*;
find root/units -type f -exec sh -c "$SCRIPT" {} \;
popd

# Probably makes sense to parallelise the previous step and merge it
# with this last analysis step.
echo "Indexing kzips."
find /tmp/split_kzip -name '*\.kzip' -print0 | \
    parallel -0 -L 1 /opt/kythe/indexers/cxx_indexer | \
    /opt/kythe/tools/dedup_stream > /index/graphstore;
