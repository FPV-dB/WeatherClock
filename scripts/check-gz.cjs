// Checks a .bin.gz the way the ESP8266 eboot will consume it: node scripts/check-gz.cjs <firmware.bin> <firmware.bin.gz>
// The eboot (copy_raw) reads the image size from the LAST 4 bytes of the written file, not from the end of the gzip stream, and
// decompresses with uzlib (8 KB window). Any byte after the gzip (like the 1.20.0 signature) makes the eboot
// erase its own sector 0 and the clock no longer boots. That is why nothing goes out without passing through here.
const fs = require('fs'), zlib = require('zlib');

// Returns the list of problems (empty = the eboot will accept it).
function conferir(bin, gz) {
  const erros = [];
  if (bin[0] !== 0xE9) erros.push(`the .bin doesn't start with the magic byte 0xE9 (it starts with 0x${(bin[0] ?? 0).toString(16)})`);
  if (gz.length < 18 || gz[0] !== 0x1f || gz[1] !== 0x8b || gz[2] !== 8) return [...erros, 'the .bin.gz is not a gzip (deflate)'];
  const ult = gz.readUInt32LE(gz.length - 4);
  if (ult !== bin.length) erros.push(`the last 4 bytes of the .bin.gz say ${ult}, but the .bin has ${bin.length} bytes (the eboot would erase the boot loader)`);
  // minimal gzip: 10-byte header with no optional fields (what zlib/compress.js writes) + deflate + CRC32 + size
  if (gz[3] !== 0) { erros.push('the gzip header has optional fields (name, comment...); the one compress.js writes has none'); return erros; }
  let r;
  try {
    // windowBits 13 = 8 KB window: zlib rejects longer distances, like the clock's uzlib
    r = zlib.inflateRawSync(gz.subarray(10), { windowBits: 13, info: true });
  } catch (e) {
    return [...erros, `doesn't decompress with an 8 KB window (${e.code || e.message}); make it with node compress.js`];
  }
  if (!r.buffer.equals(bin)) erros.push('the decompressed .bin.gz is not identical to the .bin');
  const depois = gz.length - (10 + r.engine.bytesWritten + 8);
  if (depois !== 0) erros.push(`${depois} byte(s) after the end of the gzip (signature or garbage): the eboot would read the wrong size`);
  else {
    const t = gz.length - 8;
    if (gz.readUInt32LE(t + 4) !== bin.length % 2 ** 32) erros.push('the size in the gzip trailer doesn't match the .bin');
    if (zlib.crc32 && gz.readUInt32LE(t) !== zlib.crc32(bin)) erros.push("the gzip trailer's CRC32 doesn't match the .bin");
  }
  return erros;
}
module.exports = { conferir };

if (require.main === module) {
  const [b, g] = process.argv.slice(2);
  if (!b || !g) { console.error('uso: node scripts/check-gz.cjs <firmware.bin> <firmware.bin.gz>'); process.exit(2); }
  const e = conferir(fs.readFileSync(b), fs.readFileSync(g));
  if (e.length) { console.error(`ERROR: ${g} is NOT safe for the eboot:\n` + e.map(x => '  - ' + x).join('\n')); process.exit(1); }
  console.log(`ok: ${g} matches the eboot (size ${fs.statSync(b).size} in the last 4 bytes, 8 KB window, nothing after the gzip, magic 0xE9)`);
}
