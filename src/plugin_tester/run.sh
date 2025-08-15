#!/bin/bash
set -e
set -x

set -euo pipefail

cleanup() {
  echo "Cleaning up..."
  kill "$pid"
  wait "$pid" 2>/dev/null || true # avoid zombie and ignore exit code
  echo "Cleaning up... and restoring display state"
  curl -X PATCH "${SERVER}/api/plugin?id=${PLUGIN_ID}"

}

trap 'cleanup; exit 0' INT TERM

ROOT=$(pwd)/../../


g++ -DHOST_DDP -I ${ROOT}/include -I . host_ddp.cpp ${ROOT}/src/plugins/Blop.cpp -o ddp

# enable DDD plugin
SERVER="http://192.168.1.112"
DDP_PLUGIN_ID=17
PLUGIN_ID=10

# your cleanup commands here

curl -X PATCH "${SERVER}/api/plugin?id=${DDP_PLUGIN_ID}"

./ddp & pid=$!
echo "ddp running (pid=$pid). In Emacs compilation buffer, press C-c C-c to stop."

# If run interactively (not via Emacs), allow any key to stop:
if [ -t 0 ]; then
  read -rsn1
  kill -- -"$pid"
else
  # In non-interactive mode, wait until we get a signal from Emacs
  wait "$pid" || true
fi

cleanup

echo "Ctrl-C to quit"
