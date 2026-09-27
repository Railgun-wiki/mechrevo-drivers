# Mechrevo driver source

This branch contains the complete tuxedo-drivers v4.22.3 source with OpenMechrevo patches 0001–0014 applied. The upstream `main` branch of this fork is left available to track tuxedocomputers/tuxedo-drivers.

The matching patch series, generated quirk source and packaging files live in OpenMechrevo's `kernel/` directory. The `submodules/tuxedo-drivers` gitlink pins this branch to the exact full-source commit; changes to kernel source should be exported to the patch series and checked against a clean upstream v4.22.3 replay before updating the gitlink.

The `Test` workflow validates the fan-curve input rules and builds all modules against Ubuntu kernel headers. The `Release` workflow runs those checks again and publishes a source-only archive plus SHA-256 checksum when a `mechrevo-v*` tag is pushed; no release tag has been created here. DKMS packaging and distro installers are maintained separately by OpenMechrevo.

**Safety:** The dGPU runtime power-off operation remains disabled (`EOPNOTSUPP`). Restoring an already-off rail is a distinct operation. Neither a successful build nor this fork establishes that Linux ACPI eject, DRM, PCI multifunction removal and firmware `_PSC` state transitions are safe on actual hardware. No automatic module loading or hardware write is part of the workflows.
