#!/bin/sh
set -eu
# Run on an Apple-silicon Mac. Runtime is a fresh self-contained osx-arm64
# publish; never pass an installed app or a user's Application Support folder.
runtime=${1:?"Usage: Build-MacAddon.sh PUBLISHED_RUNTIME OUTPUT_DIRECTORY"}
output=${2:?"Missing output directory"}
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$output"
app="$output/GlideMouse Switch.app"
if [ -e "$app" ]; then echo "App output already exists" >&2; exit 1; fi
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources/runtime"
cp -R "$runtime/." "$app/Contents/Resources/runtime/"
xcrun swiftc -O -target arm64-apple-macosx15.0 -module-name GlideMouseSwitchMenu -debug-prefix-map "$source_dir=/_/source" -framework AppKit -framework IOBluetooth -framework Carbon -framework CoreGraphics "$source_dir/MacMenu.swift" -o "$app/Contents/MacOS/GlideMouseSwitchMenu"
xcrun clang -O2 -arch arm64 -mmacosx-version-min=15.0 -Wall -Wextra -Werror -framework Foundation -framework IOBluetooth "$source_dir/vendor/blueutil/blueutil.m" -o "$app/Contents/Resources/blueutil"
cp "$source_dir/vendor/blueutil/LICENSE.txt" "$app/Contents/Resources/blueutil-LICENSE.txt"
cp "$source_dir/blueutil-refresh.sh" "$app/Contents/Resources/blueutil-refresh.sh"
cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>org.glidemouse.switch</string>
<key>CFBundleName</key><string>GlideMouse Switch</string>
<key>CFBundleExecutable</key><string>GlideMouseSwitchMenu</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.2.0</string>
<key>CFBundleVersion</key><string>2001</string>
<key>LSMinimumSystemVersion</key><string>15.0</string>
<key>LSUIElement</key><true/>
<key>NSBluetoothAlwaysUsageDescription</key><string>Connect and release only your configured Magic Mouse.</string>
<key>NSBluetoothPeripheralUsageDescription</key><string>Connect and release only your configured Magic Mouse.</string>
<key>NSLocalNetworkUsageDescription</key><string>Coordinate Magic Mouse ownership with your trusted Windows computer.</string>
</dict></plist>
PLIST
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
cp "$source_dir/Install-Mac.command" "$output/Install-Mac.command"
chmod 755 "$output/Install-Mac.command"
echo "Built optional Apple-silicon Mac add-on. Ad-hoc signed; not notarized."
