# GlideMouse Host Switch — optional experimental add-on

This prototype coordinates a configured USB-C Magic Mouse between Windows and a Mac. It can renew a stale pairing automatically. Both computers use their own Bluetooth connection; the Lenovo does not forward input and can be off while the Mac is in use.

This is separate from the released GlideMouse installer. Do not treat it as a tested download until a complete Mac → Windows → Mac hardware round trip has passed. A Windows driver restart request is an error, not a successful switch.

## How it works

- A normal per-user agent runs on each computer. A menu-bar or tray action requests the mouse locally.
- If the mouse is already connected locally with its required driver, the action leaves it alone.
- Otherwise the target asks the other agent to release only the configured mouse. The source holds its automatic reconnects. Windows preserves its pairing record and device registration; the Mac can renew its local pairing when needed.
- Windows can use the experimental direct HID service to own the mouse's control and input channels, avoiding a dependency on reloading the legacy Apple filter. Authentication keys remain managed by Windows. A short, device-scoped renewal can request a fresh encrypted session without removing the Windows device.
- Readiness requires an encrypted link, validated full touch reports and a live GlideMouse input processor. A working pointer alone does not count. The coordinator then checks that the connection remains ready.
- If the source computer is off or unreachable, the target can still try taking the mouse. A mouse still held by an unreachable, powered-on host may need to be released there or switched off and on.
- Pairing or permission dialogs imposed by the OS are never dismissed automatically. There is no claim that switching is instantaneous or always possible without waking the mouse.

The agents use mutually authenticated TLS, with the other computer's certificate fingerprint pinned during setup. Only fixed release, status, nonce-bound rollback and source-shutdown coordination requests are exposed over the network: no address, executable, arbitrary shell command, driver operation or pairing command is accepted remotely. Private identity files stay on their own computer. Local commands use a pipe restricted to the current user. No Bluetooth link keys are read or copied.

Switching has no SSH dependency: no SSH session, SSH server, remote shell or password prompt is required during normal use. Both local agents must be running and reachable over the local network for coordinated release. SSH used during development or manual installation is separate from the switching protocol.

## Test status

The direct Windows path has passed a physical check of movement, left/right clicks, scrolling, momentum, touch-to-stop and desktop swipes without a Windows restart. A Mac/Windows round trip with both Bluetooth adapters on and source-helper-off takeover have passed on one pair. Clean installation of the optional add-on remains unverified. See [distribution setup and limits](../../docs/MAC-COMPANION.md).

Some Magic Mouse firmware responds with `INVALID_REPORT_ID` even when the touch-mode command takes effect. The implementation keeps the channels open, retries once after 500 ms, and waits for actual full reports rather than treating that response as proof of either success or failure.

## Development

Publish `HostSwitch.csproj` with .NET 10 for `win-x64` and `osx-arm64`. On Windows, compile `WindowsBackend.cpp` with MSVC and the Windows SDK, linking `windowsapp.lib`, `bthprops.lib` and `advapi32.lib`. The backend reads the mouse already selected in GlideMouse and only talks to its existing driver broker.

On macOS, build the native menu app from `MacMenu.swift`. Its bundle includes the published runtime under `Contents/Resources/runtime` and a locally built blueutil. Add Bluetooth usage descriptions and `NSLocalNetworkUsageDescription` to Info.plist before signing the bundle. Grant Bluetooth and Local Network access when macOS requests them. Local Network access allows the menu app to coordinate the handoff directly with the other computer; SSH is only an installation and diagnostic tool. A verified peer refusal or connection timeout permits a local claim when the source is unavailable. Certificate failures and explicit release failures still stop the handoff.

Mac claims retain a newly created bond across connection retries and reuse a link already opened by pairing. A thirty-second observer can reconnect that current bond after a late drop. An explicit handoff cancels the observer before changing ownership; the observer never requests a peer release. Status queries have a five-second deadline, pairing twenty seconds, and connection operations twenty-five seconds. Windows input readiness still requires real touch reports and is distinct from successful Bluetooth connection.

Run `GlideMouseSwitch init ADDRESS ABSOLUTE_BACKEND_PATH` once on each host. Exchange the printed **public certificate fingerprints**, then run `GlideMouseSwitch trust PEER_HOST PEER_FINGERPRINT` on each. Run `serve` before using `take`, `release` or `status`. Never distribute `identity.pfx` or personal settings with a release. The network port is TCP 58426; any firewall permission should be limited to the private network and the peer where possible.

## Credits

[blueutil](https://github.com/toy/blueutil), by its original authors and contributors, supplies the macOS Bluetooth operations. The vendored development fork is based on blueutil 2.14.0 and retains its MIT license; local changes are documented under `vendor/blueutil`. No blueutil source or binary is silently copied into the public installer.

The release/renew-pairing approach was researched against [MagicSwitch](https://github.com/Fouadktb/MagicSwitch) and [Magic Switch](https://github.com/MegaManSec/magic-switch). This coordinator is a separate implementation; those applications are not bundled. GlideMouse's existing upstream driver and USBPcap credits continue to apply.

Protocol research also used the Linux kernel's [hid-magicmouse](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-magicmouse.c) and [HIDP implementation](https://github.com/torvalds/linux/blob/master/net/bluetooth/hidp/core.c), plus [Magic Mouse Windows protocol notes](https://github.com/thinhnh113/magic-mouse-driver-windows/blob/main/docs/protocol-notes.md). These are research references; their implementation code is not copied into this coordinator.

## Automatic takeover and shutdown checks

The target watches a previously assigned peer and can claim the mouse after repeated peer-unavailable observations, even without recent keyboard activity. Startup also provides a bounded initial claim window. A failed attempt has a cooldown measured from its completion. Loss of network alone cannot release a mouse held by a still-running source.

The Mac menu helper handles launchd SIGTERM directly as well as application termination. It stops its managed observer and performs device-scoped native removal, with a bounded shutdown deadline; it does not launch a new Bluetooth command during logout. Windows accepts WM_QUERYENDSESSION immediately and performs bounded cleanup on WM_ENDSESSION.

The Mac managed agent and Bluetooth backend can live under `~/Library/Application Support/GlideMouseSwitch/agent` and `backend`, separately from the signed menu bundle. The agent directory must contain the complete published runtime. This avoids changing the native app signature for managed-only updates. Native menu changes still require signing and may require renewed macOS permission.

Validation distinguishes a Bluetooth connection, a retained bond waiting for input, and real touch readiness. The no-SSH Mac connection has passed a physical check. Actual shutdown and repeated hardware handoffs remain separate checks; passing the flow tests does not establish those results.