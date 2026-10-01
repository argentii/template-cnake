#!/bin/sh
# Golden replay tests. Each tests/golden/NAME.case holds "SEED SCRIPT".
# Usage: run.sh DRIVER [--update] [extra driver args...]
set -u
driver=$1; shift
update=0
if [ "${1:-}" = "--update" ]; then update=1; shift; fi
dir=$(dirname "$0")
fail=0
for c in "$dir"/*.case; do
    name=$(basename "$c" .case)
    read -r seed script < "$c"
    out="$dir/$name.out"
    actual=$("$driver" --script "$script" --no-delay --seed "$seed" "$@") || { echo "FAIL $name (driver exited $?)"; fail=1; continue; }
    if [ $update = 1 ]; then printf '%s\n' "$actual" > "$out"; echo "updated $name"; continue; fi
    if printf '%s\n' "$actual" | diff -u "$out" - > /dev/null; then
        echo "ok   $name"
    else
        echo "FAIL $name"; printf '%s\n' "$actual" | diff -u "$out" - | head -30; fail=1
    fi
done
exit $fail
