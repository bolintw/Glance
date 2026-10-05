// The privacy-mode photos page. All the image work -- cropping, tone,
// dithering to 1bpp and to four gray levels -- happens in the browser, so
// the device only stores bytes. The dithering must match
// tools/photos/bake_photos.py (test/host/test_photos_dither.js checks the
// code between the "dither" markers against it).

#include <string>

#include "setup_page.hpp"

namespace setup_page {
namespace {

constexpr const char* kPhotosPage = R"PAGE(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>隱私模式照片</title>
<link rel="icon" href="data:,">
<style>
body{font-family:system-ui,sans-serif;margin:0 auto;max-width:34rem;padding:1rem;line-height:1.5;color:#111;background:#fff}
h1{font-size:1.4rem}h2{font-size:1.1rem;margin:1.6rem 0 .4rem}
.note{color:#555;font-size:.9rem}
.thumbs{display:grid;grid-template-columns:1fr 1fr;gap:.6rem}
.thumb canvas{width:100%;border:1px solid #999;display:block;background:#ddd}
.thumb button{margin-top:.2rem;width:100%}
#preview{width:100%;border:1px solid #333;touch-action:none;cursor:grab;display:block}
label{display:block;margin:.6rem 0 .2rem}
input[type=range]{width:100%}
button{padding:.6rem;font-size:1rem}
.primary{width:100%;margin-top:1rem;font-size:1.1rem}
#editor{display:none}
#status{font-weight:600}
</style></head><body>
<p><a href="/" id="back">← 回到設定</a></p>
<h1>隱私模式照片</h1>
<p class="note">隱私模式會在框裡顯示一張照片，每天換一張。上傳自己的照片之後，就只輪播自己的照片。</p>
<p id="status"></p>
<div class="thumbs" id="thumbs"></div>

<h2>新增照片</h2>
<input type="file" id="file" accept="image/*">
<div id="editor">
  <p class="note">拖曳預覽來移動照片，預覽就是電子紙上的樣子。縮小可以放進整張照片，空出來的地方填白色或黑色。</p>
  <canvas id="preview"></canvas>
  <label>縮放</label><input type="range" id="zoom" min="1" max="3" step="0.01" value="1">
  <label>空白處</label>
  <select id="background"><option value="255">白色</option><option value="0">黑色</option></select>
  <label>亮度</label><input type="range" id="brightness" min="-80" max="80" step="1" value="0">
  <label>對比</label><input type="range" id="contrast" min="-50" max="80" step="1" value="0">
  <label>點陣方式</label>
  <select id="method"><option value="atkinson">Atkinson（乾淨、對比強）</option><option value="fs">Floyd-Steinberg（層次多、顆粒細）</option></select>
  <button class="primary" id="upload">上傳這張照片</button>
</div>

<script>
// --- dither begin ---
// Each pixel of `gray` (0-255) quantized to 0..levels-1 (0 = black) by
// error diffusion. Float64 and floor(x + 0.5) on purpose: bit-for-bit the
// same as tools/photos/bake_photos.py.
function dither(gray, w, h, levels, method) {
  const px = Float64Array.from(gray);
  const out = new Uint8Array(w * h);
  const step = 255 / (levels - 1);
  const kernel = method === 'fs'
    ? [[1, 0, 7 / 16], [-1, 1, 3 / 16], [0, 1, 5 / 16], [1, 1, 1 / 16]]
    : [[1, 0, 1 / 8], [2, 0, 1 / 8], [-1, 1, 1 / 8], [0, 1, 1 / 8], [1, 1, 1 / 8], [0, 2, 1 / 8]];
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      const old = px[i];
      const level = Math.min(levels - 1, Math.max(0, Math.floor(old / step + 0.5)));
      out[i] = level;
      const err = old - level * step;
      for (const [dx, dy, f] of kernel) {
        const xx = x + dx, yy = y + dy;
        if (xx >= 0 && xx < w && yy < h) px[yy * w + xx] += err * f;
      }
    }
  }
  return out;
}

// Rows of `bits`-per-pixel values, MSB first, each row padded to a byte.
function pack(levels, w, h, bits) {
  const perByte = 8 / bits, rowBytes = Math.ceil(w / perByte);
  const out = new Uint8Array(rowBytes * h);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++)
      out[y * rowBytes + Math.floor(x / perByte)] |= levels[y * w + x] << (8 - bits - bits * (x % perByte));
  return out;
}
// --- dither end ---

let W = 0, H = 0, source = null, offsetX = 0, offsetY = 0, lastCover = 0, pending = false, generation = 0;
let photoRect = null;  // where the photo landed in the frame, in whole pixels
const $ = id => document.getElementById(id);
const preview = $('preview');

function status(text) { $('status').textContent = text; }

function drawLevels(canvas, levels, w, h, levelCount) {
  canvas.width = w; canvas.height = h;
  const ctx = canvas.getContext('2d'), img = ctx.createImageData(w, h);
  const scale = 255 / (levelCount - 1);
  for (let i = 0; i < w * h; i++) {
    const v = levels[i] * scale;
    img.data[4 * i] = img.data[4 * i + 1] = img.data[4 * i + 2] = v;
    img.data[4 * i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function unpackGray(bytes, w, h) {
  const rowBytes = Math.ceil(w / 4), levels = new Uint8Array(w * h);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++)
      levels[y * w + x] = (bytes[y * rowBytes + (x >> 2)] >> (6 - 2 * (x & 3))) & 3;
  return levels;
}

async function refresh() {
  const mine = ++generation;  // a newer refresh supersedes this one
  const info = await (await fetch('/photos/list')).json();
  if (mine !== generation) return;
  W = info.width; H = info.height;
  status(`已上傳 ${info.slots.length} / ${info.max} 張` + (info.slots.length ? '' : '（目前輪播內建的浮世繪）'));
  $('file').disabled = info.slots.length >= info.max;
  const thumbs = $('thumbs');
  thumbs.textContent = '';
  for (const slot of info.slots) {
    const div = document.createElement('div');
    div.className = 'thumb';
    const canvas = document.createElement('canvas');
    canvas.width = W; canvas.height = H;  // the right shape while it loads
    const remove = document.createElement('button');
    remove.textContent = '刪除';
    remove.onclick = async () => {
      if (!confirm('刪除這張照片？')) return;
      await fetch(`/photos/${slot}/delete`, {method: 'POST'});
      refresh();
    };
    div.append(canvas, remove);
    thumbs.append(div);
  }
  // Then fill them in, one request at a time (the device serves one well).
  const canvases = thumbs.querySelectorAll('canvas');
  for (let i = 0; i < info.slots.length; i++) {
    const bytes = new Uint8Array(await (await fetch(`/photos/${info.slots[i]}`)).arrayBuffer());
    if (mine !== generation) return;
    drawLevels(canvases[i], unpackGray(bytes, W, H), W, H, 4);
  }
}

function inPhoto(i) {
  const x = i % W, y = Math.floor(i / W);
  return x >= photoRect.x0 && x < photoRect.x1 && y >= photoRect.y0 && y < photoRect.y1;
}

// The photo placed in the frame, as 0-255 gray with the tone sliders
// applied (autocontrast first, like the built-in photos). Zoomed out below
// the frame, the rest is the background colour, left out of the tone work.
function sourceGray() {
  const work = document.createElement('canvas');
  work.width = W; work.height = H;
  const ctx = work.getContext('2d');
  const cover = Math.max(W / source.width, H / source.height) * Number($('zoom').value);
  if (lastCover && cover !== lastCover) {  // zoom about the frame's centre
    offsetX = W / 2 - (W / 2 - offsetX) * cover / lastCover;
    offsetY = H / 2 - (H / 2 - offsetY) * cover / lastCover;
  }
  lastCover = cover;
  const dw = source.width * cover, dh = source.height * cover;
  // Larger than the frame: no gaps at the edges. Smaller: stays inside it.
  offsetX = Math.min(Math.max(0, W - dw), Math.max(Math.min(0, W - dw), offsetX));
  offsetY = Math.min(Math.max(0, H - dh), Math.max(Math.min(0, H - dh), offsetY));
  const background = Number($('background').value);
  ctx.fillStyle = background ? '#fff' : '#000';
  ctx.fillRect(0, 0, W, H);
  ctx.drawImage(source, offsetX, offsetY, dw, dh);
  photoRect = {x0: Math.max(0, Math.round(offsetX)), y0: Math.max(0, Math.round(offsetY)),
               x1: Math.min(W, Math.round(offsetX + dw)), y1: Math.min(H, Math.round(offsetY + dh))};
  const rgba = ctx.getImageData(0, 0, W, H).data;
  const gray = new Float64Array(W * H);
  const histogram = new Array(256).fill(0);
  let photoPixels = 0;
  for (let i = 0; i < W * H; i++) {
    gray[i] = 0.299 * rgba[4 * i] + 0.587 * rgba[4 * i + 1] + 0.114 * rgba[4 * i + 2];
    if (inPhoto(i)) { histogram[Math.round(gray[i])]++; photoPixels++; }
  }
  const cut = photoPixels * 0.01;
  let lo = 0, hi = 255, seen = 0;
  while (lo < 255 && (seen += histogram[lo]) <= cut) lo++;
  seen = 0;
  while (hi > 0 && (seen += histogram[hi]) <= cut) hi--;
  const span = Math.max(1, hi - lo);
  const contrast = 1 + Number($('contrast').value) / 100, brightness = Number($('brightness').value);
  for (let i = 0; i < gray.length; i++) {
    if (!inPhoto(i)) { gray[i] = background; continue; }
    const stretched = (gray[i] - lo) * 255 / span;
    gray[i] = Math.min(255, Math.max(0, (stretched - 128) * contrast + 128 + brightness));
  }
  return gray;
}

// Dithered levels with the background kept clean: error diffusion would
// otherwise scatter dots from the photo's edge into it.
function ditherPhoto(gray, levels, method) {
  const out = dither(gray, W, H, levels, method);
  const fill = Number($('background').value) ? levels - 1 : 0;
  for (let i = 0; i < out.length; i++) if (!inPhoto(i)) out[i] = fill;
  return out;
}

function render() {
  pending = false;
  if (!source) return;
  drawLevels(preview, ditherPhoto(sourceGray(), 4, $('method').value), W, H, 4);
}
function schedule() { if (!pending) { pending = true; requestAnimationFrame(render); } }

$('file').onchange = () => {
  const file = $('file').files[0];
  if (!file) return;
  const img = new Image();
  img.onload = () => {
    source = img;
    // All the way out, the whole photo fits in the frame.
    $('zoom').min = (Math.min(W / img.width, H / img.height) / Math.max(W / img.width, H / img.height)).toFixed(3);
    $('zoom').value = 1;
    const cover = Math.max(W / img.width, H / img.height);  // start centred
    lastCover = cover;
    offsetX = (W - img.width * cover) / 2; offsetY = (H - img.height * cover) / 2;
    $('editor').style.display = 'block';
    schedule();
  };
  img.src = URL.createObjectURL(file);
};
for (const id of ['zoom', 'background', 'brightness', 'contrast', 'method']) $(id).oninput = schedule;

let drag = null;
preview.onpointerdown = e => { drag = {x: e.clientX, y: e.clientY}; preview.setPointerCapture(e.pointerId); };
preview.onpointermove = e => {
  if (!drag) return;
  const scale = W / preview.clientWidth;
  offsetX += (e.clientX - drag.x) * scale; offsetY += (e.clientY - drag.y) * scale;
  drag = {x: e.clientX, y: e.clientY};
  schedule();
};
preview.onpointerup = preview.onpointercancel = () => { drag = null; };

$('upload').onclick = async () => {
  const gray = sourceGray(), method = $('method').value;
  const body = new Blob([pack(ditherPhoto(gray, 2, method), W, H, 1), pack(ditherPhoto(gray, 4, method), W, H, 2)]);
  $('upload').disabled = true;
  status('上傳中…');
  const response = await fetch('/photos/add', {method: 'POST', body});
  $('upload').disabled = false;
  if (!response.ok) { status('上傳失敗：' + await response.text()); return; }
  source = null; $('editor').style.display = 'none'; $('file').value = '';
  refresh();
};

// Opened in its own tab from the settings page: going back closes this tab,
// which leaves the settings page as it was, half-filled form included.
$('back').onclick = e => {
  if (window.opener && !window.opener.closed) { e.preventDefault(); window.close(); }
};

refresh();
</script>
</body></html>
)PAGE";

}  // namespace

std::string renderPhotosPage() { return kPhotosPage; }

std::string photosListJson(std::span<const int> slots, size_t max, int width, int height) {
    std::string json = "{\"max\":" + std::to_string(max) + ",\"width\":" + std::to_string(width) +
                       ",\"height\":" + std::to_string(height) + ",\"slots\":[";
    for (size_t i = 0; i < slots.size(); i++) {
        json += (i ? "," : "") + std::to_string(slots[i]);
    }
    return json + "]}";
}

}  // namespace setup_page
