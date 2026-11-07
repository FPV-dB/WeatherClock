// Compresses a firmware for the clock's compressed update: node compress.js <firmware.bin>
// Writes <firmware.bin>.gz with an 8 KB window: that is what the clock can check before allowing the swap
// (a .gz with a bigger window is refused). Also works for the original GeekMagic firmware, to go back
// to it once our firmware exceeds ~530 KB.
const fs = require('fs'), zlib = require('zlib');
const f = process.argv[2];
if (!f) { console.log('uso: node compress.js <firmware.bin>'); process.exit(1); }
const bin = fs.readFileSync(f), gz = zlib.gzipSync(bin, { level: 9, windowBits: 13, memLevel: 9 });
fs.writeFileSync(f + '.gz', gz);
console.log(`${f}.gz: ${gz.length} bytes (${Math.round(gz.length * 100 / bin.length)}% de ${bin.length})`);
