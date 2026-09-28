#!/usr/bin/env bash
# Export a self-contained AUR source directory from the patch-series checkout.
set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: %s NEW_DIRECTORY\n' "$0" >&2
    exit 2
fi

arch_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
destination="$1"
if [[ -e "$destination" ]]; then
    printf 'Destination must not exist: %s\n' "$destination" >&2
    exit 1
fi

mkdir -- "$destination"
cp -- "$arch_dir/PKGBUILD" "$destination/PKGBUILD"
# Dereference local symlinks: an AUR git checkout cannot resolve paths in
# the parent OpenMechrevo repository.
shopt -s nullglob
for source_file in "$arch_dir"/*.patch "$arch_dir/dkms.conf" "$arch_dir/60-mechrevo.rules"; do
    cp -L -- "$source_file" "$destination/$(basename -- "$source_file")"
done
(
    cd -- "$destination"
    makepkg --printsrcinfo > .SRCINFO
)
printf 'Standalone AUR source directory: %s\n' "$destination"
