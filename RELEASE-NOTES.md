# GlideMouse 0.1.1 experimental alpha

This patch addresses missing momentum and gestures when GlideMouse shares a Bluetooth adapter with Podglow. A connected pointer is no longer treated as proof that the full mouse driver chain is ready.

- The connection broker checks the active Apple filter, preserves an intentional disconnect and supports reconnecting with one click.
- While Podglow owns Bluetooth, filter repair ignores its temporary native-radio mouse twin. The virtual-radio mouse remains the input owner.
- Setup resolves Podglow's virtual adapter to the matching physical USB adapter instead of attaching the capture filter to the emulated hub. A mismatched adapter is rejected.
- Setup records any change to a hidden native mouse filter so uninstall can restore it. Other filters are preserved.
- Connection-intent and installer-topology regression checks are included.

Use this with **Podglow 0.1.6 alpha or newer**. That patch initializes Intel firmware before takeover and restores the scoped USBPcap child filter after changing the radio driver.

## Install or update

Download and extract the entire ZIP, then run `GlideMouseSetup.exe`. Keep the original supported Apple driver package available; it is not included. Save your work and restart when setup is complete.

The alpha installer still refuses to overwrite an existing installation. To update an earlier public alpha, uninstall it first and follow its restart instructions, then install the new package. A manual prototype without an installation journal needs manual migration; do not delete its driver files.

If you use Podglow, update it first, let Bluetooth reconnect, then install GlideMouse. Windhawk and an Island are optional: the normal Windows notification-area widget remains available.

## What was tested

The runtime fixes were checked on one Windows 11 x64 Intel Bluetooth development system after a full restart. The active stack included both USBPcap and the Apple mouse filter, and real touch reports and momentum events arrived. The user confirmed the mouse features and AirPods worked together.

The newly packaged installer and removal path have not been tested on a clean second PC. Other adapters and sleep/resume combinations remain unverified. This is an experimental, unsigned application package using unmodified signed upstream drivers; it is not a newly Microsoft-certified mouse driver.

See [third-party credits](THIRD_PARTY_NOTICES.md) for USBPcap, Apple driver requirements and artwork provenance.
