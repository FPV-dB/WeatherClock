#!/bin/zsh
set -e
D=${0:A:h}
IP=$(cat "$D/clock-ip")
"$D/.venv/bin/python" "$D/markets.py" --output "$D/markets.wcr"
curl --noproxy '*' -fsS -m 60 -H 'X-SmallTV: 1' \
  -F "file=@$D/markets.wcr;filename=markets.wcr;type=application/octet-stream" \
  "http://$IP/api/upload"
echo " $(date) ($IP)"
