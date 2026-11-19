// The privacy-mode photos view (part of the home-network form) and its
// script, served on its own as /photos.js. All the image work -- cropping to the
// screen's shape, tone, dithering to four gray levels -- happens in the
// browser, so the device only stores bytes. The dithering must match
// tools/photos/bake_photos.py (test/host/test_photos_dither.js checks the
// code between the "dither" markers against it).

#include <string>

#include "setup_page.hpp"

namespace setup_page {
namespace {

constexpr const char* kPhotosView = R"PAGE(
<p class="intro">隱私模式會用一張照片佔滿整個畫面，每天換一張。橫放和直立的照片分開輪播，只會顯示跟裝置目前擺放方向一樣的照片；上傳時會照目前的方向裁切。</p>
<div class="card"><h2><span>已上傳</span></h2><p id="status" class="help">讀取中…</p><div class="thumbs" id="thumbs"></div></div>
<div class="card"><h2><span>新增照片</span></h2>
<p class="help" id="room"></p>
<label class="btn pri file">選擇照片<input type="file" id="file" accept="image/*"></label>
<div id="editor" style="display:none">
  <p class="help">拖曳預覽來移動照片，預覽就是電子紙上的樣子；右下角的虛線框是日期和天氣的位置。縮小可以放進整張照片，空出來的地方填白色或黑色。</p>
  <canvas id="preview"></canvas>
  <div class="f col"><label class="fl" for="zoom">縮放</label><input type="range" id="zoom" min="1" max="3" step="0.01" value="1"></div>
  <div class="f"><label class="lb" for="background">空白處</label><select class="s" id="background"><option value="255">白色</option><option value="0">黑色</option></select></div>
  <div class="f col"><label class="fl" for="brightness">亮度</label><input type="range" id="brightness" min="-80" max="80" step="1" value="0"></div>
  <div class="f col"><label class="fl" for="contrast">對比</label><input type="range" id="contrast" min="-50" max="80" step="1" value="0"></div>
  <div class="f col"><label class="fl" for="method">點陣方式</label><select id="method"><option value="atkinson">Atkinson（乾淨、對比強）</option><option value="fs">Floyd-Steinberg（層次多、顆粒細）</option></select></div>
  <div class="f"><button type="button" class="btn pri wide" id="upload">上傳這張照片</button></div>
</div>
</div>
)PAGE";

constexpr const char* kPhotosScript = R"PAGE((() => {
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

const shapeName = (w, h) => h > w ? '直立' : '橫放';

async function refresh() {
  const mine = ++generation;  // a newer refresh supersedes this one
  const info = await (await fetch('/photos/list')).json();
  if (mine !== generation) return;
  W = info.width; H = info.height;
  const shown = info.photos.filter(p => p.width === W && p.height === H).length;
  status(`已上傳 ${info.photos.length} / ${info.max} 張，${shown} 張是${shapeName(W, H)}的` +
         (!shown ? '（目前輪播內建的浮世繪）' : info.builtIn ? '（和內建的浮世繪一起輪播）' : ''));
  const landscape = info.photos.filter(p => p.width > p.height).length;
  document.dispatchEvent(new CustomEvent('glance-photos', {detail: {
    landscape, portrait: info.photos.length - landscape, total: info.photos.length}}));
  // Room goes by photos, not bytes: every photo takes the same fixed slot.
  const left = info.max - info.photos.length;
  $('room').textContent = left > 0
    ? `還可以再上傳 ${left} 張（每張照片佔一樣的空間，直的橫的都一樣）。`
    : `已經存滿 ${info.max} 張了：先刪掉幾張，才能再上傳。`;
  $('file').disabled = left <= 0;
  const thumbs = $('thumbs');
  thumbs.textContent = '';
  for (const photo of info.photos) {
    const div = document.createElement('div');
    const other = photo.width !== W || photo.height !== H;
    div.className = other ? 'thumb other' : 'thumb';
    const canvas = document.createElement('canvas');
    canvas.width = photo.width; canvas.height = photo.height;  // the right shape while it loads
    const shape = document.createElement('div');
    shape.className = 'shape';
    shape.textContent = shapeName(photo.width, photo.height) + (other ? '（現在的方向不顯示）' : '');
    const remove = document.createElement('button');
    remove.type = 'button';
    remove.className = 'btn';
    remove.textContent = '刪除';
    remove.onclick = async () => {
      if (!confirm('刪除這張照片？')) return;
      await fetch(`/photos/${photo.slot}/delete`, {method: 'POST'});
      refresh();
    };
    div.append(canvas, shape, remove);
    thumbs.append(div);
  }
  // Then fill them in, one request at a time (the device serves one well).
  const canvases = thumbs.querySelectorAll('canvas');
  for (let i = 0; i < info.photos.length; i++) {
    const {slot, width, height} = info.photos[i];
    const bytes = new Uint8Array(await (await fetch(`/photos/${slot}`)).arrayBuffer());
    if (mine !== generation) return;
    drawLevels(canvases[i], unpackGray(bytes, width, height), width, height, 4);
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

// Where the date and the weather go (calendar_view's label, at its usual
// size): drawn over the preview so the photo can be moved clear of it.
function drawLabel(canvas) {
  const ctx = canvas.getContext('2d'), w = 150, h = 300, margin = 16;
  ctx.fillStyle = 'rgba(255,255,255,0.8)';
  ctx.fillRect(W - margin - w, H - margin - h, w, h);
  ctx.strokeStyle = '#000'; ctx.setLineDash([6, 4]); ctx.lineWidth = 2;
  ctx.strokeRect(W - margin - w, H - margin - h, w, h);
}

function render() {
  pending = false;
  if (!source) return;
  drawLevels(preview, ditherPhoto(sourceGray(), 4, $('method').value), W, H, 4);
  drawLabel(preview);
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
  const body = new Blob([pack(ditherPhoto(sourceGray(), 4, $('method').value), W, H, 2)]);
  $('upload').disabled = true;
  status('上傳中…');
  const response = await fetch('/photos/add', {method: 'POST', body});
  $('upload').disabled = false;
  if (!response.ok) { status('上傳失敗：' + await response.text()); return; }
  source = null; $('editor').style.display = 'none'; $('file').value = '';
  refresh();
};

refresh();
})();
)PAGE";

}  // namespace

std::string_view photosView() { return kPhotosView; }

std::string_view photosScript() { return kPhotosScript; }

std::string photosListJson(std::span<const StoredPhoto> photos, size_t max, int width, int height, bool builtIn) {
    std::string json = "{\"max\":" + std::to_string(max) + ",\"width\":" + std::to_string(width) +
                       ",\"height\":" + std::to_string(height) + ",\"builtIn\":" + (builtIn ? "true" : "false") +
                       ",\"photos\":[";
    for (size_t i = 0; i < photos.size(); i++) {
        json += (i ? ",{\"slot\":" : "{\"slot\":") + std::to_string(photos[i].slot) +
                ",\"width\":" + std::to_string(photos[i].width) + ",\"height\":" + std::to_string(photos[i].height) +
                "}";
    }
    return json + "]}";
}

}  // namespace setup_page
