#!/bin/zsh
set -e
D=${0:A:h}
IP=${1:-$(cat "$D/clock-ip" 2>/dev/null)}
if [ -z "$IP" ]; then
  printf 'Weather Clock IP address: '
  read IP
fi
[[ "$IP" == <->.<->.<->.<-> ]] || { echo 'Enter an IPv4 address, for example 192.168.1.178'; exit 2; }
echo "$IP" > "$D/clock-ip"
python3 -m venv "$D/.venv"
"$D/.venv/bin/python" -m pip install --disable-pip-version-check Pillow
chmod +x "$D/update.sh"
LABEL=com.weatherclock.markets
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
mkdir -p "$HOME/Library/LaunchAgents"
cat > "$PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key><array><string>$D/update.sh</string></array>
  <key>StartInterval</key><integer>900</integer>
  <key>RunAtLoad</key><true/>
  <key>StandardOutPath</key><string>$D/markets.log</string>
  <key>StandardErrorPath</key><string>$D/markets.log</string>
</dict></plist>
PLIST
plutil -lint "$PLIST" >/dev/null
launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
launchctl bootstrap "gui/$(id -u)" "$PLIST"
echo "Markets card installed for $IP; it refreshes every 15 minutes."
