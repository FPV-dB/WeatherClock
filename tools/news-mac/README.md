# News for every clock

Sends the latest headlines to the News face on every Weather Clock on your Wi-Fi, every 10 minutes.

It uses `news.py` from the radar helper, which fetches the headlines. This script only delivers them.

1. Copy `clocks.example.txt` to `clocks.txt` and list your clocks, one IP address or local name per line.
2. Run `bash install.sh` on the computer that keeps the helpers running.
3. Check `news-all.log`: each clock should answer `200`.

`clocks.txt` and the log stay on your computer and are not committed.
If the radar helper is not in `~/WeatherClockRadar`, set `NEWS_DIR` in the launchd job.
