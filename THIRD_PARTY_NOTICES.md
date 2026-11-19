# Third-party components

## USBPcap

The documented USBPcap interface header in `third_party/USBPcap.h` retains its
original copyright and BSD-2-Clause SPDX identifier. See
`third_party/USBPcap-HEADER-LICENSE.txt`.

The optional release package includes the **unmodified** Microsoft-attested
USBPcap 1.5.4.0 x64 driver, copyright Tomasz Mon and USBPcap contributors,
licensed under GPL version 2. Its original corresponding-source/PDB release
archive is included under `corresponding-source/`. It is not our own signed
driver. No claim of Microsoft certification of GlideMouse is made.

- [Upstream release and corresponding source](https://github.com/desowin/usbpcap/releases/tag/1.5.4.0)
- [Upstream driver licensing](https://github.com/desowin/usbpcap/blob/1.5.4.0/README)

## Apple

The Apple Wireless Mouse driver is proprietary and **not distributed** in this
repository or our release ZIP. The installer can reuse a matching original
installation or accept an original file supplied by the user. Its SHA-256 and
Authenticode signature are checked before any binding is added.

Magic Mouse is an Apple trademark. GlideMouse is an independent community
project and is not affiliated with Apple. The mouse-outline notification icon
is drawn by the application. At the project owner's request, the widget also
uses the existing locally rendered 48-frame black-mouse product animation.
Its source and provenance are recorded in `assets/MODEL-PROVENANCE.md` (copied
as `MODEL-PROVENANCE.md` in the release package). That third-party visualization
and its marks are not covered by the MIT license for our code; no claim of an
official Apple USB-C model or of ownership of the original model is made.

## Windhawk

Windhawk is optional. This repository does not bundle or replace the Mac Island
& Dock mod (GPL-3.0). It can signal an existing compatible native mouse panel,
and otherwise opens its standalone Windows tray popup. No Windhawk installation
is necessary for the Windows tray experience.
