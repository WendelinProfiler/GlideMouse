# GlideMouse blueutil backend

Based on blueutil 2.14.0 by Ivan Kuchin and Frederik Seiffert:
https://github.com/toy/blueutil

The upstream MIT license is preserved in LICENSE.txt.

Local changes:
- Pairing success requires the pairing delegate's actual Bluetooth completion status to be successful, as well as a retained device record.
- Log the completion status without logging the confirmation number.
- For an explicit pairing request, GLIDEMOUSE_APPROVE_TARGET permits confirmation only when the configured 12-digit address exactly matches the requesting device. Other targets retain upstream behavior.

Build on macOS:
`xcrun clang -Wall -Wextra -Werror -mmacosx-version-min=10.9 -framework Foundation -framework IOBluetooth blueutil.m -o blueutil`

Install to a new file, sign it, verify that --version runs, and atomically rename it into the bundle. Do not overwrite an executable inode while it is mapped by a running process. Re-sign and verify the containing bundle after replacement.

Further local changes:
- Complete stale-bond removal with the same forceRemove operation already used by the native menu helper; require both pairing and connection to be gone before returning success, then restore the device's native HID eligibility.
- Include the IOReturn value in connect-failure metadata, without printing authentication values.

Version 2.14.0-glidemouse.4 also bounds pairing to eight seconds and preserves the authenticated connection on successful pairing: `IOBluetoothDevicePair.stop()` is reserved for failure, because it disconnects the device. A successful SDP query must confirm the HID service before pairing is reported ready. Install backend updates atomically into the private `backend` directory when the menu helper is configured to use it; no signed app-bundle change is needed for that layout.