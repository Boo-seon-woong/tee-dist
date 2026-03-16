#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

make >/dev/null

cleanup() {
  pkill -f "tee-dist/bin/mn --config tee-dist/build/config/mn1.conf" >/dev/null 2>&1 || true
  pkill -f "tee-dist/bin/mn --config tee-dist/build/config/mn2.conf" >/dev/null 2>&1 || true
  pkill -f "tee-dist/bin/mn --config tee-dist/build/config/mn3.conf" >/dev/null 2>&1 || true
}
trap cleanup EXIT

./bin/mn --config build/config/mn1.conf >/tmp/tee-dist-mn1.log 2>&1 &
./bin/mn --config build/config/mn2.conf >/tmp/tee-dist-mn2.log 2>&1 &
./bin/mn --config build/config/mn3.conf >/tmp/tee-dist-mn3.log 2>&1 &
sleep 1

run_case() {
  local cfg="$1"
  local start end
  start="$(date +%s%N)"
  {
    printf 'write beta payload\n'
    for i in $(seq 1 200); do
      printf 'read beta\n'
    done
    printf 'quit\n'
  } | ./bin/cn --config "$cfg" >/tmp/tee-dist-bench.out
  end="$(date +%s%N)"
  echo $(((end - start) / 1000))
}

ON_US="$(run_case build/config/cn.conf)"
OFF_US="$(run_case build/config/cn.cache-off.conf)"

echo "cache_on_total_us=$ON_US"
echo "cache_off_total_us=$OFF_US"
