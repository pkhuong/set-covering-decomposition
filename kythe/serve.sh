#!/bin/sh
set -e

BASE=$(dirname $(dirname $(readlink -f "$0")))

if [ ! -d "$BASE/kythe_out/index" ];
then
    echo "Run kythe/index.sh first to generate the index.";
    exit 1;
fi

BUILT=$(docker build -q -t kythe "$BASE/kythe")
PORT=${1:-8080}

exec docker run --rm \
     -v "$BASE/kythe_out/index:/index" \
     -p $PORT:$PORT $BUILT \
     /opt/kythe/tools/http_server \
     --listen 0.0.0.0:$PORT \
     --serving_table "/index/serving_table" \
     --public_resources /opt/kythe/web/ui/
