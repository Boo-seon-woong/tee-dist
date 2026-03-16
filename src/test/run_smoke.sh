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

OUTPUT="$(printf 'write alpha hello\nread alpha\nupdate alpha world\nread alpha\ndelete alpha\nread alpha\nquit\n' | ./bin/cn --config build/config/cn.conf)"
printf '%s\n' "$OUTPUT"

grep -q "ok alpha" <<<"$OUTPUT"
grep -q "value alpha hello" <<<"$OUTPUT"
grep -q "value alpha world" <<<"$OUTPUT"
grep -q "not_found alpha" <<<"$OUTPUT"
