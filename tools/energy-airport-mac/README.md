# Energy War and Airport Traffic updater for macOS

This companion sends data to the built-in **Energy War** and **Airport Traffic** faces introduced in Weather Clock 2.0.15.

It refuses to write unless the configured clock MAC address matches and the clock runs firmware 2.0.15 or newer. Energy War records come from the configured GitHub repository through the authenticated `gh` CLI; if GitHub is temporarily unavailable, the last downloaded source is retained as a cache. Airport Traffic reuses the observation state produced by the existing Weather Clock flight updater.

## Set up

1. Install and authenticate the GitHub CLI (`gh auth login`) with access to the EnergyWar source repository.
2. Copy `config.example.json` to `config.json` and confirm the clock IP, MAC address, flight-state path and source repository.
3. Test without writing to the clock:

   ```sh
   /usr/bin/python3 update.py --dry-run
   ```

4. Run `update.py` every 15 minutes with launchd. The same macOS user must run the job so it can use that user's `gh` authentication.

The script POSTs only to `/api/energy` and `/api/flightstats`; it does not replace settings, rotation, history or uploaded cards.
