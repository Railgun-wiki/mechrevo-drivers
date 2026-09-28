# OpenMechrevo Linux Kernel Driver Adaptation (Phase P1)

This directory houses the minimal incremental patch series, automated patch generator, udev rules, and packaging definitions for **Mechrevo laptops (both AMD and Intel platforms: GM5HG0A, GM5IXxA, GM6IXxB, etc.)** and compatible Uniwill ODM platforms, based on upstream `tuxedo-drivers` release tag `v4.22.3`.

---

## Patch Series Architecture (0001–0016)

The adaptation strictly maintains separation between mechanism and policy, ensures zero regressions for non-target models, and avoids independent driver forks.

- **0001-quirks-add-DMI-entries-for-Mechrevo-family.patch**
  Adds MECHREVO vendor identifiers to the compatibility check allowlist, maps Mechrevo AMD and Intel models to their respective TDP limits in `tuxedo_io`, and enables auto-boot, powershare, and custom profile quirks in `uniwill_keyboard`.

- **0002-uniwill-extend-EC-abstraction-with-model-scoped-regi.patch**
  Introduces `struct uniwill_ec_regmap` to abstract EC register locations (perf mode 0x0751, status LED 0x07A5, TDP limits 0x0783–0x0785, thermal limit 0x0786, fan tables 0x0F00/0x0F30). Unmatched models default to `default_uniwill_regmap`, guaranteeing backward compatibility.

- **0003-uniwill-expose-perf-mode-power-led-via-sysfs.patch**
  Registers sysfs attributes `perf_mode` (`office`, `gaming`, `turbo`, `custom`) and `power_led_mode` (0–3 with atomic read-modify-write on 0x07A5). Integrates with the kernel `platform_profile` framework with compatibility for both legacy kernels and Linux 6.13+ (`platform_profile_ops`).

- **0004-uniwill-expose-AMD-TDP-limits-via-sysfs.patch**
  Exposes `tdp_spl`, `tdp_sppt`, `tdp_fppt`, and `tcc_offset` along with `*_min` and `*_max` boundaries. Enforces range clamping and post-write readback verification returning `-EIO` on mismatch.

- **0005-uniwill-expose-fan-curve-11-point-via-sysfs-binary-a.patch**
  Introduces binary attributes `fan_curve1` (CPU) and `fan_curve2` (GPU) for
  `{up_temp, down_temp, duty_percent}` triplets; patch 0011 subsequently expands
  the public ABI to the native 16-slot/48-byte layout.

- **0006-uniwill-expose-dGPU-power-cut-and-MUX-scheme-via-sys.patch**
  Implements `uniwill_wmi_oemg()` for ACPI `AMW0.OEMG` (subsystem `0x0300`). The final patch exposes status and restricted restoration via `dgpu_power`; live power cuts are refused until a safe eject handshake is proven. `mux_scheme` describes the separate reboot-time display MUX.

- **0007-Documentation-ABI-testing-sysfs-platform-mechrevo.patch**
  Provides standard Linux kernel ABI documentation for all newly introduced sysfs attributes.

- **0008–0011 safety and ABI corrections**
  Preserve `FanBoost` during mode changes, keep runtime dGPU power control read-only,
  support model globs, clamp final-slot reads, and expose the native 48-byte curve ABI.

- **0012-uniwill-support-GM6IXxB-three-fan-tables.patch**
  Selects `FanTable2p0` for GM6IXxB, exposes `fan_curve3`, and maps three
  logical 16-point curves onto the three 80-byte physical fan tables.

- **0013-fix-ite8297-nb05-remove-uninitialized-paths.patch**
  Rejects unknown ITE8297 LED class devices and removes the dead NB05 probe
  check that read an uninitialized return value.

- **0014-uniwill-validate-ec-writes-and-fan-tables.patch**
  Corrects Intel EC 0x0786 offset limits, checks multi-register EC writes and fan table updates, preserves FanTable2p0 GPU thresholds, and refuses unverified live dGPU power cuts. It does **not** implement safe runtime dGPU eject; that feature remains blocked pending a proven Linux/firmware handshake and hardware validation.

- **0015-uniwill-support-legacy-platform-profile-api.patch**
  Keeps compatibility with the pre-6.13 `platform_profile` handler API (which has no `CUSTOM` enum); the custom mode remains available through the dedicated `perf_mode` sysfs attribute.

- **0016-uniwill-break-wmi-keyboard-module-dependency-cycle.patch**
  Routes OEMG through the registered WMI interface instead of linking `tuxedo_keyboard` back to `uniwill_wmi`. An isolated `depmod` check covers both compiled modules; this fixes the dependency cycle found when installing the 4.22.3-1 package.

---

## Automated Quirk Header Generation

The generated quirk header comes from `hardware/archetypes/*.toml`. When model ranges change:

```bash
cargo run --manifest-path protocol/codegen-rs/Cargo.toml -- --check
```

The final release patch must include the updated header; generation alone does not change the packaged source.

---

## Standalone Smoke Testing (No DKMS Required)

The packages replay patches 0001–0016 against tuxedo-drivers v4.22.3. The full-source fork at [Railgun-wiki/mechrevo-drivers](https://github.com/Railgun-wiki/mechrevo-drivers/tree/mechrevo-v4.22.3) is pinned as `submodules/tuxedo-drivers` and must match that replay; its `main` branch tracks upstream. On Clang-built kernels, pass `LLVM=1` when invoking the kernel build system with `M=<absolute patched source path>`. Do not load modules or write EC/GPU sysfs controls as part of a build smoke test.

---

## Sysfs Attribute Verification Matrix

| Sysfs Node | Access | Description | Test Command |
| :--- | :--- | :--- | :--- |
| `perf_mode` | rw | Mode selector: `office`, `gaming`, `turbo`, `custom` | `cat perf_mode` / `echo turbo \| sudo tee perf_mode` |
| `power_led_mode` | rw | Status LED color: `0` (office), `1` (gaming), `2` (turbo) | `cat power_led_mode` / `echo 2 \| sudo tee power_led_mode` |
| `tdp_spl` | rw | Sustained Power Limit (SPL / PL1 in Watts) | `cat tdp_spl` / `echo 65 \| sudo tee tdp_spl` |
| `tdp_sppt` | rw | Slow PPT (sPPT / PL2 in Watts) | `cat tdp_sppt` / `echo 75 \| sudo tee tdp_sppt` |
| `tdp_fppt` | rw | Fast PPT (fPPT / PL4 in Watts) | `cat tdp_fppt` / `echo 85 \| sudo tee tdp_fppt` |
| `tcc_offset` | rw | AMD absolute target °C; Intel offset below TjMax (0–20); out-of-range rejected | `cat tcc_offset` |
| `fan_boost` | rw | Maximum fan speed override (`0` or `1`) | `cat fan_boost` / `echo 1 \| sudo tee fan_boost` |
| `fan_curve1` | rw (bin) | FanTable1p5 CPU or FanTable2p0 fan 1 (48 bytes) | `hexdump -C fan_curve1` |
| `fan_curve2` | rw (bin) | FanTable1p5 GPU or FanTable2p0 fan 2 (48 bytes) | `hexdump -C fan_curve2` |
| `fan_curve3` | rw (bin) | FanTable2p0 fan 3 (48 bytes; hidden on two-fan models) | `hexdump -C fan_curve3` |
| `dgpu_power` | rw (restricted) | Status and restore of already-cut rail; live cut returns `EOPNOTSUPP` | `cat dgpu_power` |

---

## [UNVERIFIED] Features and Safety Instructions

### Fan Curve Tables (`fan_curve1` / `fan_curve2` / `fan_curve3`)
- **Status**: `[UNVERIFIED]`
- **Mechanism**: Each ABI file contains 16 `{up_temp, down_temp, duty_percent}` points (48 bytes). FanTable1p5 uses two channels; GM6IXxB FanTable2p0 has three 80-byte physical fan tables. Only each table's CPU thresholds and duty block are changed; separate GPU thresholds are preserved. The EC table is disabled during write/readback and restored on error when hardware access permits; this is not a substitute for model-specific live validation.
- **Verification Protocol**:
  First, dump and back up current factory curves:
  ```bash
  dd if=/sys/devices/platform/tuxedo_keyboard/fan_curve1 of=cpu_curve_backup.bin bs=48 count=1
  hexdump -C cpu_curve_backup.bin
  ```
  Verify that temperatures ascend monotonically and duty percentages remain between 0 and 100 before writing any test buffer back.

### Discrete GPU Power Cut (`dgpu_power`)
- **Status**: Runtime power-off is blocked (`EOPNOTSUPP`). Restoring an already-cut rail is available where the capability bit is set, with status verification. The GPU hot-switch feature is **not delivered**.
- **Firmware handshake**: `IGPS(1)` notifies `PEGP` with eject request `0x03`, then waits for `PEGP._PSC == 3` before turning off `PG00`. The SSDT implements `_PS3`, but a complete Linux ACPI hotplug/DRM/multifunction-device handshake has not been established or tested. No-process checks or a plain PCI remove do not establish safety.
- **MUX**: `OemDisplayMode` is a separate NVRAM display-route choice that normally takes effect after reboot; it is not a safe runtime dGPU power-off protocol.

---

## Packaging

### Arch / AUR-style DKMS package

`kernel/packaging/arch/PKGBUILD` produces `mechrevo-drivers-dkms 4.22.3-2`. It downloads the unmodified tuxedo-drivers v4.22.3 source and applies all 16 patches in order; the full-source fork is kept in sync by `tests/test_kernel_release_sync.py`. The PKGBUILD conflicts with and provides `tuxedo-drivers-dkms`, so it **replaces** that package rather than installing a second copy of the same modules. It does not publish itself to the AUR.

The checkout uses relative patch symlinks; export a **self-contained** AUR source tree (real patch files plus `.SRCINFO`) before uploading it to an AUR git repository or building it outside OpenMechrevo:

```bash
./kernel/packaging/arch/export-aur.sh /tmp/mechrevo-drivers-aur
```

A guarded, manual local trial script is provided. **The corrected package has not been installed or loaded on this machine by this workflow.** `--preflight` only inspects; `--build` builds an archive without installing; `--install` requires an old-package rollback archive (matching the installed old package when present), builds the new archive, and asks for an explicit typed confirmation before running pacman. It also supports upgrading the already-installed 4.22.3-1 package, then checks DKMS and read-only `depmod` for installed kernels; stop if either fails. The current machine's rollback archive was located at `/home/yuki/Documents/Coding/L-Mechrevo/upstream/tuxedo-drivers-dkms/tuxedo-drivers-dkms-4.22.2-3-x86_64.pkg.tar.zst` (verify it is your intended recovery version when executing). For example:

```bash
bash kernel/packaging/arch/trial-dkms.sh --build
```

```bash
bash kernel/packaging/arch/trial-dkms.sh --install /home/yuki/Documents/Coding/L-Mechrevo/upstream/tuxedo-drivers-dkms/tuxedo-drivers-dkms-4.22.2-3-x86_64.pkg.tar.zst
```

Pacman/DKMS may build modules for installed kernel headers, but **loaded old modules remain in memory**. The script never unloads or reloads modules, reboots, or writes to EC/fan/dGPU/MUX controls. After a separate, planned reboot, `--check-loaded` performs read-only sysfs checks; a successful package build or install alone is not a live driver test. If recovery is needed, reinstall the saved old archive with `sudo pacman -U <path-to-old-package>` and reboot in a maintenance window. Do not attempt dGPU power-off: safe runtime hot switching is not implemented.

### Debian / Ubuntu
Build the Debian package using the automated build script:
```bash
./kernel/packaging/build-deb.sh
sudo dpkg -i kernel/packaging/build-deb/mechrevo-drivers-dkms_*.deb
```
