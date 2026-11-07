# Signed updates (since 1.20.1)

Over the network, the clock only switches firmware when the uploaded file comes with a valid signature from the project key. Without that, anyone or any web page on the same network that got past the panel password (or reached a clock with no password) could install any firmware.

## How it works

- **Algorithm:** RSA-2048, PKCS#1 v1.5, over the SHA-256 of the file **exactly as uploaded** (`.bin` or `.bin.gz`).
- **The signature never goes inside the firmware.** It is a separate file (`<file>.sig`, 256 bytes) and reaches the clock in base64 in the `X-Firmware-Sig` HTTP header. The panel reads the signature from `latest.json`/`beta.json` (`sig` and `gzsig`). On the `/update` page, you pick the `.sig` together with the firmware.
- **Order on the clock:**
  1. During the upload, each block goes into the SHA-256 and into the update area.
  2. At the end, the signature is checked **before** `Update.end(true)`.
  3. Without a valid signature nothing is scheduled: the eboot command is not written and the current firmware carries on.
- **In the firmware:** only BearSSL's `br_rsa_i15_pkcs1_vrfy` maths, with the key as bytes (`ota_key.h`). That costs ~6.5 KB, against ~37 KB for the core's `PublicKey`/`SigningVerifier` classes. The maths runs on the core's extra stack (6.2 KB). Weather Clock reserves that stack as soon as an upload starts, while memory is still in one piece, so the check doesn't fail for lack of memory at the end. If memory is still too short, the clock refuses the update instead of crashing.
- **Recovery mode** (physical: power the clock on and off 3 times; it opens its own network with the password on screen) accepts unsigned firmware. That is the way back to the original GeekMagic firmware, or to keep updating if the private key is lost.

## Why not the 1.20.0 way

In 1.20.0 the signature was **appended to the end of the `.bin.gz`**, in the format of the core's `Updater` (`installSignature`).

- The boot loader (eboot) uses the **last 4 bytes** of the stored file as the size of the decompressed image. With the signature at the end, it read the wrong size, erased its own boot loader, and the clock stopped booting. It took a USB-serial cable to recover.
- 1.19.9 refuses any `.bin.gz` with data after the gzip. From 1.20.1, `sim/security_test.cjs` fails if `installSignature` appears in the code.
- `scripts/check-gz.cjs` checks that the `.gz` ends with the size of the `.bin`.

## Keys

- Private key: `signing/ota-private.pem` next to the repository (or `~/.weatherclock/ota-private.pem`, or `SMALLTV_SIGNING_KEY`).
  - It never goes to git, the website, CI or the clock; `.gitignore` refuses `*private*.pem` and `*.key`.
  - **Keep a copy off the computer.** Without it, clocks only accept new versions through recovery mode.
- Public key: `weatherclock/ota-public.pem`. `ota_key.h` is generated from it with `node scripts/sign.cjs header`.
- Commands:
  - `node scripts/sign.cjs sign <files>` creates the `.sig` files;
  - `node scripts/sign.cjs verify <files>` checks the `.sig` files.
  - `tools/release.sh` (kept outside this repository) signs with `openssl dgst -sha256 -sign` and publishes.
- Current key id: `e103e5ccbc2b2a16` (Weather Clock's own key; the original project used a different one).

## Moving from another firmware

- A clock running the original SmallTV Pomodoro, or any other firmware, can only install Weather Clock through recovery mode, because its key is different.
- From then on, the panel always sends the signature and updates install normally.
