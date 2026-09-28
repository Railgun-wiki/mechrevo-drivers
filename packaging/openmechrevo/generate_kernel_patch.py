#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""OpenMechrevo Kernel Patch Generator & DKMS Submodule Synchronizer.

Features:
1. Generates `uniwill_mechrevo_quirks.h` from hardware TOML declarations.
2. Synchronizes the generated header into the DKMS/tuxedo-drivers submodule.
3. Automatically exports `000x-*.patch` series from commits in the submodule.
4. Updates symlinks in `kernel/packaging/arch/` for Arch PKGBUILD compatibility.
5. Generates a consolidated `patch.diff` for DKMS packaging.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

KERNEL_DIR = Path(__file__).resolve().parent
REPO_ROOT = KERNEL_DIR.parent
UPSTREAM_DIR = REPO_ROOT.parent / "upstream"
CODEGEN_KERNEL = REPO_ROOT / "protocol" / "codegen" / "codegen_kernel.py"
DEFAULT_PATCHES_DIR = KERNEL_DIR / "patches"
DEFAULT_ARCH_PACKAGING_DIR = KERNEL_DIR / "packaging" / "arch"
DEFAULT_BASE_REF = "v4.22.3"


def find_submodule_dir(preferred: Path | None = None) -> Path | None:
    """Locate the tuxedo-drivers submodule/upstream repository."""
    candidates = []
    if preferred:
        candidates.append(preferred)
    candidates.extend([
        REPO_ROOT / "submodules" / "tuxedo-drivers",
        UPSTREAM_DIR / "tuxedo-drivers",
        REPO_ROOT / "upstream" / "tuxedo-drivers",
    ])

    for candidate in candidates:
        if candidate.exists() and (candidate / "Makefile").exists() and (candidate / "src").exists():
            # Ensure .git connection if working in a git submodule
            git_file = candidate / ".git"
            git_module_dir = REPO_ROOT / ".git" / "modules" / "submodules" / "tuxedo-drivers"
            if not git_file.exists() and git_module_dir.exists():
                try:
                    rel_gitdir = os.path.relpath(git_module_dir, candidate)
                    git_file.write_text(f"gitdir: {rel_gitdir}\n", encoding="utf-8")
                except Exception:
                    pass
            return candidate
    return None


def run_cmd(cmd: list[str], cwd: Path | None = None) -> tuple[int, str, str]:
    """Execute command and return returncode, stdout, stderr."""
    res = subprocess.run(cmd, cwd=str(cwd) if cwd else None, capture_output=True, text=True)
    return res.returncode, res.stdout, res.stderr


def detect_base_commit(submodule_dir: Path, requested_base: str | None = None) -> str:
    """Determine base commit/tag in submodule."""
    if requested_base:
        code, out, _ = run_cmd(["git", "rev-parse", "--verify", f"{requested_base}^{{commit}}"], cwd=submodule_dir)
        if code == 0:
            return out.strip()

    # Try default tag
    for tag in [DEFAULT_BASE_REF, "v4.22.3_rc", "0b2f8c6"]:
        code, out, _ = run_cmd(["git", "rev-parse", "--verify", f"{tag}^{{commit}}"], cwd=submodule_dir)
        if code == 0:
            return out.strip()

    # Fallback to initial commit
    code, out, _ = run_cmd(["git", "rev-list", "--max-parents=0", "HEAD"], cwd=submodule_dir)
    if code == 0 and out.strip():
        return out.strip().splitlines()[0]

    raise RuntimeError(f"Unable to determine base commit in {submodule_dir}")


def export_patches(
    submodule_dir: Path,
    patches_dir: Path,
    base_ref: str,
    arch_packaging_dir: Path | None = None,
    sync_header: bool = True,
) -> bool:
    """Export git format-patch series from submodule and update packaging."""
    print(f"[*] Submodule directory: {submodule_dir}")
    print(f"[*] Base reference:      {base_ref}")
    print(f"[*] Output patches dir:   {patches_dir}")

    # 1. Optionally sync generated C quirks header into submodule tree
    if sync_header:
        header_in_submodule = submodule_dir / "src" / "uniwill_mechrevo_quirks.h"
        header_in_kernel = KERNEL_DIR / "uniwill_mechrevo_quirks.h"
        import importlib.util
        spec = importlib.util.spec_from_file_location("codegen_kernel", str(CODEGEN_KERNEL))
        if spec and spec.loader:
            cg = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cg)
            cg.run(output_path=header_in_kernel)
            if header_in_submodule.parent.exists():
                shutil.copy2(header_in_kernel, header_in_submodule)
                print(f"[+] Synchronized quirks header to {header_in_submodule}")

    # Check for commits ahead of base
    code, out, _ = run_cmd(["git", "rev-list", "--count", f"{base_ref}..HEAD"], cwd=submodule_dir)
    commit_count = int(out.strip()) if code == 0 and out.strip().isdigit() else 0

    code, status_out, _ = run_cmd(["git", "status", "--porcelain"], cwd=submodule_dir)
    is_dirty = bool(status_out.strip())

    if commit_count == 0 and not is_dirty:
        print("[!] No commits or working-tree changes ahead of base. Nothing to export.")
        return True

    print(f"[*] Found {commit_count} commit(s) ahead of base {base_ref[:8]} (Working tree dirty: {is_dirty})")

    patches_dir.mkdir(parents=True, exist_ok=True)

    # 2. Export format-patch series
    if commit_count > 0:
        for existing_patch in patches_dir.glob("0*.patch"):
            existing_patch.unlink()

        cmd = [
            "git", "format-patch",
            "-N",
            "--no-signature",
            f"{base_ref}..HEAD",
            "-o", str(patches_dir)
        ]
        code, out, err = run_cmd(cmd, cwd=submodule_dir)
        if code != 0:
            print(f"[-] git format-patch failed: {err}", file=sys.stderr)
            return False

        generated_patches = sorted(patches_dir.glob("0*.patch"))
        print(f"[+] Generated {len(generated_patches)} patch file(s):")
        for p in generated_patches:
            print(f"    - {p.name}")

        # 3. Synchronize symlinks in arch packaging dir if present
        if arch_packaging_dir and arch_packaging_dir.exists():
            for old_link in arch_packaging_dir.glob("0*.patch"):
                if old_link.is_symlink():
                    old_link.unlink()
            for p in generated_patches:
                link_target = arch_packaging_dir / p.name
                rel_path = os.path.relpath(p, arch_packaging_dir)
                link_target.symlink_to(rel_path)
            print(f"[+] Updated symlinks in {arch_packaging_dir}")

    # 4. Generate consolidated patch.diff
    unified_diff_path = patches_dir / "patch.diff"
    code, diff_out, _ = run_cmd(["git", "diff", f"{base_ref}"], cwd=submodule_dir)
    if code == 0 and diff_out.strip():
        unified_diff_path.write_text(diff_out, encoding="utf-8")
        print(f"[+] Wrote consolidated diff: {unified_diff_path} ({len(diff_out.splitlines())} lines)")

        # Sync to upstream dkms if directory exists
        upstream_dkms_diff = UPSTREAM_DIR / "mechrevo-drivers-dkms" / "patch.diff"
        if upstream_dkms_diff.parent.exists():
            shutil.copy2(unified_diff_path, upstream_dkms_diff)
            print(f"[+] Updated DKMS package diff: {upstream_dkms_diff}")

    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="OpenMechrevo Kernel Patch & Quirk Generator"
    )
    parser.add_argument(
        "--header-out",
        type=Path,
        default=None,
        help="Target C header output path (default: kernel/uniwill_mechrevo_quirks.h)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check for quirks header drift without writing",
    )
    parser.add_argument(
        "--export-patches",
        action="store_true",
        help="Extract format-patch series from tuxedo-drivers submodule into kernel/patches/",
    )
    parser.add_argument(
        "--submodule-dir",
        type=Path,
        default=None,
        help="Path to tuxedo-drivers git submodule (auto-detected if omitted)",
    )
    parser.add_argument(
        "--base",
        type=str,
        default=None,
        help=f"Base commit/tag for patch extraction (default: {DEFAULT_BASE_REF})",
    )
    parser.add_argument(
        "--patches-out",
        type=Path,
        default=DEFAULT_PATCHES_DIR,
        help="Destination directory for exported patches (default: kernel/patches)",
    )

    args = parser.parse_args()

    # Mode 1: Export patches from DKMS submodule
    if args.export_patches:
        submodule_path = find_submodule_dir(args.submodule_dir)
        if not submodule_path:
            print("[-] Error: tuxedo-drivers submodule directory not found.", file=sys.stderr)
            return 1

        try:
            base_commit = detect_base_commit(submodule_path, args.base)
        except Exception as exc:
            print(f"[-] Error detecting base commit: {exc}", file=sys.stderr)
            return 1

        success = export_patches(
            submodule_dir=submodule_path,
            patches_dir=args.patches_out,
            base_ref=base_commit,
            arch_packaging_dir=DEFAULT_ARCH_PACKAGING_DIR,
            sync_header=True,
        )
        return 0 if success else 1

    # Mode 2: Generate quirks C header via codegen_kernel
    import importlib.util
    spec = importlib.util.spec_from_file_location("codegen_kernel", str(CODEGEN_KERNEL))
    if not spec or not spec.loader:
        print(f"[-] Error loading {CODEGEN_KERNEL}", file=sys.stderr)
        return 1
    codegen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(codegen)

    target_header = args.header_out or (KERNEL_DIR / "uniwill_mechrevo_quirks.h")
    success = codegen.run(check_only=args.check, output_path=target_header)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
