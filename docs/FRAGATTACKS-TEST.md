# Testing the clock against FragAttacks

FragAttacks (2021) are flaws in the way Wi-Fi assembles fragmented and aggregated packets. On the ESP8266 they
live in Espressif's closed libraries (`libnet80211`, `libpp`): Espressif's advisory
([AR2023-008](https://www.espressif.com/sites/default/files/advisory_downloads/AR2023-008%20Security%20Advisory%20for%20WLAN%20FragAttacks%20EN.pdf),
V1.1 of 10/05/2024) has a fix for the ESP8266 RTOS SDK, but not for the NONOS SDK the Arduino core
uses. So the firmware can't fix the flaw itself; it limits what an injected packet can do
(see "What the firmware already does" at the end).

This walkthrough uses the public tool from the researcher who found the flaws,
[vanhoefm/fragattacks](https://github.com/vanhoefm/fragattacks), to measure which ones this clock has.
**Only use it on your own devices and networks.**

## What you need

- A PC with **Ubuntu 20.04** (kernel up to 5.12; the author tested with 5.8). A "live" USB stick works.
- A compatible USB Wi-Fi card. The author recommends the **Alfa AWUS036ACM**. Others tested:
  Intel Wireless-AC 8265/3160, Netgear WN111v2, TP-Link TL-WN722N **v1.x**, Alfa AWUS036NHA. The Intel AX200
  does **not** work. With two cards you can use "injection" mode, which is more reliable.
- The clock near the PC.

## 1. Install the tool (once)

```bash
sudo apt-get update
sudo apt-get install libnl-3-dev libnl-genl-3-dev libnl-route-3-dev libssl-dev libdbus-1-dev git pkg-config build-essential macchanger net-tools python3-venv aircrack-ng rfkill firmware-ath9k-htc
git clone https://github.com/vanhoefm/fragattacks.git fragattacks
cd fragattacks/research
./build.sh
./pysetup.sh
```

Drivers with the tool's patches (restart the PC afterwards):

```bash
sudo apt-get install bison flex linux-headers-$(uname -r)
git clone https://github.com/vanhoefm/fragattacks-drivers58.git fragattacks-drivers58
cd fragattacks-drivers58
make defconfig-wifi
make -j 4
sudo make install
```

Cards with `ath9k_htc` (TP-Link TL-WN722N v1, Alfa AWUS036NHA) also need the firmware:
`cd research/ath9k-firmware/ && ./install.sh`.

## 2. Before each session

1. Turn off Ubuntu's Wi-Fi in the network manager (the tool needs the card to itself).
2. Open a terminal:
   ```bash
   cd fragattacks/research
   sudo su
   source venv/bin/activate
   ./droptraffic.sh
   ```
3. Find the card's name with `iw dev` (below, `wlan0`; the second card, if any, `wlan1`).

## 3. Put the clock on the test network

The tool creates a network called **`testnetwork`**, password **`abcdefgh`**, with DHCP. The clock has to
join it:

1. Make a **backup** in the panel (Settings → System → Make backup).
2. In the panel, **Reset Wi-Fi**. The clock opens the `WeatherClock-Setup` network (the password shows on screen).
3. Run the first test below (`testnetwork` only exists while the tool runs) and, while it's up,
   connect a phone to `WeatherClock-Setup`, choose `testnetwork` and type `abcdefgh`.
4. At the end, do the same with your home network.

The clock answers ping, which is what the tool uses to tell whether a forged packet was accepted.

## 4. Check the equipment (required before trusting the results)

```bash
./fragattack.py wlan0 --ap ping
./fragattack.py wlan0 --ap ping I,E,E
```

- `ping` failed: the clock isn't connected or the card isn't suitable. Stop here.
- `ping I,E,E` failed: repeat all the tests with `--icmp-size 100` at the end.

Card injection test: `./fragattack.py wlan0 ping --inject-test wlan1 --ap` (with a second card)
or, in injection mode, `./test-injection.py wlan0 wlan1`.

## 5. Tests

Run each line (replace `COMMAND` with the one from the table):

```bash
./fragattack.py wlan0 --ap COMMAND
```

With two cards: `./fragattack.py wlan0 --inject wlan1 --ap COMMAND`.

| Command | Flaw | Result |
|---|---|---|
| `ping I,E --amsdu` | (only shows whether A-MSDU is accepted) | |
| `amsdu-inject` | CVE-2020-24588 | |
| `amsdu-inject-bad` | CVE-2020-24588 | |
| `ping I,F,BE,AE` | CVE-2020-24587 | |
| `ping I,E,R,AE` | CVE-2020-24586 | |
| `ping I,E,R,E` | CVE-2020-24586 | |
| `ping I,E,E --inc-pn 2` | CVE-2020-26146 | |
| `ping I,E,P` | CVE-2020-26147 | |
| `ping I,P,E` | CVE-2020-26147 | |
| `ping I,P` | CVE-2020-26140 | |
| `ping I,P,P` | CVE-2020-26143 | |
| `eapol-amsdu I,P` | CVE-2020-26144 | |
| `eapol-amsdu-bad I,P` | CVE-2020-26144 | |
| `ping I,D,P --bcast-ra` | CVE-2020-26145 (manual check) | |

How to read it:

- `TEST COMPLETED SUCCESSFULLY` → **vulnerable** to that flaw.
- `Test timed out! Retry to be sure, or manually check result` → not accepted; repeat once to confirm.
- The `--bcast-ra` tests give no automatic answer: they would need a capture on the device itself,
  which the clock can't do. Skip them or note "not measured".

## What the firmware already does against an injected packet

| An injected packet would try to | Defence |
|---|---|
| Change the time (SNTP accepts any reply) and wipe the history | the time only counts once it matches Open-Meteo's, which arrives over TCP; an unconfirmed jump is undone; a date in the past never erases anything |
| Send commands to the panel | the panel password (turn it on in Settings → System) |
| Write another firmware | the panel password, and updates must be signed (see SIGNATURES.md) |
| Lock the clock up with wrong passwords | a 1 s wait per IP, without freezing the screen |
| Use the clock to multiply traffic (SSDP) | at most 4 answers per second |
| Fake the weather or a rain warning | values are bounded: it shows the wrong thing, but doesn't crash |

On the network side: update the router's firmware (several variants depend on the router also being vulnerable) and
keep the clock on a guest or IoT network, isolated from your computers.
