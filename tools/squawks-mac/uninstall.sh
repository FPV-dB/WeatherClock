#!/bin/sh
# Stops and removes the Bad Squawks feed jobs (the ~/WeatherClockSquawks folder is kept).
for job in squawks squawks-push; do
  launchctl bootout "gui/$(id -u)/com.weatherclock.$job" 2>/dev/null || true
  rm -f "$HOME/Library/LaunchAgents/com.weatherclock.$job.plist"
done
echo "Bad Squawks feed stopped."
