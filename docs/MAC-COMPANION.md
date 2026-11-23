# Optional Mac companion: GlideMouse Switch 0.2.0 alpha

The normal GlideMouse Windows package works without a Mac or this add-on.
Host switching additionally requires the matching Windows Switch add-on and
Podglow 0.1.8 alpha or newer on the same USB Bluetooth adapter.

## Downloads

- GlideMouse-0.2.0-alpha.1-win-x64.zip: normal Windows mouse setup.
- GlideMouse-Switch-0.2.0-alpha.1-win-x64.zip: optional Windows coordinator.
- GlideMouse-Switch-0.2.0-alpha.1-macos-arm64.zip: optional Apple-silicon Mac app.

The Mac app is ad-hoc signed, not notarized. The deployment target is macOS 15;
runtime hardware testing used macOS Tahoe. Intel Macs and older macOS versions
are not verified. Use macOS's normal Privacy & Security / Open Anyway flow for
an unsigned community application; the add-on does not disable Gatekeeper.

## One-time setup

1. Install the matching Podglow and GlideMouse releases. Existing public alpha
   installers still require their documented removal/migration procedure.
2. Extract the entire Windows Switch ZIP and run Setup-Windows.cmd as your normal
   Windows user. Leave it open. It displays your selected mouse address, local
   IPv4 addresses and public certificate fingerprint.
3. Extract the entire Mac ZIP and run Install-Mac.command. It installs the app
   under your user Applications folder and registers its login agent. In the app,
   enter the mouse address, Windows IP/hostname and public fingerprint from step 2.
   Allow Bluetooth and Local Network access when macOS requests them.
4. The Mac app copies its public fingerprint to the clipboard. Enter the Mac
   IP/hostname and this fingerprint in the Windows setup window. The Windows
   administrator prompt enables only the existing selected mouse and adds an
   app-scoped inbound firewall rule on Private networks, limited to the local subnet.
5. Ctrl + Alt + M on Windows and Control + Option + M on Mac take the mouse locally.
   The Mac menu shows the same shortcut with the system Magic Mouse glyph.

Both computers need your trusted local network. Do not expose TCP 58426 through
a router. A public network should not be marked trusted just to enable this app.
No SSH, account password or shared Bluetooth link key is used by setup or handoff.
Only public fingerprints are exchanged; each host creates and keeps its own identity.

## Behavior and limits

The target requests a device-scoped release from the trusted source, then opens
its own Bluetooth link. When the source helper is repeatedly unreachable, it can
attempt automatic takeover without a shortcut. Normal shutdown releases the mouse.
An unreachable helper does not prove the computer is off: an isolated source can
still hold the mouse. Automatic takeover is a bounded attempt, not a guarantee.

The app runs at login. It does not require an SSH session. With the direct Windows
path, Podglow reads real input report 0x90 for the mouse battery and publishes only
validated readings. Cached values are not relabelled as current measurements.
Windhawk and the full Island mod are not bundled in the mouse release.

On one Windows 11 x64 Intel USB / Apple-silicon Mac pair, runtime testing confirmed
pointer, clicks, momentum, touch-to-stop, desktop gestures, bidirectional handoff,
real Mac shutdown followed by Windows takeover, and Mac takeover with the Windows
helper stopped and SSH closed. A real 99% charge report reached the Island donut.
Actual Windows shutdown was not retested. A universally successful twenty-handoff
run, abrupt power loss, other hardware, all sleep states and clean installation of
the new distribution installer remain unverified. This is an experimental add-on.

## Updates and removal

The Mac installer preserves local settings/identity and backs up an existing app.
A changed native development signature can require renewed macOS privacy permission.
Managed-only updates can use private agent/backend directories without re-signing
an unchanged native menu app. Certificates are local and are not part of a download.

Use Quit GlideMouse Switch to stop the Mac helper. To remove its login item, run
`launchctl bootout gui/$(id -u)/org.glidemouse.switch`, remove
`~/Library/LaunchAgents/org.glidemouse.switch.plist`, then remove the app. Preserve
`~/Library/Application Support/GlideMouseSwitch` to retain trust. Never share identity.pfx.

Windows registers the current-user GlideMouseSwitch Run entry and enables
HostSwitchEnabled / DirectPointer in the selected mouse profile. The opt-in is
under HKLM\SOFTWARE\GlideMouse\Handoff; previous values are saved locally under
%ProgramData%\GlideMouse. Stop the add-on before changing those values. Reverting
the direct path also requires re-enabling the native mouse profile; removing the
add-on is not the same as uninstalling the GlideMouse driver. Do not delete driver
files or pairing records manually. Full add-on removal on a clean system is untested.
