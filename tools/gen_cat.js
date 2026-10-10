/*
 * 小猫位图生成器（Node.js，仅用于开发期生成 C 数组，不参与固件编译）
 *
 *   node tools/gen_cat.js
 *
 * 输出：
 *   - 终端打印 ASCII 预览（# = 点亮像素），用来肉眼确认画得像不像猫
 *   - Core/Src/cat_bitmap.c / Core/Inc/cat_bitmap.h
 *
 * 打包格式与 SSD1306 显存一致：按页(8 行)打包，每页 WIDTH 个字节，
 * 字节 bit0 = 该页最上面一行。
 */
'use strict';

const fs = require('fs');
const path = require('path');

const W = 56;
const H = 56;

const newBuf = () => new Uint8Array(W * H);

function px(b, x, y, v) {
  x = Math.round(x);
  y = Math.round(y);
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  b[y * W + x] = v ? 1 : 0;
}

function fillCircle(b, cx, cy, r, v) {
  for (let y = Math.floor(cy - r); y <= Math.ceil(cy + r); y++) {
    for (let x = Math.floor(cx - r); x <= Math.ceil(cx + r); x++) {
      if ((x - cx) ** 2 + (y - cy) ** 2 <= r * r) px(b, x, y, v);
    }
  }
}

function fillEllipse(b, cx, cy, rx, ry, v) {
  for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++) {
    for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) {
      if (((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1) px(b, x, y, v);
    }
  }
}

function fillTri(b, p0, p1, p2, v) {
  const minX = Math.floor(Math.min(p0[0], p1[0], p2[0]));
  const maxX = Math.ceil(Math.max(p0[0], p1[0], p2[0]));
  const minY = Math.floor(Math.min(p0[1], p1[1], p2[1]));
  const maxY = Math.ceil(Math.max(p0[1], p1[1], p2[1]));
  const area = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1]);
  if (area === 0) return;
  for (let y = minY; y <= maxY; y++) {
    for (let x = minX; x <= maxX; x++) {
      const w0 = ((p1[0] - x) * (p2[1] - y) - (p2[0] - x) * (p1[1] - y)) / area;
      const w1 = ((p2[0] - x) * (p0[1] - y) - (p0[0] - x) * (p2[1] - y)) / area;
      const w2 = 1 - w0 - w1;
      if (w0 >= 0 && w1 >= 0 && w2 >= 0) px(b, x, y, v);
    }
  }
}

function line(b, x0, y0, x1, y1, v, thick = 1) {
  const steps = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0)) * 2 + 1;
  const off = (thick - 1) / 2;
  for (let i = 0; i <= steps; i++) {
    const t = i / steps;
    const x = x0 + (x1 - x0) * t;
    const y = y0 + (y1 - y0) * t;
    for (let dy = 0; dy < thick; dy++) px(b, x, y - off + dy, v);
  }
}

function quad(b, p0, pc, p1, v, thick = 1) {
  const steps = 60;
  const off = (thick - 1) / 2;
  for (let i = 0; i <= steps; i++) {
    const t = i / steps;
    const x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * pc[0] + t * t * p1[0];
    const y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * pc[1] + t * t * p1[1];
    for (let dy = 0; dy < thick; dy++) px(b, x, y - off + dy, v);
  }
}

/* ------------------------------ 画猫 ------------------------------ */
const ON = 1;
const OFF = 0;

function drawCat(blink) {
  const b = newBuf();
  const cx = 28;

  /* 耳朵（底边落在头部轮廓内，保证与头连成一体） */
  fillTri(b, [9, 4], [22, 16], [13, 21], ON);
  fillTri(b, [47, 4], [34, 16], [43, 21], ON);

  /* 头 */
  fillCircle(b, cx, 33, 20, ON);

  /* 耳内（掏白） */
  fillTri(b, [12, 9], [18, 15], [14, 17], OFF);
  fillTri(b, [44, 9], [38, 15], [42, 17], OFF);

  /* 眼睛：白色眼白 + 竖瞳（眨眼时改成一条横线） */
  fillEllipse(b, 19, 31, 6, 7, OFF);
  fillEllipse(b, 37, 31, 6, 7, OFF);
  if (blink) {
    line(b, 14, 32, 24, 32, ON, 2);
    line(b, 32, 32, 42, 32, ON, 2);
  } else {
    fillEllipse(b, 19, 32, 2.4, 6, ON);
    fillEllipse(b, 37, 32, 2.4, 6, ON);
  }

  /* 嘴部（掏白） + 鼻子 + 嘴 */
  fillEllipse(b, cx, 44, 10, 5, OFF);
  fillTri(b, [25, 40], [31, 40], [28, 44], ON);
  line(b, 28, 44, 28, 47, ON, 1);
  quad(b, [28, 47], [24, 51], [20, 46], ON, 1);
  quad(b, [28, 47], [32, 51], [36, 46], ON, 1);

  /* 胡须：只画到头部轮廓附近，露出背景的那一段可见，不横穿嘴部 */
  line(b, 16, 43, 1, 39, ON, 1);
  line(b, 16, 46, 0, 46, ON, 1);
  line(b, 16, 49, 2, 53, ON, 1);
  line(b, 40, 43, 54, 39, ON, 1);
  line(b, 40, 46, 55, 46, ON, 1);
  line(b, 40, 49, 53, 53, ON, 1);

  return b;
}

/* --------------------------- 页式打包 ---------------------------- */
function pack(b) {
  const pages = H / 8;
  const out = [];
  for (let p = 0; p < pages; p++) {
    for (let x = 0; x < W; x++) {
      let byte = 0;
      for (let bit = 0; bit < 8; bit++) {
        if (b[(p * 8 + bit) * W + x]) byte |= 1 << bit;
      }
      out.push(byte);
    }
  }
  return out;
}

function preview(name, b) {
  console.log(`--- ${name} (${W}x${H}) ---`);
  for (let y = 0; y < H; y++) {
    let row = '';
    for (let x = 0; x < W; x++) row += b[y * W + x] ? '#' : '.';
    console.log(row);
  }
  console.log('');
}

function cArray(name, bytes) {
  const lines = [];
  lines.push(`const uint8_t ${name}[${bytes.length}] = {`);
  for (let i = 0; i < bytes.length; i += 12) {
    lines.push('    ' + bytes.slice(i, i + 12).map((v) => `0x${v.toString(16).padStart(2, '0')}`).join(', ') + ',');
  }
  lines.push('};');
  return lines.join('\n');
}

const open = drawCat(false);
const blink = drawCat(true);
preview('cat_open', open);
preview('cat_blink', blink);

const root = path.join(__dirname, '..');
const pages = H / 8;

const cSrc = `/*
 * 本文件由 tools/gen_cat.js 自动生成，请勿手工修改。
 * 小猫位图 ${W}x${H}，按 SSD1306 页式显存格式打包（${pages} 页 x ${W} 字节），bit0 = 页内最上面一行。
 */
#include "cat_bitmap.h"

${cArray('cat_bitmap_open', pack(open))}

${cArray('cat_bitmap_blink', pack(blink))}
`;

const hSrc = `/*
 * 本文件由 tools/gen_cat.js 自动生成，请勿手工修改。
 */
#ifndef CAT_BITMAP_H
#define CAT_BITMAP_H

#include <stdint.h>

#define CAT_IMG_W    ${W}u
#define CAT_IMG_H    ${H}u
#define CAT_IMG_SIZE ${W * pages}u

extern const uint8_t cat_bitmap_open[CAT_IMG_SIZE];
extern const uint8_t cat_bitmap_blink[CAT_IMG_SIZE];

#endif /* CAT_BITMAP_H */
`;

fs.mkdirSync(path.join(root, 'Core', 'Inc'), { recursive: true });
fs.mkdirSync(path.join(root, 'Core', 'Src'), { recursive: true });
fs.writeFileSync(path.join(root, 'Core', 'Src', 'cat_bitmap.c'), cSrc);
fs.writeFileSync(path.join(root, 'Core', 'Inc', 'cat_bitmap.h'), hSrc);
console.log('written: Core/Src/cat_bitmap.c, Core/Inc/cat_bitmap.h');
