# AI Usage updater for macOS

Feeds the **AI Usage** face (Weather Clock 2.0.17+): what is left of the Claude and OpenAI Codex 5-hour and
7-day limits, and when each resets. Runs on the MacBook Pro every 5 minutes.

## Needs

- **Claude**: Claude Code installed on this Mac and signed in once with your Claude account (`claude`, then `/login`).
  The first run may show a Keychain prompt for `security`; choose **Always Allow**.
- **Codex**: the Codex app or CLI signed in on this Mac with your ChatGPT account (`codex login`).
  Without a working sign-in, the face uses the limits Codex last wrote to its own logs on this Mac.

Both usage endpoints are the ones the Claude Code and Codex apps use for their own usage screens. They are not
published APIs and may change. Sign-ins are only read, never changed.

## Set up

```sh
/usr/bin/python3 ai_usage.py      # test: prints the numbers, sends nothing
cp clocks.example.txt clocks.txt  # one clock IP per line
sh install.sh                     # every 5 minutes; log in ai-usage.log
```

`sh uninstall.sh` removes the job. If the clocks hear nothing for 45 minutes, they skip the face in rotation.
