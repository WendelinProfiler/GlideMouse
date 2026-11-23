#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
app="$HOME/Applications/GlideMouse Switch.app"
folder="$HOME/Library/Application Support/GlideMouseSwitch"
plist="$HOME/Library/LaunchAgents/org.glidemouse.switch.plist"
if [ ! -d "$source_dir/GlideMouse Switch.app" ]; then echo "Extract the entire add-on ZIP first."; exit 1; fi
codesign --verify --deep --strict "$source_dir/GlideMouse Switch.app"
mkdir -p "$HOME/Applications" "$folder" "$HOME/Library/LaunchAgents"
chmod 700 "$folder"
launchctl bootout "gui/$(id -u)/org.glidemouse.switch" 2>/dev/null || true
if [ -d "$app" ]; then mv "$app" "$folder/app-before-update-$(date +%Y%m%d-%H%M%S).app"; fi
ditto "$source_dir/GlideMouse Switch.app" "$app"
mkdir -p "$folder/agent" "$folder/backend"
ditto "$app/Contents/Resources/runtime" "$folder/agent"
cp "$app/Contents/Resources/blueutil" "$folder/backend/blueutil.next"
chmod 755 "$folder/backend/blueutil.next"
mv -f "$folder/backend/blueutil.next" "$folder/backend/blueutil"
cp "$app/Contents/Resources/blueutil-refresh.sh" "$folder/blueutil-refresh.sh"
chmod 700 "$folder/blueutil-refresh.sh" "$folder/agent" "$folder/backend"
if [ -f "$plist" ]; then mv "$plist" "$folder/launch-agent-before-update-$(date +%Y%m%d-%H%M%S).plist"; fi
plutil -create xml1 "$plist"
plutil -insert Label -string org.glidemouse.switch "$plist"
plutil -insert ProgramArguments -json '[]' "$plist"
plutil -insert ProgramArguments.0 -string "$app/Contents/MacOS/GlideMouseSwitchMenu" "$plist"
plutil -insert RunAtLoad -bool YES "$plist"
plutil -insert KeepAlive -json '{"SuccessfulExit":false}' "$plist"
plutil -insert LimitLoadToSessionType -string Aqua "$plist"
plutil -insert ProcessType -string Interactive "$plist"
plutil -insert ThrottleInterval -integer 15 "$plist"
plutil -insert ExitTimeOut -integer 6 "$plist"
plutil -insert StandardOutPath -string "$folder/launch-agent.stdout.log" "$plist"
plutil -insert StandardErrorPath -string "$folder/launch-agent.stderr.log" "$plist"
launchctl bootstrap "gui/$(id -u)" "$plist"
echo "Installed. Complete setup in the GlideMouse Switch window and allow Bluetooth / Local Network access."
