// Checks that the upload page dithers exactly like tools/photos/bake_photos.py:
// runs the page's dither()/pack() (the code between the "dither" markers in
// components/setup_page/photos_page.cpp) on vectors from
// photos_dither_reference.py. Run through `make dither-check`.
const fs = require('fs');
const path = require('path');

const source = fs.readFileSync(path.join(__dirname, '../../components/setup_page/photos_page.cpp'), 'utf8');
const begin = source.indexOf('// --- dither begin ---');
const end = source.indexOf('// --- dither end ---');
if (begin < 0 || end < begin) throw new Error('dither markers not found');
const {dither, pack} = new Function(source.slice(begin, end) + '\nreturn {dither, pack};')();

const dir = process.argv[2];
let failures = 0;
for (const name of fs.readFileSync(path.join(dir, 'cases.txt'), 'utf8').split('\n').filter(Boolean)) {
  const input = fs.readFileSync(path.join(dir, `${name}.gray`));
  const w = input.readUInt16LE(0), h = input.readUInt16LE(2);
  const gray = input.subarray(4);
  for (const [levels, bits, ext] of [[2, 1, 'mono'], [4, 2, 'gray2']]) {
    const expected = fs.readFileSync(path.join(dir, `${name}.${ext}`));
    const actual = Buffer.from(pack(dither(gray, w, h, levels, 'atkinson'), w, h, bits));
    const differing = actual.reduce((n, b, i) => n + (b !== expected[i]), 0);
    if (actual.length !== expected.length || differing) {
      console.log(`FAIL ${name} ${levels} levels: ${differing} of ${expected.length} bytes differ`);
      failures++;
    } else {
      console.log(`ok   ${name} ${levels} levels (${w}x${h}, ${expected.length} bytes identical)`);
    }
  }
}
process.exit(failures ? 1 : 0);
