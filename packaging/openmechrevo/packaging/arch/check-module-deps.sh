#!/usr/bin/env bash
# Check a freshly built module tree without modifying the host's module indexes.
set -euo pipefail

if (( $# != 2 )); then
    printf 'Usage: %s BUILD_DIRECTORY KERNEL_VERSION\n' "$0" >&2
    exit 2
fi

build_dir="$1"
kernel_version="$2"
command -v depmod >/dev/null
command -v modinfo >/dev/null
root="$(mktemp -d)"
trap 'rm -rf -- "$root"' EXIT
modules_dir="$root/lib/modules/$kernel_version/extra"
mkdir -p -- "$modules_dir"
: > "$root/lib/modules/$kernel_version/modules.order"
: > "$root/lib/modules/$kernel_version/modules.builtin"
: > "$root/lib/modules/$kernel_version/modules.builtin.modinfo"
mapfile -d '' modules < <(find "$build_dir" -type f -name '*.ko' -print0)
if (( ${#modules[@]} < 2 )); then
    printf 'Expected multiple compiled modules in %s\n' "$build_dir" >&2
    exit 1
fi
for module in "${modules[@]}"; do
    cp -- "$module" "$modules_dir/$(basename -- "$module")"
done
for module in tuxedo_keyboard uniwill_wmi; do
    [[ -f "$modules_dir/$module.ko" ]] || { printf 'Missing %s.ko\n' "$module" >&2; exit 1; }
done
# -b confines writes to the temporary root; -a indexes all staged modules.
depmod -b "$root" -a "$kernel_version"
if modinfo -F depends "$modules_dir/tuxedo_keyboard.ko" | tr ',' '\n' | grep -qx uniwill_wmi; then
    printf 'tuxedo_keyboard still depends on uniwill_wmi\n' >&2
    exit 1
fi
printf 'Isolated depmod passed for %s (%s modules).\n' "$kernel_version" "${#modules[@]}"
