#!/usr/bin/env bash
# Manual AUR DKMS trial; do not use as a boot-time service.
set -euo pipefail

usage() {
    printf 'Usage: %s --preflight | --build | --install OLD_TUXEDO_PACKAGE | --check-loaded\n' "$0" >&2
    printf '  --install needs the exact installed-version package file for rollback.\n' >&2
    exit 2
}

arch_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
mode="${1:---preflight}"
case "$mode" in
    --preflight|--build|--check-loaded) (( $# == 1 || ( $# == 0 && "$mode" == --preflight ) )) || usage ;;
    --install) (( $# == 2 )) || usage ;;
    *) usage ;;
esac

for command in pacman dkms modinfo; do
    command -v "$command" >/dev/null || { printf 'Missing %s\n' "$command" >&2; exit 1; }
done

printf 'Current kernel: %s\n' "$(uname -r)"
printf 'Board: %s\n' "$(< /sys/class/dmi/id/board_name)"
printf '\nInstalled driver packages:\n'
pacman -Q tuxedo-drivers-dkms mechrevo-drivers-dkms 2>/dev/null || true
printf '\nInstalled DKMS versions:\n'
dkms status
printf '\nLoaded driver modules:\n'
for module in tuxedo_keyboard tuxedo_io uniwill_wmi clevo_wmi tuxedo_compatibility_check; do
    if [[ -d "/sys/module/$module" ]]; then
        printf '%s: loaded\n' "$module"
    fi
done
printf '\nCurrently selected module file: '
modinfo -n tuxedo_keyboard || true

if [[ "$mode" == --preflight ]]; then
    printf '\nNo package or module was changed.\n'
    exit 0
fi

if [[ "$mode" == --check-loaded ]]; then
    printf '\nRead-only check of the currently loaded driver (run only after a planned reboot):\n'
    if [[ ! -d /sys/module/tuxedo_keyboard ]]; then
        printf 'tuxedo_keyboard is not loaded.\n' >&2
        exit 1
    fi
    sysfs=/sys/devices/platform/tuxedo_keyboard
    if [[ ! -d "$sysfs" ]]; then
        printf 'Missing %s\n' "$sysfs" >&2
        exit 1
    fi
    for attribute in perf_mode tdp_spl_min tdp_spl_max tcc_offset_min tcc_offset_max dgpu_power; do
        if [[ -r "$sysfs/$attribute" ]]; then
            printf '%s: ' "$attribute"
            IFS= read -r value < "$sysfs/$attribute" || true
            printf '%s\n' "${value:-<empty>}"
        fi
    done
    printf 'No sysfs value was written; this does not validate fan writes or dGPU hot switching.\n'
    exit 0
fi

for command in makepkg patch sha256sum; do
    command -v "$command" >/dev/null || { printf 'Missing %s\n' "$command" >&2; exit 1; }
done
if (( EUID == 0 )); then
    printf 'Run makepkg as your normal user, not root.\n' >&2
    exit 1
fi

if [[ "$mode" == --install ]]; then
    old_package="$2"
    [[ -f "$old_package" ]] || { printf 'Rollback package not found: %s\n' "$old_package" >&2; exit 1; }
    old_archive="$(pacman -Qp -- "$old_package")" || exit 1
    [[ "$old_archive" == tuxedo-drivers-dkms\ * ]] || {
        printf 'Rollback archive is not tuxedo-drivers-dkms: %s\n' "$old_archive" >&2
        exit 1
    }
    if old_installed="$(pacman -Q tuxedo-drivers-dkms 2>/dev/null)"; then
        [[ "$old_archive" == "$old_installed" ]] || {
            printf 'Rollback archive %s does not match installed %s\n' "$old_archive" "$old_installed" >&2
            exit 1
        }
    elif old_installed="$(pacman -Q mechrevo-drivers-dkms 2>/dev/null)"; then
        printf 'Replacing an installed Mechrevo driver: %s\n' "$old_installed"
    else
        printf 'Neither supported driver package is installed; refusing replacement.\n' >&2
        exit 1
    fi
    printf 'Rollback archive: %s\n' "$old_package"
    sha256sum -- "$old_package"
fi

build_dir="$(mktemp -d "${TMPDIR:-/tmp}/mechrevo-aur-trial.XXXXXXXX")"
"$arch_dir/export-aur.sh" "$build_dir/aur"
(
    cd -- "$build_dir/aur"
    makepkg --verifysource
    makepkg --cleanbuild
)
package=("$build_dir"/aur/mechrevo-drivers-dkms-*.pkg.tar.*)
if (( ${#package[@]} != 1 )) || [[ ! -f "${package[0]}" ]]; then
    printf 'Exactly one built DKMS package expected in %s\n' "$build_dir/aur" >&2
    exit 1
fi
printf '\nBuilt package: %s\n' "${package[0]}"
pacman -Qp -- "${package[0]}"
printf 'Build directory retained for inspection: %s\n' "$build_dir"
if [[ "$mode" == --build ]]; then
    printf 'Package built only. Nothing installed or loaded.\n'
    exit 0
fi

printf '\nThe following transaction replaces %s.\n' "$old_installed"
printf 'Existing loaded modules will NOT be unloaded. Reboot manually in a maintenance window.\n'
printf 'Rollback (if needed): sudo pacman -U -- %q\n' "$old_package"
printf 'Type INSTALL MECHREVO to authorize the pacman transaction: '
IFS= read -r confirmation
[[ "$confirmation" == 'INSTALL MECHREVO' ]] || { printf 'Installation cancelled.\n'; exit 1; }
sudo pacman -U -- "${package[0]}"
printf '\nPost-transaction package/DKMS status (not proof that new modules are loaded):\n'
pacman -Q mechrevo-drivers-dkms
dkms status -m mechrevo-drivers
for modules_dir in /usr/lib/modules/*; do
    [[ -d "$modules_dir/build" ]] || continue
    kernel_version="${modules_dir##*/}"
    if ! dkms status -m mechrevo-drivers -v 4.22.3 -k "$kernel_version" | grep -q ': installed'; then
        printf 'DKMS module is not installed for %s; do not reboot into it.\n' "$kernel_version" >&2
        exit 1
    fi
    if ! depmod -n -a "$kernel_version" >/dev/null; then
        printf 'depmod check failed for %s; do not reboot into it.\n' "$kernel_version" >&2
        exit 1
    fi
done
printf '\nNo automatic module reload. After a planned reboot, run --check-loaded.\n'
