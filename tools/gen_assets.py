#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# gen_assets.py - deterministic procedural generator for Mountain's bitmap and
# sound assets.
#
# The script itself is MIT licensed (Mountain contributors).
# The assets it GENERATES (assets/textures/*.png, assets/sounds/*.wav) are
# licensed CC BY 4.0, (c) Mountain contributors.
# assets/fonts/font8x8.png is derived from Daniel Hepper's public-domain
# font8x8 (tools/font8x8_basic.h).
#
# Every random choice comes from fixed seeds, so re-running the script produces
# byte-identical files.  Requires Python 3, numpy and Pillow only.
#
# Usage:  python3 tools/gen_assets.py [--preview DIR]
#   --preview DIR   also write upscaled contact sheets of every atlas into DIR

import math
import os
import re
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX_DIR = os.path.join(ROOT, "assets", "textures")
SND_DIR = os.path.join(ROOT, "assets", "sounds")
FONT_DIR = os.path.join(ROOT, "assets", "fonts")
SEED = 20261119

LIGHT = np.array([-0.55, -0.65, 0.52])          # towards the light: upper-left, front
LIGHT /= np.linalg.norm(LIGHT)
LX, LY, LZ = LIGHT

def R(*key):
    """Deterministic RNG for a given key (ints only - never str hashes)."""
    return np.random.default_rng([SEED, *key])

def C(*c):
    return np.array(c, float)

def gray(l):
    """Map a 0..1 shade to the 0.45..1.0 luminance band used by tinted tiles."""
    return 0.45 + 0.55 * np.clip(l, 0, 1)

def smooth(e0, e1, x):
    t = np.clip((np.asarray(x, float) - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)

def colmul(col, f):
    col = np.asarray(col, float)
    return col * f if col.ndim == 0 else col * np.asarray(f)[..., None]

def lerp(a, b, t):
    t = np.asarray(t, float)
    return a + (b - a) * (t[..., None] if t.ndim else t)

def bez(p0, p1, p2, k=10):
    t = np.linspace(0, 1, k + 1)[:, None]
    p0, p1, p2 = (np.asarray(p, float) for p in (p0, p1, p2))
    return (1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2

# ---- Tileable noise / cellular helpers -------------------------------------

def snoise(h, w, rng, p=1.0, fmin=1.0, fmax=None, stretch=(1.0, 1.0)):
    """Tileable noise by spectral synthesis (mean 0, std 1).
    p: amplitude falloff exponent (1/f^p); stretch=(sy, sx) elongates features."""
    fy, fx = np.fft.fftfreq(h)[:, None] * h, np.fft.fftfreq(w)[None, :] * w
    f = np.sqrt((fy * stretch[0]) ** 2 + (fx * stretch[1]) ** 2)
    amp = np.where(f >= fmin, 1.0 / np.maximum(f, 1e-9) ** p, 0.0)
    if fmax:
        amp *= np.exp(-(f / fmax) ** 2)
    z = np.fft.ifft2(np.fft.fft2(rng.standard_normal((h, w))) * amp).real
    return (z - z.mean()) / (z.std() + 1e-12)

def voronoi(n, pts, sy=1.0):
    """Wrap-around Voronoi on an n x n tile: F1, F2, nearest id, dx, dy to it.
    sy < 1 elongates cells vertically."""
    yy, xx = np.mgrid[0:n, 0:n] + 0.5
    dx, dy = (xx[..., None] - pts[:, 0] + n / 2) % n - n / 2, (yy[..., None] - pts[:, 1] + n / 2) % n - n / 2
    d = np.hypot(dx, dy * sy)
    o = np.argsort(d, axis=-1)
    i1, f1 = o[..., 0], np.take_along_axis(d, o[..., :1], -1)[..., 0]
    f2 = np.take_along_axis(d, o[..., 1:2], -1)[..., 0]
    ddx, ddy = np.take_along_axis(dx, o[..., :1], -1)[..., 0], np.take_along_axis(dy, o[..., :1], -1)[..., 0]
    return f1, f2, i1, ddx, ddy

def jitter_pts(rng, cells, jit, n=64):
    s, g = n / cells, np.array([(i, j) for j in range(cells) for i in range(cells)], float)
    return (g + 0.5) * s + rng.uniform(-jit, jit, g.shape)

def relief(h, k=1.0):
    """Lambert shading of a tileable height field (wraps)."""
    hx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * k
    hy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * k
    nz = 1.0 / np.sqrt(hx * hx + hy * hy + 1)
    return np.clip((-hx * LX - hy * LY + LZ) * nz, 0, 1)

def grid64(n=64):
    yy, xx = np.mgrid[0:n, 0:n] + 0.5
    return xx, yy

def edge_fill(t):
    """Set RGB of transparent pixels to the average colour of the opaque edge."""
    a = t[..., 3] > 0.5
    if a.any() and not a.all():
        p = np.pad(a, 1)
        edge = a & (~p[:-2, 1:-1] | ~p[2:, 1:-1] | ~p[1:-1, :-2] | ~p[1:-1, 2:])
        t[~a, :3] = t[edge, :3].mean(0)
    return t

def rgba(rgb, a=1.0):
    a = np.broadcast_to(np.asarray(a, float), rgb.shape[:2])
    return np.dstack([np.clip(rgb, 0, 1), a])

# ---- Supersampled sprite painter -------------------------------------------

class Sprite:
    """Painter for one tile; coordinates are tile pixels, rendered at ss x ss."""

    def __init__(self, seed, n=64, ss=4):
        self.n, self.ss = n, ss
        N = n * ss
        self.rgb = np.zeros((N, N, 3))
        self.a = np.zeros((N, N))
        self.tex = snoise(N, N, R(9000, seed), p=0.9, fmin=8)

    def grid(self, x0, y0, x1, y1):
        s, N = self.ss, self.n * self.ss
        j0, j1 = max(int(math.floor(x0 * s)), 0), min(int(math.ceil(x1 * s)), N)
        i0, i1 = max(int(math.floor(y0 * s)), 0), min(int(math.ceil(y1 * s)), N)
        if j1 <= j0 or i1 <= i0:
            return None, None, None
        X, Y = np.meshgrid((np.arange(j0, j1) + 0.5) / s, (np.arange(i0, i1) + 0.5) / s)
        return (slice(i0, i1), slice(j0, j1)), X, Y

    def put(self, sl, m, col):
        if not m.any():
            return
        col = np.asarray(col, float)
        if col.ndim == 0:
            col = np.repeat(col, 3)
        elif col.ndim == 2:
            col = col[..., None].repeat(3, -1)
        self.rgb[sl][m] = np.broadcast_to(col, m.shape + (3,))[m]
        self.a[sl][m] = 1.0

    def capsule(self, x0, y0, x1, y1, r0, r1, col):
        pad = max(r0, r1) + 1
        sl, X, Y = self.grid(min(x0, x1) - pad, min(y0, y1) - pad, max(x0, x1) + pad, max(y0, y1) + pad)
        if sl is None:
            return
        dx, dy = x1 - x0, y1 - y0
        t = np.clip(((X - x0) * dx + (Y - y0) * dy) / max(dx * dx + dy * dy, 1e-9), 0, 1)
        d = np.hypot(X - x0 - t * dx, Y - y0 - t * dy)
        self.put(sl, d <= r0 + (r1 - r0) * t, col)

    def stroke(self, pts, r0, r1, c0, c1=None):
        pts = np.asarray(pts, float)
        k, c0 = len(pts) - 1, np.asarray(c0, float)
        c1 = c0 if c1 is None else np.asarray(c1, float)
        for i in range(k):
            t0, t1 = i / k, (i + 1) / k
            self.capsule(*pts[i], *pts[i + 1], r0 + (r1 - r0) * t0, r0 + (r1 - r0) * t1, c0 + (c1 - c0) * (t0 + t1) / 2)

    def leaf(self, bx, by, ang, L, W, col, shape=0.7, serr=0.0, rib=0.0, edge=0.8, curl=0.0):
        ca, sa = math.cos(ang), math.sin(ang)
        ex, ey = bx + ca * L, by + sa * L
        pad = W + abs(curl) * L + 1
        sl, X, Y = self.grid(min(bx, ex) - pad, min(by, ey) - pad, max(bx, ex) + pad, max(by, ey) + pad)
        if sl is None:
            return
        dx, dy = X - bx, Y - by
        u = (dx * ca + dy * sa) / L
        uc = np.clip(u, 0, 1)
        v, hw = -dx * sa + dy * ca - curl * L * uc * uc, 0.5 * W * np.sin(np.pi * uc ** 0.85) ** shape
        if serr:
            hw = hw * (1 + serr * np.sin(uc * L * 2.6))
        m = (u >= 0) & (u <= 1) & (np.abs(v) <= hw)
        if not m.any():
            return
        nrm = np.clip(v / np.maximum(hw, 1e-6), -1, 1)
        f = (1 + 0.28 * nrm * (-sa * LX + ca * LY)) * (0.9 + 0.18 * uc)
        f = f * np.where(np.abs(v) > hw - 0.45, edge, 1.0)
        if rib:
            f = f * np.where((np.abs(v) < 0.28) & (uc < 0.85), 1 - rib, 1.0)
        f = f * (1 + 0.035 * self.tex[sl])
        self.put(sl, m, colmul(col, f))

    def blob(self, cx, cy, rx, ry, col, rng=None, rough=0.0, shade=False, amb=0.45,
             spec=0.0, tex=0.0, ang=0.0, clip_y=None, col2=None, harm=6):
        Rr = max(rx, ry) * (1 + rough * 1.6) + 1
        sl, X, Y = self.grid(cx - Rr, cy - Rr, cx + Rr, cy + Rr)
        if sl is None:
            return
        ca, sa = math.cos(ang), math.sin(ang)
        dx0, dy0 = X - cx, Y - cy
        dx, dy = dx0 * ca + dy0 * sa, -dx0 * sa + dy0 * ca
        rad = 1.0
        if rough and rng is not None:
            th = np.arctan2(dy, dx)
            for k in range(2, harm + 2):
                rad = rad + rough * rng.uniform(0.3, 1.0) / k * np.sin(k * th + rng.uniform(0, 6.283))
        nx, ny = dx / (rx * rad), dy / (ry * rad)
        q = nx * nx + ny * ny
        m = q <= 1
        if clip_y is not None:
            m &= Y <= clip_y
        f = np.ones_like(q)
        if shade:
            nz = np.sqrt(np.clip(1 - q, 0, 1))
            ix, iy = nx * ca - ny * sa, nx * sa + ny * ca
            lam = np.clip(ix * LX + iy * LY + nz * LZ, 0, 1)
            f = amb + (1 - amb) * lam + spec * lam ** 12
        if tex:
            f = f * (1 + tex * self.tex[sl])
        if col2 is not None:
            c0, c1 = np.asarray(col, float), np.asarray(col2, float)
            c = c0 + (c1 - c0) * np.clip(f, 0, 1.3)[..., None]
        else:
            c = colmul(col, f)
        self.put(sl, m, c)

    def resolve(self, is_gray=False, thresh=0.4):
        n, s = self.n, self.ss
        a = self.a.reshape(n, s, n, s)
        w = a.sum((1, 3))
        rgb = (self.rgb * self.a[..., None]).reshape(n, s, n, s, 3).sum((1, 3)) / np.maximum(w, 1e-9)[..., None]
        alpha = (a.mean((1, 3)) >= thresh)
        if is_gray:   # consistent contrast across species: stretch, then gamma to a common median
            v = rgb[..., 0]
            lo, hi = np.percentile(v[alpha], (1, 99.5))
            u = np.clip((v - lo) / (hi - lo), 0, 1)
            u = u ** np.clip(math.log(0.42) / math.log(max(np.median(u[alpha]), 1e-3)), 0.5, 2.0)
            rgb = np.repeat(gray(u)[..., None], 3, -1)
        return edge_fill(rgba(rgb, alpha.astype(float)))

def wrap9(fn, n=64):
    for oy in (-n, 0, n):
        for ox in (-n, 0, n):
            fn(ox, oy)

# ---- Foliage atlas ---------------------------------------------------------

def leaf_mass(sp, rng, lobes, n, L, W, shape=0.7, serr=0.0, col=None, out=0.75, back=0.1, droop=0.0, rib=0.0):
    """Clump of overlapping leaves lit as a lumpy sphere (upper-left light)."""
    for (x, y, rx, ry) in lobes:
        c = gray(back) if col is None else np.asarray(col) * 0.35
        sp.blob(x + 0.5, y + 0.8, rx * 0.8, ry * 0.8, c, rng, rough=0.3)
    area, items = np.array([l[2] * l[3] for l in lobes]), []
    for _ in range(n):
        x0, y0, rx, ry = lobes[rng.choice(len(lobes), p=area / area.sum())]
        d = rng.normal(size=3)
        d /= np.linalg.norm(d)
        r, lam = rng.random() ** 0.45, max(float(d @ LIGHT), 0.0)
        l = (0.12 + 0.88 * lam) * (0.5 + 0.5 * r) + rng.normal(0, 0.06)
        a = math.atan2(d[1], d[0]) + rng.normal(0, 0.35 + 1.2 * (1 - out)) + droop
        items.append((d[2] * r, x0 + d[0] * r * rx, y0 + d[1] * r * ry, a, l))
    items.sort(key=lambda t: t[0])
    for _, x, y, a, l in items:
        c = gray(l) if col is None else np.asarray(col) * (0.45 + 0.75 * np.clip(l, 0, 1))
        ll = L * rng.uniform(0.8, 1.15)
        sp.leaf(x - math.cos(a) * ll * 0.35, y - math.sin(a) * ll * 0.35, a, ll,
                W * rng.uniform(0.85, 1.1), c, shape, serr, rib)

def random_lobes(rng, cx, cy, R0, k, spread):
    lobes = [(cx, cy, R0, R0 * 0.92)]
    for i in range(k):
        a, d, r = rng.uniform(0, 2 * math.pi), spread * rng.uniform(0.6, 1.0), R0 * rng.uniform(0.45, 0.65)
        lobes.append((cx + math.cos(a) * d, cy + math.sin(a) * d * 0.8, r, r * 0.9))
    return lobes

def fol_broadleaf(sp, rng, v):
    cfg = [(5.6, 3.4, 0.75, 0.0, 330), (4.6, 3.8, 0.55, 0.0, 380),
           (6.8, 2.4, 0.95, 0.0, 330), (5.4, 4.0, 0.6, 0.22, 300)][v]
    lobes = random_lobes(rng, 32, 32, 19.5, 6, 11)
    leaf_mass(sp, rng, lobes, int(cfg[4] * 1.6), cfg[0], cfg[1], cfg[2], cfg[3], rib=0.12)

def needles(sp, rng, pts, base, nl, spacing, droop, spread=(0.55, 1.05), flat=False, top_y=0, h=30):
    pts = np.asarray(pts, float)
    for i in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        seg, ang = math.hypot(x1 - x0, y1 - y0), math.atan2(y1 - y0, x1 - x0)
        for k in range(max(1, int(seg / spacing))):
            t = (k + rng.random()) * spacing / max(seg, 1e-6)
            px, py = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            for side in (-1, 1):
                a = ang + side * rng.uniform(*spread)
                dx, dy = math.cos(a), math.sin(a) * (0.45 if flat else 1.0)
                dy += droop
                nrm = math.hypot(dx, dy)
                dx, dy = dx / nrm, dy / nrm
                L, vy = rng.uniform(*nl), (py - top_y) / h
                l = base + 0.28 * (-dy) - 0.25 * vy + rng.normal(0, 0.08)
                sp.capsule(px, py, px + dx * L, py + dy * L, 0.5, 0.36, gray(l))

def fol_conifer(sp, rng, v):
    """A whorl of drooping limbs seen from slightly above: wide, layered, jagged."""
    spruce, pine, fir = v in (0, 1), v == 2, v == 3
    nl = (4.5, 6.5) if pine else (2.0, 3.2) if fir else (2.3, 3.6)
    cx, cy = 32, 22 + (3 if fir else 0)
    nlimb, limbs = 20 if v == 1 else 16, []
    for k in range(nlimb):
        th = 2 * math.pi * (k + rng.uniform(-0.3, 0.3)) / nlimb
        limbs.append((math.sin(th), th, rng.uniform(21, 27) * (0.8 + 0.2 * abs(math.cos(th)))))
    limbs.sort()
    droop0 = 15 if spruce else 7 if fir else 11
    for depth, th, L in limbs:
        base, ex = 0.42 + 0.3 * depth, cx + math.cos(th) * L
        ey = cy + math.sin(th) * L * 0.42 + droop0 * rng.uniform(0.7, 1.2) * (0.6 + 0.4 * abs(math.cos(th)))
        mid = (cx + math.cos(th) * L * 0.55, cy + math.sin(th) * L * 0.18 - 1.5)
        limb = bez((cx, cy), mid, (ex, ey), 9)
        sp.stroke(limb, 1.0, 0.45, gray(0.02))
        needles(sp, rng, limb[2:], base, nl, 1.6 if pine else 0.75, 0.3 if spruce else 0.12,
                flat=fir, top_y=cy - 10, h=36)
        for k in range(3, 9, 1 if v == 1 else 2):
            sx, sy = limb[k]
            tx, ty = limb[k + 1] - limb[k]
            sgn = rng.choice([-1, 1])
            a, Ls = math.atan2(ty, tx) + sgn * rng.uniform(0.5, 1.0), rng.uniform(4, 8) * (1.15 - k / 12)
            dy = abs(math.sin(a)) * (1.0 if spruce else 0.5) + (0.6 if spruce else 0.15)
            end = (sx + math.cos(a) * Ls, sy + dy * Ls * 0.8)
            shoot = bez((sx, sy), ((sx + end[0]) / 2, (sy + end[1]) / 2 - 0.5), end, 4)
            sp.stroke(shoot, 0.55, 0.35, gray(0.04))
            if pine:
                for j in range(14):
                    b, L2 = -math.pi / 2 + rng.uniform(-1.5, 1.5), rng.uniform(*nl)
                    l = base + 0.3 * (-math.sin(b)) + rng.normal(0, 0.08)
                    sp.capsule(end[0], end[1], end[0] + math.cos(b) * L2, end[1] + math.sin(b) * L2 * 0.8,
                               0.5, 0.35, gray(l))
            else:
                needles(sp, rng, shoot, base + 0.04, nl, 0.75, 0.35 if spruce else 0.12, flat=fir, top_y=cy - 10, h=36)
        if pine:
            for j in range(18):
                b, L2 = -math.pi / 2 + rng.uniform(-1.6, 1.6), rng.uniform(*nl)
                sp.capsule(ex, ey, ex + math.cos(b) * L2, ey + math.sin(b) * L2 * 0.8, 0.5, 0.35,
                           gray(base + 0.3 * (-math.sin(b)) + rng.normal(0, 0.08)))

def fol_bush(sp, rng, v):
    lobes = random_lobes(rng, 32, 34, 18, 6, 12)
    leaf_mass(sp, rng, lobes, 1000 if v == 0 else 880, 3.6 if v == 0 else 4.2, 2.4 if v == 0 else 2.0, 0.6, 0.0)

def fol_grass(sp, rng, v):
    n, h0, h1, r0, spread = [(52, 10, 30, 0.85, 7), (34, 26, 58, 0.65, 6),
                             (40, 14, 40, 0.8, 8), (30, 14, 30, 1.25, 6)][v]
    blades = []
    for _ in range(n):
        bx = 32 + rng.normal(0, spread)
        h = rng.uniform(h0, h1) * (1 - 0.45 * min(abs(bx - 32) / 22, 1))
        lean = rng.normal(0, 0.28) + (bx - 32) * (0.05 if v == 3 else 0.025)
        blades.append((rng.random(), bx, h, lean))
    blades.sort()
    for depth, bx, h, lean in blades:
        tip = (bx + lean * h * 0.7, 64 - h + (abs(lean) * h * 0.35 if v == 3 else 0))
        ctrl = (bx + lean * h * 0.15, 64 - h * (0.75 if v == 3 else 0.55))
        pts = bez((bx, 64.6), ctrl, tip, 9)
        sp.stroke(pts, r0 * rng.uniform(0.8, 1.15), 0.3, gray(0.08 + 0.3 * depth),
                  gray(0.5 + 0.45 * depth + rng.normal(0, 0.06)))
    if v == 2:
        for _ in range(6):
            bx, h, lean = 32 + rng.normal(0, 6), rng.uniform(40, 56), rng.normal(0, 0.15)
            tip = (bx + lean * h, 64 - h)
            pts = bez((bx, 64.6), (bx, 64 - h * 0.5), tip, 8)
            sp.stroke(pts, 0.45, 0.4, gray(0.3), gray(0.75))
            for j in range(8):
                t = j / 8
                y, side = tip[1] + t * 11, 1 if j % 2 else -1
                sp.leaf(tip[0] + lean * t * 11, y, math.pi / 2 + side * 1.1 - lean, 2.6, 1.1,
                        gray(0.75 + 0.2 * rng.random() - 0.3 * t), 0.8)

def fol_fern(sp, rng):
    fronds = []
    for i in range(7):
        a = -math.pi / 2 + (i - 3) * 0.42 + rng.normal(0, 0.08)
        fronds.append((abs(i - 3) * -1 + rng.random(), a, rng.uniform(26, 36)))
    fronds.sort()
    for depth, a, L in fronds:
        dl = 0.55 + 0.12 * depth
        x0, y0 = 32, 64.5
        tipx, tipy = x0 + math.cos(a) * L * 1.1, y0 + math.sin(a) * L * 0.75 + abs(math.cos(a)) * 12
        ctrl = (x0 + math.cos(a) * L * 0.5, y0 + math.sin(a) * L * 1.0)
        rach = bez((x0, y0), ctrl, (tipx, tipy), 20)
        sp.stroke(rach, 0.6, 0.35, gray(0.2), gray(0.4))
        for k in range(3, 20):
            t = k / 20
            px, py = rach[k]
            tx, ty = rach[k + 1] - rach[k]
            ang, pl = math.atan2(ty, tx), 7.5 * math.sin(math.pi * (0.12 + 0.88 * t)) ** 0.8 * (1 - 0.35 * t)
            for side in (-1, 1):
                b = ang + side * 1.0
                b += 0.35 * (math.pi / 2 - b) * 0.3
                l = dl + 0.2 * (-math.sin(b)) - 0.15 * side * math.cos(a) + rng.normal(0, 0.05)
                sp.leaf(px, py, b, pl, 1.9, gray(l), 0.75, 0.3)

def fol_reeds(sp, rng):
    items = []
    for _ in range(26):
        items.append((rng.random(), 32 + rng.normal(0, 7), rng.uniform(30, 60), rng.normal(0, 0.12)))
    items.sort()
    for depth, bx, h, lean in items:
        tip = (bx + lean * h, 64 - h + abs(lean) * 20)
        pts = bez((bx, 64.6), (bx + lean * h * 0.2, 64 - h * 0.7), tip, 10)
        sp.stroke(pts, 0.75, 0.3, gray(0.1 + 0.3 * depth), gray(0.5 + 0.45 * depth))
    for _ in range(4):
        bx, h, lean = 32 + rng.normal(0, 5), rng.uniform(46, 60), rng.normal(0, 0.06)
        tip = (bx + lean * h, 64 - h)
        sp.stroke(bez((bx, 64.6), (bx, 64 - h / 2), tip, 8), 0.45, 0.4, gray(0.3), gray(0.6))
        sp.blob(tip[0], tip[1] + 3.5, 1.1, 4.0, gray(0.25), shade=True, amb=0.3)

GREEN = C(0.30, 0.46, 0.17)

def stem(sp, rng, x, h, lean, col=GREEN, r=0.55):
    top = (x + lean * h, 64 - h)
    sp.stroke(bez((x, 64.6), (x + lean * h * 0.15, 64 - h * 0.55), top, 8), r, r * 0.75, col * 0.7, col)
    return top

def basal(sp, rng, n, L, W, col, serr=0.0, shape=0.7, cx=32, spread=1.25):
    for i in range(n):
        a = -math.pi / 2 + rng.uniform(-spread, spread)
        sp.leaf(cx + rng.normal(0, 3), 64, a, rng.uniform(*L), W, col * rng.uniform(0.75, 1.1),
                shape, serr, rib=0.15, curl=0.12 * np.sign(math.cos(a)))

def radial_petals(sp, cx, cy, n, L, W, col, sq, rng, phase=0.0, droop=0.0, serr=0.0, back=None):
    pet = []
    for k in range(n):
        a = phase + 2 * math.pi * k / n + rng.normal(0, 0.06)
        ex, ey = math.cos(a), math.sin(a) * sq + droop
        pet.append((ey, math.atan2(ey, ex), L * math.hypot(ex, ey)))
    for ey, a, l in sorted(pet):
        c = col * (0.78 if ey < 0 else 1.0) if back is None else (back if ey < 0 else col)
        sp.leaf(cx, cy, a, max(l, 1.0), W, c * rng.uniform(0.9, 1.05), 0.6, serr)

def fol_flower(sp, rng, kind):
    G = GREEN * rng.uniform(0.9, 1.1)
    if kind == 0:     # poppy
        basal(sp, rng, 6, (8, 13), 3.4, C(0.36, 0.5, 0.26), serr=0.35)
        for x, h, lean in ((24, 40, -0.1), (35, 46, 0.06), (42, 32, 0.15)):
            tx, ty = stem(sp, rng, x, h, lean, C(0.38, 0.52, 0.25), 0.5)
            s = rng.uniform(4.0, 4.8)
            sp.blob(tx - s * 0.45, ty - s * 0.25, s * 0.7, s * 0.55, C(0.66, 0.07, 0.05), shade=True, amb=0.55)
            sp.blob(tx + s * 0.45, ty - s * 0.3, s * 0.7, s * 0.55, C(0.70, 0.08, 0.05), shade=True, amb=0.55)
            sp.blob(tx, ty - s * 0.35, s * 0.45, s * 0.22, C(0.1, 0.08, 0.07))
            sp.blob(tx, ty - s * 0.42, s * 0.18, s * 0.12, C(0.45, 0.5, 0.3))
            sp.blob(tx - s * 0.35, ty + s * 0.05, s * 0.65, s * 0.55, C(0.9, 0.14, 0.07), rng, 0.15, True, 0.55)
            sp.blob(tx + s * 0.4, ty + s * 0.1, s * 0.6, s * 0.5, C(0.85, 0.12, 0.06), rng, 0.15, True, 0.55)
        tx, ty = stem(sp, rng, 30, 30, -0.25, C(0.38, 0.52, 0.25), 0.45)
        sp.blob(tx - 1.2, ty + 1.5, 1.6, 2.3, C(0.42, 0.55, 0.3), shade=True, ang=0.5)
    elif kind == 1:   # buttercup
        basal(sp, rng, 7, (5, 8), 4.2, C(0.25, 0.45, 0.14), shape=0.5)
        for i in range(6):
            x = 32 + rng.normal(0, 7)
            tx, ty = stem(sp, rng, x, rng.uniform(16, 36), rng.normal(0, 0.15), G, 0.45)
            s = rng.uniform(2.6, 3.2)
            for k in range(5):
                a = -math.pi / 2 + 2 * math.pi * k / 5
                sp.blob(tx + math.cos(a) * s * 0.8, ty + math.sin(a) * s * 0.45, s * 0.75, s * 0.6,
                        C(1.0, 0.80, 0.06), shade=True, amb=0.6, spec=0.5)
            sp.blob(tx, ty, s * 0.35, s * 0.3, C(0.75, 0.62, 0.1))
    elif kind == 2:   # trumpet gentian
        basal(sp, rng, 10, (5, 9), 3.4, C(0.2, 0.42, 0.14), spread=1.5)
        for x, h, w, tl in ((24, 24, 9, -0.12), (35, 30, 10, 0.05), (44, 20, 8, 0.2), (30, 16, 7, -0.05)):
            sp.stroke(bez((x, 64.6), (x, 64 - h * 0.25), (x + tl * h * 0.3, 64 - h * 0.35), 3), 0.8, 0.8, G * 0.8)
            sl, X, Y = sp.grid(x - w, 64 - h - 4, x + w + 4, 64)
            bx, by = x + tl * h * 0.3, 64 - h * 0.35
            Xr, u = X - bx - tl * (by - Y), (by - Y) / (h * 0.65)
            hw = w * 0.5 * (0.2 + 0.8 * np.clip(u, 0, 1) ** 2.2)
            m, nx = (u >= 0) & (u <= 1) & (np.abs(Xr) <= hw), np.clip(Xr / np.maximum(hw, 0.1), -1, 1)
            f = 0.45 + 0.7 * np.clip(nx * LX + np.sqrt(1 - nx * nx) * LZ, 0, 1)
            f = f * (1 - 0.25 * np.exp(-((np.abs(Xr) - hw * 0.35) / 0.5) ** 2) * (u > 0.2))
            sp.put(sl, m, colmul(C(0.12, 0.22, 0.8), f))
            mx, my = bx + tl * h * 0.65, 64 - h
            for k in range(5):
                a2 = -math.pi / 2 + (k - 2) * 0.62 + tl
                sp.leaf(mx + (k - 2) * w * 0.17, my + 1.2, a2, 3.0, 2.8, C(0.2, 0.32, 0.9) * (1.1 - 0.1 * abs(k - 2)), 0.6)
            sp.blob(mx, my + 1.0, w * 0.38, 1.2, C(0.06, 0.08, 0.32))
            sp.blob(mx, my + 1.4, w * 0.15, 0.6, C(0.3, 0.45, 0.3))
    elif kind == 3:   # edelweiss
        basal(sp, rng, 10, (7, 12), 2.2, C(0.55, 0.6, 0.5), spread=1.4)
        for x, h, lean in ((24, 26, -0.18), (36, 34, 0.06), (43, 18, 0.25)):
            tx, ty = stem(sp, rng, x, h, lean, C(0.55, 0.6, 0.5), 0.6)
            radial_petals(sp, tx, ty, 9, rng.uniform(6.0, 7.2), 2.8, C(0.96, 0.96, 0.92), 0.72, rng,
                          back=C(0.8, 0.82, 0.78))
            for k in range(7):
                a = 2 * math.pi * k / 7
                sp.blob(tx + math.cos(a) * 1.5, ty + math.sin(a) * 1.0, 0.95, 0.85, C(0.9, 0.85, 0.58), shade=True)
    elif kind == 4:   # lavender
        basal(sp, rng, 10, (7, 12), 1.4, C(0.45, 0.55, 0.42), spread=1.0)
        for i in range(9):
            x, h = 32 + (i - 4) * 1.6 + rng.normal(0, 0.8), rng.uniform(36, 52)
            lean = (i - 4) * 0.05 + rng.normal(0, 0.03)
            tx, ty = stem(sp, rng, x, h, lean, C(0.42, 0.52, 0.38), 0.4)
            for j in range(9):
                t = j / 9
                px, py = tx - lean * t * 14, ty + t * 14
                r = 1.25 * (1 - 0.3 * (1 - t)) if j else 0.9
                c = C(0.55, 0.38, 0.78) * (1.05 - 0.4 * t) * rng.uniform(0.9, 1.1)
                if j < 7 or j % 2 == 0:
                    sp.blob(px, py, r, r * 1.15, c, shade=True, amb=0.5)
    elif kind == 5:   # daisy
        basal(sp, rng, 8, (4, 7), 2.6, C(0.28, 0.48, 0.16), shape=0.5)
        for i in range(5):
            x = 32 + rng.normal(0, 7)
            tx, ty = stem(sp, rng, x, rng.uniform(16, 36), rng.normal(0, 0.12), G, 0.5)
            radial_petals(sp, tx, ty, 16, rng.uniform(4.6, 5.4), 1.6, C(0.98, 0.98, 0.95), 0.55, rng)
            sp.blob(tx, ty - 0.3, 1.9, 1.5, C(1.0, 0.76, 0.1), shade=True, amb=0.5)
    elif kind == 6:   # alpine rose (rhododendron)
        lobes = [(32, 50, 17, 11), (21, 55, 9, 8), (43, 54, 10, 8)]
        leaf_mass(sp, rng, lobes, 360, 4.6, 1.9, 0.7, col=C(0.2, 0.36, 0.13), back=0.0)
        for _ in range(7):
            a = rng.uniform(-math.pi * 0.95, -math.pi * 0.05)
            cx, cy = 32 + math.cos(a) * 15, 50 + math.sin(a) * 10
            for k in range(6):
                b = -math.pi / 2 + rng.uniform(-1.7, 1.7)
                sp.leaf(cx, cy, b, 2.9, 2.1, C(0.92, 0.32, 0.5) * rng.uniform(0.7, 1.05), 0.6)
    else:             # arnica
        basal(sp, rng, 5, (6, 10), 3.6, C(0.32, 0.5, 0.2), shape=0.6, spread=1.4)
        for x, h, lean in ((26, 34, -0.1), (37, 42, 0.07), (44, 26, 0.18)):
            tx, ty = stem(sp, rng, x, h, lean, C(0.35, 0.5, 0.22), 0.5)
            my, mx = 64 - h * 0.45, x + lean * h * 0.4
            sp.leaf(mx, my, -0.4, 4.5, 2.0, C(0.32, 0.5, 0.2))
            sp.leaf(mx, my, math.pi + 0.4, 4.5, 2.0, C(0.28, 0.45, 0.18))
            radial_petals(sp, tx, ty, 12, rng.uniform(5.2, 6.0), 2.0, C(1.0, 0.62, 0.1), 0.6, rng, droop=0.25, serr=0.3)
            sp.blob(tx, ty - 0.2, 2.2, 1.6, C(0.85, 0.48, 0.08), shade=True, amb=0.45)

def mushroom(sp, rng, x, h, rx, ry, cap, stemc, spots=0):
    sp.blob(x, 64 - h * 0.5, max(rx * 0.25, 0.9), h * 0.5 + 0.6, stemc, shade=True, amb=0.55)
    sp.blob(x, 64 - h + ry * 0.2, rx * 0.92, ry * 0.3, C(0.78, 0.7, 0.58), shade=True)
    sp.blob(x, 64 - h + ry * 0.25, rx, ry, cap, rng, 0.05, True, 0.4, 0.25, 0.04, clip_y=64 - h + ry * 0.3)
    for _ in range(spots):
        a, r = rng.uniform(-math.pi * 0.95, -math.pi * 0.05), rng.uniform(0.25, 0.85)
        sp.blob(x + math.cos(a) * r * rx * 0.95, 64 - h + ry * 0.25 + math.sin(a) * r * ry * 0.9,
                0.65, 0.5, C(0.98, 0.95, 0.88), shade=True, amb=0.7)

def fol_misc(sp, rng, kind):
    if kind == 24:
        for x in range(8):
            sp.leaf(16 + x * 4.5 + rng.normal(0, 1), 64, -math.pi / 2 + rng.normal(0, 0.4),
                    rng.uniform(5, 10), 1.2, GREEN * rng.uniform(0.6, 1.0), 0.8)
        mushroom(sp, rng, 26, 22, 10, 5.5, C(0.85, 0.12, 0.06), C(0.94, 0.92, 0.85), 14)
        mushroom(sp, rng, 41, 14, 6, 5, C(0.8, 0.1, 0.05), C(0.92, 0.9, 0.82), 7)
        mushroom(sp, rng, 34, 7, 3.5, 3, C(0.75, 0.15, 0.06), C(0.9, 0.88, 0.8), 3)
    elif kind == 25:
        for x, h, rx in ((22, 16, 5), (29, 22, 6.5), (36, 18, 5.5), (43, 12, 4.5), (33, 9, 3.5)):
            x += rng.normal(0, 0.8)
            mushroom(sp, rng, x, h, rx, rx * 0.65, C(0.58, 0.38, 0.2) * rng.uniform(0.9, 1.1), C(0.85, 0.78, 0.62))
    elif kind == 26:
        stones = [(32, 56, 9, 6.5), (21, 58, 7, 5.5), (43, 58, 7.5, 5), (27, 49, 6, 4.5),
                  (38, 48, 6.5, 4.8), (32, 42, 5, 4), (14, 60, 4.5, 3.5), (50, 60, 4.5, 3.6),
                  (24, 61, 4, 3), (40, 62, 4.5, 2.6)]
        stones.sort(key=lambda s: s[1])
        for x, y, rx, ry in stones:
            base = C(0.58, 0.57, 0.55) * rng.uniform(0.8, 1.1) + rng.normal(0, 0.03, 3)
            sp.blob(x, min(y, 64 - ry), rx, ry, base, rng, 0.18, True, 0.3, 0.1, 0.08, rng.uniform(-0.4, 0.4))
            for _ in range(3):
                sp.blob(x + rng.normal(0, rx * 0.35), y - ry * rng.uniform(0.2, 0.6), 0.9, 0.6, C(0.72, 0.74, 0.5))
    elif kind == 27:
        pts = [(5, 56), (18, 53), (31, 55), (45, 50), (59, 49)]
        pts = [(x, y + rng.normal(0, 0.6)) for x, y in pts]
        for (x0, y0), (x1, y1) in ((pts[1], (14, 42)), (pts[2], (27, 44)), (pts[3], (52, 40)), ((38, 52), (44, 60))):
            sp.stroke(bez((x0, y0), ((x0 + x1) / 2, (y0 + y1) / 2 - 1), (x1, y1), 5), 0.9, 0.45, C(0.3, 0.24, 0.18))
            sp.stroke(bez((x0, y0 - 0.3), ((x0 + x1) / 2 - 0.3, (y0 + y1) / 2 - 1.3), (x1 - 0.3, y1 - 0.3), 5),
                      0.45, 0.25, C(0.55, 0.47, 0.38))
        sp.stroke(pts, 2.4, 1.5, C(0.32, 0.25, 0.19))
        sp.stroke([(x - 0.4, y - 0.9) for x, y in pts], 1.4, 0.8, C(0.5, 0.42, 0.33))
        sp.stroke([(x - 0.6, y - 1.5) for x, y in pts[1:4]], 0.5, 0.4, C(0.62, 0.55, 0.45))
        sp.blob(59.5, 49, 1.4, 1.6, C(0.75, 0.64, 0.45), shade=True)
        for _ in range(5):
            i, t = rng.integers(0, 4), rng.random()
            x, y = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t
            sp.blob(x, y - 0.8, 1.1, 0.7, C(0.6, 0.68, 0.48))
    elif kind == 28:
        lobes = random_lobes(rng, 32, 33, 14.5, 5, 10)
        leaf_mass(sp, rng, lobes, 90, 4.5, 2.6, col=C(0.42, 0.62, 0.22), back=0.0)
        area, fl = np.array([l[2] * l[3] for l in lobes]), []
        for _ in range(150):
            x0, y0, rx, ry = lobes[rng.choice(len(lobes), p=area / area.sum())]
            d = rng.normal(size=3)
            d /= np.linalg.norm(d)
            r = rng.random() ** 0.4
            fl.append((d[2] * r, x0 + d[0] * r * rx, y0 + d[1] * r * ry, max(float(d @ LIGHT), 0) * (0.6 + 0.4 * r)))
        for _, x, y, l in sorted(fl):
            f, ph = 0.62 + 0.4 * l, rng.uniform(0, 6.3)
            for k in range(5):
                a = ph + 2 * math.pi * k / 5
                sp.blob(x + math.cos(a) * 1.2, y + math.sin(a) * 1.0, 1.15, 1.0, C(1.0, 0.9, 0.94) * f)
            sp.blob(x, y, 0.55, 0.5, C(0.88, 0.42, 0.55) * f)
    elif kind == 29:
        parts = [(32, 38, 15, 12), (22, 44, 10, 8), (43, 43, 11, 9), (30, 29, 8, 6.5), (38, 47, 9, 7)]
        for x, y, rx, ry in parts:
            sp.blob(x, y, rx, ry, C(0.5, 0.56, 0.68), rng, 0.12, True, 0.0, 0.0, 0.02, col2=C(1.0, 1.0, 1.0))
        for _ in range(12):
            sp.blob(rng.uniform(18, 46), rng.uniform(28, 46), 0.4, 0.4, C(1.0, 1.0, 1.0))

def fol_birch(sp, rng):
    for (x, y, rx, ry) in [(32, 30, 12, 10), (24, 38, 8, 7), (41, 38, 8, 7)]:
        sp.blob(x, y, rx, ry, gray(0.05), rng, 0.3)
    strands = []
    for i in range(16):
        a = rng.uniform(-math.pi * 0.95, -math.pi * 0.05)
        sx, sy = 32 + math.cos(a) * rng.uniform(4, 12), 30 + math.sin(a) * rng.uniform(3, 10)
        ex = sx + math.cos(a) * rng.uniform(6, 14)
        strands.append((rng.random(), (sx, sy), (ex, sy - rng.uniform(0, 4)), (ex + rng.normal(0, 2), sy + rng.uniform(14, 26))))
    strands.sort()
    for depth, p0, p1, p2 in strands:
        pts = bez(p0, p1, p2, 12)
        sp.stroke(pts, 0.4, 0.35, gray(0.1))
        for k in range(1, 13):
            px, py = pts[k]
            for side in (-1, 1):
                a = math.pi / 2 + side * rng.uniform(0.5, 1.2) + rng.normal(0, 0.2)
                l = 0.25 + 0.45 * depth + 0.3 * (1 - (py - 15) / 45) + rng.normal(0, 0.08)
                sp.leaf(px, py, a, rng.uniform(3.0, 4.0), 2.6, gray(l), 0.55, 0.15)

def fol_palm(sp, rng):
    fr = []
    for i in range(9):
        a = -math.pi / 2 + (i - 4) * 0.36 + rng.normal(0, 0.05)
        fr.append((-abs(i - 4) + rng.random(), a))
    fr.sort()
    for depth, a in fr:
        L = rng.uniform(24, 30)
        tip = (32 + math.cos(a) * L * 1.05, 38 + math.sin(a) * L * 0.5 + abs(math.cos(a)) * 16)
        ctrl = (32 + math.cos(a) * L * 0.55, 38 + math.sin(a) * L * 0.95)
        rach = bez((32, 40), ctrl, tip, 18)
        sp.stroke(rach, 0.7, 0.3, gray(0.2))
        dl = 0.55 + 0.1 * depth
        for k in range(2, 18):
            px, py = rach[k]
            t = k / 18
            for side in (-1, 1):
                b = math.pi / 2 + side * (0.5 + 0.4 * t) * np.sign(math.cos(a) + 1e-3) * -1
                b = 0.6 * b + 0.4 * (math.atan2(*(rach[k + 1] - rach[k])[::-1]) + side * 0.9)
                l = dl + 0.25 * (1 - t) * 0.5 - 0.15 * side + rng.normal(0, 0.06)
                sp.leaf(px, py, b, 7.5 * (1 - 0.5 * t), 1.5, gray(l), 0.9)
    sp.blob(32, 40, 3.5, 3.0, gray(0.25), shade=True)

def fol_butterfly(sp, rng, species, frame):
    sl, X, Y = sp.grid(0, 0, 64, 64)
    cx, cy = 32, 32
    sx = 1.0 if frame == 0 else 0.4
    U, W = np.abs(X - cx) / sx, Y - cy

    def ell(u0, w0, ru, rw, ang):
        ca, sa = math.cos(ang), math.sin(ang)
        a, b = ((U - u0) * ca + (W - w0) * sa) / ru, (-(U - u0) * sa + (W - w0) * ca) / rw
        return a * a + b * b
    qf, qh = ell(5.8, -3.2, 6.6, 4.2, -0.55), ell(4.6, 3.2, 4.9, 3.9, 0.55)
    q = np.minimum(qf, qh)
    m, th = q <= 1, np.arctan2(W, U + 0.5)
    if species == 0:
        base = C(0.95, 0.52, 0.08) if frame == 0 else C(0.9, 0.62, 0.3)
        col = base[None, None, :] * (0.85 + 0.25 * (1 - q))[..., None]
        dark = (q > 0.68) | (np.abs(np.sin(th * 4.5)) < 0.13) & (q > 0.15)
        col = np.where(dark[..., None], C(0.1, 0.07, 0.05), col)
        dots = (q > 0.74) & (q < 0.92) & (np.sin(th * 14) > 0.55)
        col = np.where(dots[..., None], C(0.98, 0.95, 0.85), col)
    else:
        base = C(0.6, 0.76, 0.96) if frame == 0 else C(0.75, 0.78, 0.82)
        col = base[None, None, :] * (0.8 + 0.3 * (1 - q))[..., None]
        col = np.where((q > 0.8)[..., None], C(0.28, 0.3, 0.38), col)
        col = np.where((q > 0.93)[..., None], C(0.95, 0.96, 1.0), col)
        sp_ = (np.abs(qh - 0.45) < 0.08) & (np.sin(th * 9) > 0.6)
        col = np.where(sp_[..., None], C(0.2, 0.22, 0.3), col)
    sp.put(sl, m, col)
    bc = C(0.12, 0.1, 0.08) if species == 0 else C(0.2, 0.22, 0.28)
    sp.blob(cx, cy + 0.5, 1.1, 6.0, bc, shade=True, amb=0.6)
    sp.blob(cx, cy - 6.0, 1.2, 1.1, bc, shade=True, amb=0.6)
    for side in (-1, 1):
        sp.stroke([(cx + side * 0.4, cy - 6.5), (cx + side * 1.8, cy - 9.5), (cx + side * 3.2, cy - 11.5)], 0.32, 0.3, bc)
        sp.blob(cx + side * 3.3, cy - 11.7, 0.6, 0.6, bc)

def soft_tile(kind):
    xx, yy = grid64()
    x, y = xx - 32, yy - 32
    r, rng = np.hypot(x, y), R(700, kind)
    if kind == 36:
        a = 0.9 * np.exp(-(r / 9) ** 2) + 0.1 * np.exp(-(r / 3) ** 2)
        rgb = C(0.82, 1.0, 0.4) * np.ones((64, 64, 1))
        rgb = rgb + (1 - rgb) * np.exp(-(r / 3.5) ** 2)[..., None]
    elif kind == 40:
        a = np.exp(-(x / 0.75) ** 2) * smooth(6, 20, yy) * smooth(58, 40, yy) * 0.8
        rgb = C(0.78, 0.84, 0.92) * np.ones((64, 64, 1))
    elif kind == 41:
        arms = sum(np.exp(-((x * math.sin(t) - y * math.cos(t)) / 0.8) ** 2) for t in (0, 1.047, 2.094))
        a = np.clip(np.exp(-(r / 3.2) ** 2) + 0.45 * arms * smooth(9, 3, r), 0, 1)
        rgb = C(1.0, 1.0, 1.0) * np.ones((64, 64, 1))
    elif kind == 42:
        star = np.exp(-np.abs(x) / 1.2 - (y / 14) ** 2) + np.exp(-np.abs(y) / 1.2 - (x / 14) ** 2)
        a = np.clip(star * 0.9 + np.exp(-(r / 5) ** 2) * 0.6, 0, 1)
        rgb = C(1.0, 0.86, 0.48) * np.ones((64, 64, 1))
        rgb = rgb + (1 - rgb) * np.exp(-(r / 3) ** 2)[..., None]
    elif kind == 43:
        n = snoise(64, 64, rng, p=1.3, fmin=2)
        a = np.clip((1 - r / 27) * (0.9 + 0.35 * n), 0, 1) ** 1.4 * 0.85
        rgb = C(0.62, 0.62, 0.64) * (0.85 + 0.15 * smooth(20, -20, y))[..., None]
    elif kind == 44:
        n = snoise(64, 64, rng, p=1.2, fmin=2)
        lob = sum(np.exp(-(((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2))
                  for cx, cy, rx, ry in ((32, 34, 15, 11), (21, 37, 10, 8), (43, 36, 11, 8), (30, 26, 9, 8), (38, 29, 8, 7)))
        a = np.clip(1.7 * lob * (1 + 0.25 * n) - 0.3, 0, 1)
        rgb = lerp(C(0.8, 0.83, 0.9), C(1.0, 1.0, 1.0), smooth(42, 26, yy))
    elif kind == 45:
        a = np.clip(np.exp(-(r / 12) ** 2) * 0.7 + np.exp(-(r / 4.5) ** 2) * 0.3 + 0.25 * np.exp(-r / 20), 0, 1)
        a, rgb = a * smooth(31.5, 26, r), C(1.0, 0.94, 0.78) * np.ones((64, 64, 1))
    else:
        a, rgb = smooth(30, 0, r) ** 2, C(1.0, 1.0, 1.0) * np.ones((64, 64, 1))
    return rgba(np.broadcast_to(rgb, (64, 64, 3)), a)

def tile_at(atlas, i, t):
    atlas[(i // 8) * 64:(i // 8) * 64 + 64, (i % 8) * 64:(i % 8) * 64 + 64] = t

FOLIAGE = [(range(0, 4), fol_broadleaf, 0), (range(4, 8), fol_conifer, 4), (range(8, 10), fol_bush, 8),
           (range(10, 14), fol_grass, 10), (range(16, 24), fol_flower, 16), (range(24, 30), fol_misc, 0)]

def make_foliage():
    atlas = np.zeros((512, 512, 4))
    for i in range(36):
        sp, rng = Sprite(100 + i), R(100, i)
        if i >= 32:
            fol_butterfly(sp, rng, (i - 32) // 2, (i - 32) % 2)
        elif i in (14, 15, 30, 31):
            {14: fol_fern, 15: fol_reeds, 30: fol_birch, 31: fol_palm}[i](sp, rng)
        else:
            rg, fn, off = next(f for f in FOLIAGE if i in f[0])
            fn(sp, rng, i - off)
        tile_at(atlas, i, sp.resolve(i < 16 or i in (30, 31)))
    for i in (36, 40, 41, 42, 43, 44, 45, 46):
        tile_at(atlas, i, soft_tile(i))
    return atlas

# ---- Terrain detail textures (grayscale, mean ~128, tileable) --------------

def norm8(x, std):
    x = (x - x.mean()) / (x.std() + 1e-12)
    return np.clip(128 + std * x, 0, 255) / 255

def make_terrain():
    n, rng = 64, R(200)
    xx, yy = grid64()
    # meadow: soft clumps, vertical blade dashes, small dark soil gaps
    m = 0.6 * snoise(n, n, rng, 1.2) + 0.25 * snoise(n, n, rng, 0.5, 8, stretch=(3, 1))
    m -= 0.9 * smooth(1.0, 2.0, snoise(n, n, rng, 0.8, 4))
    for _ in range(900):
        x, y = rng.uniform(0, n, 2)
        a, L = -math.pi / 2 + rng.normal(0, 0.45), rng.uniform(2, 5)
        v = rng.choice([-1.0, 1.0]) * rng.uniform(0.5, 1.0)
        for t in np.linspace(0, 1, int(L * 1.5) + 1):
            m[int(y + math.sin(a) * L * t) % n, int(x + math.cos(a) * L * t) % n] += v * (1.1 - 0.5 * t)
    meadow, k = norm8(m, 21), 6
    # rock: warped strata, cracks, relief shading
    s = (yy + 8 * snoise(n, n, rng, 1.6)) * k / n
    lay, frac = (np.floor(s) % k).astype(int), s - np.floor(s)
    h = 0.5 * rng.uniform(-1, 1, k)[lay] - 0.8 * np.exp(-(frac / 0.08) ** 2) + 0.45 * snoise(n, n, rng, 1.0, 3)
    f1, f2, *_ = voronoi(n, jitter_pts(rng, 3, 8))
    crack = np.exp(-((f2 - f1) / 0.9) ** 2) * (snoise(n, n, rng, 1.0, 2) > -0.4)
    rock = h + 1.4 * (relief(h * 2.5) - 0.5) - 1.3 * crack + 0.25 * snoise(n, n, rng, 0.2, 12)
    rock, pts = norm8(rock, 30), rng.uniform(0, n, (75, 2))
    # dirt / gravel path: shaded pebbles with contact shadows on soil
    f1, f2, ids, dx, dy = voronoi(n, pts)
    th = np.arctan2(dy, dx)
    rad = rng.uniform(1.3, 3.0, 75)[ids] * (1 + 0.22 * np.sin(3 * th + rng.uniform(0, 6.3, 75)[ids]))
    tone = rng.normal(0, 0.45, 75)[ids]
    inside, hz = f1 < rad, np.sqrt(np.clip(1 - (f1 / rad) ** 2, 0, 1))
    lam = np.clip(dx / rad * LX + dy / rad * LY + hz * LZ, 0, 1)
    peb = 0.3 + tone + 1.2 * (lam - 0.5)
    dirt = 0.45 * snoise(n, n, rng, 1.0) + 0.35 * snoise(n, n, rng, 0.3, 12) - 0.3
    shadow = (f1 < rad + 1.3) & ~inside & ((dx + dy) > 0)
    dirt = np.where(inside, peb, dirt - 0.7 * shadow)
    dirt, rip = norm8(dirt, 27), np.sin(2 * np.pi * (2 * xx + 5 * yy) / n + 2.5 * snoise(n, n, rng, 1.5))
    # snow: very soft drifts, wind ripples, rare sparkles
    sn = 0.7 * snoise(n, n, rng, 1.3) + 0.3 * rip + 0.15 * snoise(n, n, rng, 0.2, 16)
    sn += 2.5 * (rng.random((n, n)) > 0.993)
    snow = norm8(sn, 9)
    g = np.hstack([meadow, rock, dirt, snow])
    return rgba(np.repeat(g[..., None], 3, -1))

# ---- Materials atlas -------------------------------------------------------

def mix(a, b, t):
    return lerp(np.asarray(a, float), np.asarray(b, float), t)

def wrapd(xx, x0, n=64):
    return (xx - x0 + n / 2) % n - n / 2

def m_log_side(rng):
    xx, yy = grid64()
    th = (yy / 64 - 0.5) * np.pi * 0.94
    sh = 0.42 + 0.68 * np.clip(LY * np.sin(th) + LZ * np.cos(th) + 0.15, 0, 1)
    grain, lines = snoise(64, 64, rng, 1.1, 1, stretch=(1, 7)), snoise(64, 64, rng, 0.2, 6, stretch=(1, 9))
    w = 1 + 0.1 * grain + 0.08 * lines
    for _ in range(2):
        kx, ky = rng.uniform(0, 64), rng.uniform(18, 46)
        d = np.hypot(wrapd(xx, kx) / 3.2, (yy - ky) / 2.2)
        w = w * (1 - 0.45 * smooth(1.3, 0.6, d)) * (1 - 0.08 * np.sin(d * 5) * smooth(3.5, 1.0, d))
    for _ in range(3):
        cy, x0, L = rng.uniform(14, 50), rng.uniform(0, 64), rng.uniform(14, 34)
        dx = (xx - x0) % 64
        crack = (np.abs(yy - cy - 1.2 * np.sin(dx / 6)) < 0.55) & (dx < L)
        w = w * np.where(crack, 0.6, 1.0)
    bark = (snoise(64, 64, rng, 1.0, 3) > 1.2) & ((yy < 9) | (yy > 55))
    col = np.where(bark[..., None], C(0.36, 0.27, 0.2), C(0.68, 0.44, 0.24) * w[..., None])
    rgb = col * (sh * (0.35 + 0.65 * smooth(0, 2.0, yy)) * (0.45 + 0.55 * smooth(64, 61.5, yy)))[..., None]
    return rgba(rgb)

def m_log_end(rng):
    xx, yy = grid64()
    r, th, R0 = np.hypot(xx - 32, yy - 32), np.arctan2(yy - 32, xx - 32), 30.5
    rr = r + 1.0 * snoise(64, 64, rng, 1.4)
    rings, col = np.sin(2 * np.pi * rr / 2.4), mix(C(0.6, 0.4, 0.22), C(0.84, 0.68, 0.45), smooth(0, 24, r))
    v = 0.9 + 0.08 * rings + 0.05 * snoise(64, 64, rng, 0.3, 8)
    for a in rng.uniform(-np.pi, np.pi, 3):
        dth = np.abs((th - a + np.pi) % (2 * np.pi) - np.pi)
        v = v * np.where((dth * r < 0.6 + r * 0.02) & (r > 6) & (r < R0 - 4), 0.45, 1.0)
    v = v * (1 - 0.35 * smooth(2.5, 0, r)) * (1 + 0.08 * ((-(xx - 32) - (yy - 32)) / 40))
    rgb, bark = col * v[..., None], r > R0 - 2.6 + 0.6 * snoise(64, 64, rng, 1.0, 4)
    rgb = np.where(bark[..., None], C(0.32, 0.23, 0.16) * (0.85 + 0.15 * rings)[..., None], rgb)
    return edge_fill(rgba(rgb, (r <= R0).astype(float)))

def planks(rng, base, nb=4, joints=1, var=0.1, gamp=0.14):
    Y, X = np.mgrid[0:64, 0:64]
    bw = 64 // nb
    b, ly = Y // bw, Y % bw
    g, g2 = snoise(64, 64, rng, 1.1, 1, stretch=(1, 9)), snoise(64, 64, rng, 0.25, 8, stretch=(1, 10))
    jx = rng.integers(0, 64, (nb, max(joints, 1)))
    seg = b * 4 + sum((X >= jx[b, k]).astype(int) for k in range(joints)) if joints else b * 4
    tint, off = rng.uniform(1 - var, 1 + var, nb * 4)[seg], rng.integers(0, 64, nb * 4)[seg]
    field = g[(Y + off) % 64, X]
    v = tint * (1 + gamp * field + 0.06 * g2)
    v = v * np.where(ly == 0, 0.45, np.where(ly == 1, 1.08, np.where(ly == bw - 1, 0.85, 1.0)))
    for k in range(joints):
        j = jx[b, k]
        v = v * np.where(X == j, 0.5, np.where(X == (j + 1) % 64, 1.06, 1.0))
        nail = ((X == (j + 2) % 64) | (X == (j - 2) % 64)) & ((ly == bw // 2 - 3) | (ly == bw // 2 + 3))
        v = v * np.where(nail, 0.45, 1.0)
    return np.asarray(base) * v[..., None]

def m_shingles(rng):
    Y, X = np.mgrid[0:64, 0:64]
    rows = 4
    ids, lxs, ovs = np.zeros((rows, 64), int), np.zeros((rows, 64), int), np.zeros((rows, 64), int)
    nid = 0
    for r in range(rows):
        start, x = int(rng.integers(0, 64)), 0
        while x < 64:
            w = int(rng.integers(7, 15))
            w = 64 - x if 64 - x < w + 5 else w
            for k in range(w):
                c = (start + x + k) % 64
                ids[r, c], lxs[r, c], ovs[r, c] = nid, k, int(rng.integers(0, 2)) if k == 0 else ovs[r, (c - 1) % 64]
            nid, x = nid + 1, x + w
    b, ly = Y // 16, Y % 16
    prev = (b - 1) % rows
    own = ly < ovs[prev, X]
    row, ly = np.where(own, prev, b), np.where(own, ly + 16, ly)
    sid, lx = ids[row, X], lxs[row, X]
    ovl, tint, warm = ovs[row, X], rng.uniform(0.8, 1.15, nid)[sid], rng.uniform(0, 1, nid)[sid]
    g = snoise(64, 64, rng, 0.6, 3, stretch=(6, 1))
    col = mix(C(0.42, 0.36, 0.3), C(0.56, 0.4, 0.26), warm) * (tint * (1 + 0.12 * g))[..., None]
    f = (0.5 + 0.5 * smooth(0, 7, ly)) * np.where(ly >= 15 + ovl, 1.2, 1.0)
    f = f * np.where(lx == 0, 0.4, np.where(lx == 1, 0.85, 1.0))
    split = (rng.random(nid) < 0.25)[sid] & (lx == rng.integers(3, 8, nid)[sid]) & (ly > 5)
    f = f * np.where(split, 0.55, 1.0)
    return rgba(col * f[..., None])

def m_stones(rng):
    pts = jitter_pts(rng, 4, 5)
    f1, f2, ids, dx, dy = voronoi(64, pts)
    e = (f2 - f1) / 2 + 0.4 * snoise(64, 64, rng, 1.0, 4)
    h = smooth(1.0, 5.0, e) ** 0.7 + 0.08 * snoise(64, 64, rng, 0.8, 4)
    lam = relief(h * 2.6)
    pal = np.array([[0.56, 0.54, 0.5], [0.6, 0.53, 0.43], [0.5, 0.52, 0.55], [0.44, 0.42, 0.4], [0.62, 0.6, 0.55]])
    sc = pal[rng.integers(0, 5, len(pts))][ids] * rng.uniform(0.85, 1.1, len(pts))[ids][..., None]
    tex = 1 + 0.08 * snoise(64, 64, rng, 0.3, 10)
    stone = sc * ((0.35 + 0.85 * lam) * tex)[..., None]
    mortar = C(0.6, 0.57, 0.5) * ((0.7 + 0.08 * snoise(64, 64, rng, 0.2, 10)) * (0.7 + 0.3 * smooth(0, 1.0, e)))[..., None]
    return rgba(np.where((e < 1.5)[..., None], mortar, stone))

def m_bark_gray(rng):
    xx, yy = grid64()
    warp, n2 = snoise(64, 64, rng, 1.8, 1, stretch=(5, 1)), snoise(64, 64, rng, 1.8, 1, stretch=(4, 1))
    r = np.abs(np.sin(np.pi * (xx + 4.5 * warp) * 6 / 64 + 0.9 * n2))
    h = smooth(0.12, 0.8, r) * (1 - 0.75 * smooth(0.9, 1.5, snoise(64, 64, rng, 1.2, 3, stretch=(1, 2.5))))
    lam = relief(h * 2.0)
    v = gray(0.08 + 0.55 * h + 0.45 * (lam - 0.45) + 0.07 * snoise(64, 64, rng, 0.6, 6, stretch=(3, 1)))
    return rgba(np.repeat(v[..., None], 3, -1))

def m_birch(rng):
    xx, yy = grid64()
    v = 1 + 0.04 * snoise(64, 64, rng, 1.0, 1, stretch=(1, 3)) + 0.03 * snoise(64, 64, rng, 0.2, 10)
    rgb = C(0.9, 0.89, 0.84) * v[..., None]
    for _ in range(45):
        x0, y0, L = rng.uniform(0, 64), rng.uniform(0, 64), rng.uniform(3, 10)
        m = (np.abs(wrapd(xx, x0)) < L / 2) & (np.abs(wrapd(yy, y0)) < (0.5 if rng.random() < 0.7 else 1.0))
        rgb = np.where(m[..., None], C(0.32, 0.3, 0.28), rgb)
    for _ in range(4):
        x0, y0, rx, ry = rng.uniform(0, 64), rng.uniform(0, 64), rng.uniform(4, 10), rng.uniform(1.5, 3.5)
        d = (wrapd(xx, x0) / rx) ** 2 + (wrapd(yy, y0) / ry) ** 2 + 0.35 * snoise(64, 64, rng, 0.8, 4)
        rgb = np.where((d < 1)[..., None], C(0.11, 0.1, 0.09), rgb)
    for _ in range(3):
        x0, y0 = rng.uniform(0, 64), rng.uniform(0, 64)
        d = (wrapd(xx, x0) / 3.5) ** 2 + (wrapd(yy, y0) / 1.5) ** 2
        rgb = np.where((d < 1)[..., None], C(0.8, 0.62, 0.46), rgb)
    return rgba(rgb)

def m_quilt(rng):
    Y, X = np.mgrid[0:64, 0:64]
    pal = [C(0.62, 0.12, 0.12), C(0.80, 0.62, 0.18), C(0.18, 0.36, 0.2), C(0.92, 0.86, 0.72)]
    rgb = np.zeros((64, 64, 3))
    for j in range(4):
        for i in range(4):
            c1, c2 = pal[(i + 2 * j + (i * j) % 3) % 4], pal[(i + 2 * j + 1 + (i * j) % 3) % 4]
            lx, ly = X[j * 16:j * 16 + 16, i * 16:i * 16 + 16] % 16, Y[j * 16:j * 16 + 16, i * 16:i * 16 + 16] % 16
            pat = (i * 3 + j) % 4
            if pat == 0:
                m = ((lx // 2 + ly // 2) % 2) == 0
            elif pat == 1:
                m = ((lx % 4 == 1) & (ly % 4 == 1))
            elif pat == 2:
                m = ((lx + ly) % 6) < 2
            else:
                m = np.zeros_like(lx, bool)
            col = np.where(m[..., None], c2 * 0.9 + 0.1, c1)
            puff = 0.82 + 0.2 * np.sin(np.pi * (lx + 0.5) / 16) * np.sin(np.pi * (ly + 0.5) / 16)
            rgb[j * 16:j * 16 + 16, i * 16:i * 16 + 16] = col * puff[..., None]
    seam, stitch = (X % 16 == 0) | (Y % 16 == 0), ((X % 16 == 2) | (Y % 16 == 2)) & (((X + Y) // 2) % 2 == 0)
    rgb = rgb * np.where(seam, 0.5, 1.0)[..., None]
    rgb = np.where(stitch[..., None], rgb * 0.6 + 0.35, rgb)
    return rgba(rgb * (1 + 0.04 * snoise(64, 64, rng, 0.2, 12))[..., None])

def m_rug(rng):
    Y, X = np.mgrid[0:64, 0:64]
    red, ochre, navy, cream = C(0.62, 0.13, 0.1), C(0.8, 0.56, 0.2), C(0.12, 0.16, 0.35), C(0.9, 0.84, 0.7)
    rgb, yb, dx = np.broadcast_to(red, (64, 64, 3)).copy(), Y % 32, np.abs((X % 32) - 15.5)
    dia = np.abs(yb - 15.5) + dx
    rgb = np.where(((dia < 13) & (dia > 10))[..., None], navy, rgb)
    rgb = np.where(((dia < 8) & (dia > 5))[..., None], ochre, rgb)
    rgb, band = np.where((dia < 2.5)[..., None], cream, rgb), Y % 32 < 3
    zig = np.abs(((X + 2) % 8) - 4) + (Y % 32) * 2 < 5
    rgb = np.where(band[..., None], np.where(zig[..., None], cream, navy), rgb)
    weave = 0.9 + 0.1 * (((X + Y) % 2) == 0) + 0.05 * snoise(64, 64, rng, 0.2, 12)
    return rgba(rgb * weave[..., None])

def m_books(rng):
    Y, X = np.mgrid[0:64, 0:64]
    rgb = C(0.16, 0.1, 0.06) * (0.7 + 0.3 * smooth(0, 20, Y))[..., None] * np.ones((64, 64, 1))
    pal = np.array([[0.55, 0.12, 0.1], [0.14, 0.3, 0.2], [0.15, 0.22, 0.42], [0.5, 0.35, 0.2],
                    [0.7, 0.55, 0.25], [0.35, 0.18, 0.28], [0.76, 0.7, 0.58], [0.25, 0.4, 0.45]])
    x = 0
    while x < 64:
        w = int(rng.integers(3, 8))
        w = 64 - x if 64 - x < w + 3 else w
        if rng.random() < 0.12 and w > 3:
            x += 1
            continue
        h, col = int(rng.integers(36, 55)), pal[rng.integers(0, 8)] * rng.uniform(0.8, 1.1)
        lx = (X - x).astype(float)
        m, u = (lx >= 0) & (lx < w) & (Y >= 58 - h) & (Y < 58), (lx + 0.5) / w
        sh = 0.6 + 0.55 * np.sin(np.pi * np.clip(u, 0, 1)) ** 0.6 * (1.05 - 0.25 * u)
        band = ((Y == 58 - h + 4) | (Y == 58 - h + 6) | (Y == 52)) & (rng.random() < 0.7)
        bc = np.where(band[..., None], C(0.85, 0.7, 0.35), col)
        lab = (Y > 58 - h + 12) & (Y < 58 - h + 20) & (lx > 0) & (lx < w - 1) & (rng.random() < 0.4)
        bc = np.where(lab[..., None], C(0.85, 0.8, 0.65), bc)
        rgb = np.where(m[..., None], bc * sh[..., None], rgb)
        x += w
    board = Y >= 58
    rgb = np.where(board[..., None], C(0.55, 0.36, 0.2) * np.where(Y == 58, 1.25, np.where(Y == 63, 0.6, 1.0))[..., None], rgb)
    return rgba(rgb * (1 + 0.05 * snoise(64, 64, rng, 0.3, 8))[..., None])

def m_kitchen(rng):
    Y, X = np.mgrid[0:64, 0:64]
    tx, ty, lx, ly = X // 8, Y // 8, X % 8, Y % 8
    cream, blue = C(0.93, 0.9, 0.8), C(0.24, 0.4, 0.66)
    blue_tile = ((tx + ty) % 4 == 0) | ((tx - ty) % 4 == 0)
    motif = ~blue_tile & (((np.abs(lx - 3.5) + np.abs(ly - 3.5)) < 2.2))
    rgb = np.where(blue_tile[..., None], blue, np.where(motif[..., None], blue * 1.2, cream))
    v = rng.uniform(0.94, 1.04, (8, 8))[ty, tx] * np.where((lx == 1) & (ly == 1), 1.12, 1.0)
    v = v * np.where((lx == 7) | (ly == 7), 0.9, 1.0)
    rgb, grout = rgb * v[..., None], (lx == 0) | (ly == 0)
    return rgba(np.where(grout[..., None], C(0.72, 0.7, 0.66), rgb))

def m_bricks(rng):
    Y, X = np.mgrid[0:64, 0:64]
    row = Y // 8
    xs = (X + (row % 2) * 8) % 64
    bid = row * 4 + xs // 16
    lx, ly = xs % 16, Y % 8
    col = np.array([[0.58, 0.27, 0.17], [0.66, 0.34, 0.2], [0.5, 0.24, 0.16], [0.36, 0.2, 0.14], [0.62, 0.4, 0.28]])
    bc = col[rng.integers(0, 5, 32)][bid] * rng.uniform(0.85, 1.1, 32)[bid][..., None]
    v = 1 + 0.1 * snoise(64, 64, rng, 0.4, 6)
    v = v * np.where((ly == 1) | (lx == 1), 1.12, np.where((ly == 7) | (lx == 15), 0.82, 1.0))
    soot = 0.75 + 0.25 * smooth(-1.5, 1.0, snoise(64, 64, rng, 1.4, 1))
    rgb, mortar = bc * (v * soot)[..., None], (ly == 0) | (lx == 0)
    return rgba(np.where(mortar[..., None], C(0.5, 0.47, 0.42) * soot[..., None], rgb))

def m_window(rng):
    Y, X = np.mgrid[0:64, 0:64]
    frame = (X < 4) | (X > 59) | (Y < 4) | (Y > 59) | (np.abs(X - 31.5) < 2) | (np.abs(Y - 31.5) < 2)
    wood = C(0.5, 0.33, 0.18) * (1 + 0.1 * snoise(64, 64, rng, 0.5, 4, stretch=(1, 4)))[..., None]
    bev = np.where((X == 0) | (Y == 0) | (X == 30) | (Y == 30), 1.25, np.where((X == 63) | (Y == 63) | (X == 33) | (Y == 33), 0.7, 1.0))
    refl = np.exp(-(((X + Y) % 32 - 12) / 3.0) ** 2)
    glass = C(0.66, 0.78, 0.84) + 0.25 * refl[..., None]
    rgb = np.where(frame[..., None], wood * bev[..., None], glass)
    a = np.where(frame, 1.0, (90 + 40 * refl) / 255)
    return rgba(rgb, a)

def m_door(rng):
    Y, X = np.mgrid[0:64, 0:64]
    rgb = planks(rng, C(0.52, 0.33, 0.18), nb=4, joints=0, var=0.08, gamp=0.16).transpose(1, 0, 2)
    for hy in (12, 50):
        dx = X - 1
        half = 2.2 - dx * 0.012
        m = (dx >= 0) & (dx < 40) & (np.abs(Y - hy) <= half)
        iron = C(0.16, 0.15, 0.15) * np.where(Y < hy, 1.5, 1.0)[..., None]
        rgb = np.where(m[..., None], iron, rgb)
        for rx in (4, 14, 26, 37):
            rgb = np.where(((X - rx) ** 2 + (Y - hy) ** 2 <= 1)[..., None], C(0.35, 0.33, 0.32), rgb)
    d = np.hypot(X - 53, Y - 33)
    ring, rgb = np.abs(d - 3.5) < 1.0, np.where((d < 1.6)[..., None], C(0.12, 0.11, 0.1), rgb)
    rgb = np.where(ring[..., None], C(0.2, 0.19, 0.18) * (1 + 0.6 * ((X < 53) & (Y < 33)))[..., None], rgb)
    rgb = np.where(((np.abs(X - 53) < 0.8) & (Y > 38) & (Y < 42))[..., None], C(0.05, 0.05, 0.05), rgb)
    return rgba(rgb)

def m_paper(rng):
    Y, X = np.mgrid[0:64, 0:64]
    rgb = C(0.95, 0.92, 0.82) * (1 + 0.03 * snoise(64, 64, rng, 1.0) - 0.04 * smooth(20, 46, np.hypot(X - 32, Y - 32)))[..., None]
    rgb = np.where(((Y % 6 == 5) & (Y > 6))[..., None], C(0.7, 0.78, 0.9), rgb)
    rgb, sp = np.where((X == 8)[..., None], C(0.9, 0.5, 0.5), rgb), Sprite(915, ss=4)
    for line in range(1, 10):
        y, x, end = line * 6 + 4, 11.0 + rng.uniform(0, 2), 60 if line < 9 else rng.uniform(24, 44)
        while x < end - 3:
            L = min(rng.uniform(4, 11), end - x)
            k, fq = max(int(L * 1.6), 3), rng.uniform(1.3, 2.2)
            pts = [(x + L * t / k, y - 1.2 * abs(math.sin(fq * (x + L * t / k))) - 0.3 * rng.random()) for t in range(k + 1)]
            sp.stroke(pts, 0.38, 0.32, C(0.14, 0.16, 0.32))
            x += L + rng.uniform(1.5, 3)
    ink = sp.resolve(thresh=0.35)
    return rgba(np.where(ink[..., 3:4] > 0.5, ink[..., :3], rgb))

def m_moss(rng):
    pts = rng.uniform(0, 64, (420, 2))
    f1, f2, ids, dx, dy = voronoi(64, pts)
    rad = 2.0
    hz = np.sqrt(np.clip(1 - (f1 / rad) ** 2, 0, 1))
    lam, lo = np.clip(dx / rad * LX + dy / rad * LY + hz * LZ, 0, 1), snoise(64, 64, rng, 1.2)
    col = mix(C(0.16, 0.3, 0.08), C(0.42, 0.58, 0.16), smooth(-1.5, 1.5, lo + rng.normal(0, 0.4, 420)[ids]))
    v = (0.7 + 0.4 * lam) * (1 + 0.1 * snoise(64, 64, rng, 0.0, 16))
    rgb, spore = col * v[..., None], rng.random((64, 64)) > 0.993
    return rgba(np.where(spore[..., None], C(0.55, 0.32, 0.15), rgb))

def m_velvet(rng):
    v = 0.85 + 0.12 * snoise(64, 64, rng, 1.4) + 0.04 * snoise(64, 64, rng, 0.0, 20)
    crease = np.exp(-(snoise(64, 64, rng, 1.6, 1, stretch=(1, 2)) / 0.25) ** 2)
    return rgba(C(0.16, 0.4, 0.21) * (v - 0.2 * crease)[..., None])

def m_curtain(rng):
    xx, yy = grid64()
    ph = rng.uniform(0, 6.28, 3)
    fold = sum(a * np.sin(2 * np.pi * k * xx / 64 + p) for k, a, p in zip((3, 5, 7), (1.0, 0.5, 0.25), ph))
    dfold = np.gradient(fold, axis=1)
    lam = np.clip(0.6 - 2.0 * dfold * -LX, 0, 1)
    v = (0.45 + 0.6 * lam) * (1 + 0.05 * snoise(64, 64, rng, 0.3, 8, stretch=(6, 1)))
    return rgba(C(0.58, 0.08, 0.1) * v[..., None])

def m_iron(rng):
    v = 1 + 0.15 * snoise(64, 64, rng, 1.0) + 0.1 * snoise(64, 64, rng, 0.0, 16)
    rgb, pits = C(0.17, 0.17, 0.18) * v[..., None], rng.random((64, 64)) > 0.97
    rgb, rust = np.where(pits[..., None], rgb * 0.5, rgb), smooth(1.6, 2.4, snoise(64, 64, rng, 1.2, 3))
    rgb, hi = mix(rgb, C(0.36, 0.2, 0.12), rust * 0.6), rng.random((64, 64)) > 0.985
    return rgba(np.where(hi[..., None], rgb + 0.12, rgb))

def m_frame(rng):
    Y, X = np.mgrid[0:64, 0:64]
    d = np.minimum(np.minimum(X, Y), np.minimum(63 - X, 63 - Y)).astype(float) + 0.5
    prof = np.interp(d, [0, 1, 2.5, 4, 5, 6, 7, 8], [0.2, 0.8, 1.6, 1.0, 1.25, 0.6, 0.4, 0.2])
    lam, beads = relief(prof * 1.6), (np.abs(d - 4.5) < 0.6) & (((X + Y) % 3) == 0)
    v = 0.3 + 0.95 * lam + 0.15 * beads + 0.06 * snoise(64, 64, rng, 0.3, 8)
    rgb = C(0.82, 0.62, 0.24) * v[..., None] + 0.2 * np.clip(lam - 0.85, 0, 1)[..., None]
    return edge_fill(rgba(rgb, (d < 8).astype(float)))

def m_pot(rng):
    xx, yy = grid64()
    v = 1 + 0.06 * snoise(64, 64, rng, 0.8) + 0.04 * np.sin(yy * 2.2 + 2 * snoise(64, 64, rng, 1.5))
    rim = yy < 12
    rimsh = np.where(rim, 1.12 - 0.35 * smooth(3, 11.5, yy), 1.0) * np.where((yy >= 12) & (yy < 15), 0.62 + 0.13 * (yy - 12), 1.0)
    rgb = C(0.72, 0.38, 0.22) * (v * rimsh)[..., None]
    bloom = smooth(0.8, 2.2, snoise(64, 64, rng, 1.3, 2)) * 0.35
    return rgba(mix(rgb, C(0.86, 0.76, 0.66), bloom))

def m_ivy(rng):
    sp = Sprite(922, ss=4)
    stems = [bez((rng.uniform(0, 64), -4), (rng.uniform(0, 64), 32), (rng.uniform(0, 64), 68), 12) for _ in range(3)]
    leaves = []
    for s in stems:
        for k in range(1, 12):
            if rng.random() < 0.85:
                a = rng.uniform(-math.pi, math.pi)
                leaves.append((rng.random(), s[k][0], s[k][1], a, rng.uniform(5.0, 7.5), rng.normal(0, 0.08)))
    for _ in range(10):
        leaves.append((rng.random(), rng.uniform(0, 64), rng.uniform(0, 64), rng.uniform(-math.pi, math.pi),
                       rng.uniform(5, 7), rng.normal(0, 0.08)))
    leaves.sort()

    def draw(ox, oy):
        for s in stems:
            sp.stroke(s + (ox, oy), 0.55, 0.55, C(0.3, 0.26, 0.16))
        for depth, x, y, a, L, j in leaves:
            l = 0.55 + 0.35 * depth + j
            dark = C(0.14, 0.3, 0.1) * l
            for da, s2 in ((0, 1.0), (1.2, 0.7), (-1.2, 0.7), (2.3, 0.45), (-2.3, 0.45)):
                sp.leaf(x + ox, y + oy, a + da, L * s2, L * 0.6 * s2, dark, 0.55, rib=0.0)
            for da, s2 in ((0, 0.9), (1.2, 0.6), (-1.2, 0.6)):
                sp.capsule(x + ox, y + oy, x + ox + math.cos(a + da) * L * s2, y + oy + math.sin(a + da) * L * s2,
                           0.3, 0.2, C(0.45, 0.6, 0.3) * l)
    wrap9(draw)
    return sp.resolve()

def m_lantern(rng):
    xx, yy = grid64()
    r = np.hypot(xx - 32, yy - 34) / 32
    v = smooth(1.2, 0.0, r) + 0.08 * snoise(64, 64, rng, 1.0, 2) + 0.05 * np.sin(xx * 0.7 + 3 * snoise(64, 64, rng, 1.4))
    return rgba(mix(C(0.75, 0.38, 0.1), C(1.0, 0.86, 0.5), np.clip(v, 0, 1)))

def m_copper(rng):
    f1, f2, ids, dx, dy = voronoi(64, jitter_pts(rng, 10, 2.2))
    h = -(1 - np.clip(f1 / 4.5, 0, 1)) ** 2
    lam = relief(h * 3)
    rgb = mix(C(0.42, 0.2, 0.1), C(0.86, 0.52, 0.32), lam) + 0.35 * np.clip(lam - 0.85, 0, 1)[..., None] * 4
    rgb = rgb * (1 + 0.06 * snoise(64, 64, rng, 1.2))[..., None]
    pat = smooth(1.8, 2.6, snoise(64, 64, rng, 1.2, 2))
    return rgba(mix(rgb, C(0.38, 0.6, 0.5), pat * 0.6))

def m_ceramic(rng):
    f1, f2, *_ = voronoi(64, jitter_pts(rng, 6, 4))
    crackle = np.exp(-((f2 - f1) / 0.5) ** 2)
    v = 1 + 0.07 * snoise(64, 64, rng, 1.5) - 0.12 * crackle
    rgb, gloss = C(0.46, 0.63, 0.5) * v[..., None], smooth(1.5, 2.2, snoise(64, 64, rng, 1.6, 1))
    return rgba(mix(rgb, C(0.7, 0.85, 0.76), gloss * 0.3))

def m_starchart(rng):
    xx, yy = grid64()
    rgb = C(0.87, 0.79, 0.6) * (1 + 0.05 * snoise(64, 64, rng, 1.2) - 0.1 * smooth(1.5, 2.5, snoise(64, 64, rng, 1.0, 2)))[..., None]
    sp, ink = Sprite(926, ss=4), C(0.3, 0.2, 0.12)
    for rr in (29, 20, 11):
        pts = [(32 + rr * math.cos(t), 32 + rr * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 49)]
        sp.stroke(pts, 0.32, 0.32, ink)
    for k in range(12):
        t = k * math.pi / 6
        sp.capsule(32 + 11 * math.cos(t), 32 + 11 * math.sin(t), 32 + 29 * math.cos(t), 32 + 29 * math.sin(t), 0.25, 0.25, ink * 1.4)
    stars = rng.uniform(6, 58, (34, 2))
    stars = stars[np.hypot(stars[:, 0] - 32, stars[:, 1] - 32) < 28]
    for a, b in ((0, 1), (1, 2), (2, 3), (5, 6), (6, 7), (7, 5), (9, 10), (10, 11)):
        if b < len(stars):
            sp.capsule(*stars[a], *stars[b], 0.25, 0.25, C(0.55, 0.35, 0.2))
    for i, (x, y) in enumerate(stars):
        sp.blob(x, y, 0.7 + (i % 4 == 0) * 0.6, 0.7 + (i % 4 == 0) * 0.6, ink * 0.6)
    t = sp.resolve(thresh=0.3)
    return rgba(np.where(t[..., 3:4] > 0.5, t[..., :3], rgb))

def m_night(rng):
    rgb = C(0.03, 0.05, 0.16) + C(0.05, 0.05, 0.1) * smooth(-0.5, 2.0, snoise(64, 64, rng, 1.3))[..., None]
    for _ in range(46):
        x, y, b = rng.integers(0, 64), rng.integers(0, 64), rng.uniform(0.35, 1.0)
        tint = [C(1, 1, 1), C(0.75, 0.85, 1), C(1, 0.9, 0.7)][rng.integers(0, 3)]
        rgb[y, x] = np.maximum(rgb[y, x], tint * b)
    for _ in range(4):
        x, y = rng.integers(0, 64), rng.integers(0, 64)
        rgb[y, x] = 1.0
        for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            rgb[(y + ddy) % 64, (x + ddx) % 64] = np.maximum(rgb[(y + ddy) % 64, (x + ddx) % 64], 0.45)
    return rgba(rgb)

def m_water(rng):
    xx, yy = grid64()
    h = np.zeros((64, 64))
    for _ in range(9):
        kx, ky = rng.integers(-4, 5), rng.integers(1, 6)
        h += rng.uniform(0.3, 1.0) / math.hypot(kx, ky) * np.sin(2 * np.pi * (kx * xx + ky * yy) / 64 + rng.uniform(0, 6.3))
    h += 0.3 * snoise(64, 64, rng, 1.5, 2)
    lam = relief(h * 0.9)
    rgb = mix(C(0.22, 0.32, 0.38), C(0.5, 0.6, 0.66), lam) + 0.4 * np.clip(lam - 0.8, 0, 1)[..., None] * 3
    return rgba(rgb)

def m_sand(rng):
    xx, yy = grid64()
    rip = np.sin(2 * np.pi * (1 * xx + 6 * yy) / 64 + 2 * snoise(64, 64, rng, 1.5))
    v = 1 + 0.05 * rip + 0.04 * snoise(64, 64, rng, 1.2) + 0.07 * snoise(64, 64, rng, 0.0, 20)
    rgb, g = C(0.84, 0.74, 0.54) * v[..., None], rng.random((64, 64))
    rgb = np.where((g > 0.98)[..., None], rgb * 0.7, np.where((g < 0.015)[..., None], rgb * 1.12, rgb))
    return rgba(rgb)

def m_basket(rng):
    Y, X = np.mgrid[0:64, 0:64]
    row, ly = Y // 4, Y % 4
    k, lx = X // 8, X % 8
    stake = (lx < 3) & (((row + k) % 2) == 0)
    near = np.minimum(np.abs(lx - 2.5), np.abs(lx - 8.5)) * (((row + k) % 2) == 0) + 9 * (((row + k) % 2) == 1)
    wv = (0.5 + 0.6 * np.sin(np.pi * (ly + 0.5) / 4)) * (0.65 + 0.35 * smooth(0.5, 3.0, near))
    sv = 0.45 + 0.65 * np.sin(np.pi * (lx + 0.5) / 3)
    tint = np.where(stake, rng.uniform(0.85, 1.0, 8)[k], rng.uniform(0.88, 1.08, 16)[row])
    col = np.where(stake[..., None], C(0.62, 0.46, 0.26), C(0.78, 0.62, 0.36))
    g = 1 + 0.07 * snoise(64, 64, rng, 0.3, 8, stretch=(1, 4))
    return rgba(col * (np.where(stake, sv, wv) * tint * g)[..., None])

def m_plaster(rng):
    v = 1 + 0.035 * snoise(64, 64, rng, 1.3) + 0.02 * snoise(64, 64, rng, 0.0, 20)
    tr = relief(0.8 * snoise(64, 64, rng, 1.6, 2))
    return rgba(C(0.9, 0.88, 0.83) * (v * (0.94 + 0.08 * tr))[..., None])

def make_materials():
    fns = [m_log_side, m_log_end, lambda r: rgba(planks(r, C(0.78, 0.55, 0.3))),
           lambda r: rgba(planks(r, C(0.82, 0.66, 0.45), var=0.07, joints=0).transpose(1, 0, 2)),
           m_shingles, m_stones, m_bark_gray, m_birch, m_quilt, m_rug, m_books, m_kitchen, m_bricks,
           m_window, m_door, m_paper, m_moss, m_velvet, m_curtain, m_iron, m_frame, m_pot, m_ivy,
           m_lantern, m_copper, m_ceramic, m_starchart, m_night, m_water, m_sand, m_basket, m_plaster]
    atlas = np.zeros((512, 512, 4))
    for i, fn in enumerate(fns):
        tile_at(atlas, i, fn(R(300, i)))
    return atlas

# ---- Moon (128x128, disc radius 62, rendered at 2x) ------------------------

def make_moon():
    S, Rr, c, rng = 256, 124.0, 128.0, R(400)
    yy, xx = np.mgrid[0:S, 0:S] + 0.5
    nx, ny = (xx - c) / Rr, (yy - c) / Rr
    r2 = nx * nx + ny * ny
    nz = np.sqrt(np.clip(1 - r2, 0, 1))
    n1, n2, n3 = snoise(S, S, rng, 1.3, 2), snoise(S, S, rng, 1.0, 6), snoise(S, S, rng, 0.4, 20)

    def vec(x, y):
        return np.array([x, y, math.sqrt(max(1 - x * x - y * y, 0.0))])

    def ang(v, sl=(slice(None), slice(None))):
        return np.arccos(np.clip(nx[sl] * v[0] + ny[sl] * v[1] + nz[sl] * v[2], -1, 1))
    maria = [(-0.30, -0.45, 0.30), (0.18, -0.38, 0.18), (0.36, -0.10, 0.20), (0.70, -0.30, 0.11),
             (0.55, 0.15, 0.15), (0.33, 0.30, 0.09), (-0.55, -0.05, 0.31), (-0.65, 0.22, 0.17),
             (-0.18, 0.36, 0.17), (-0.48, 0.42, 0.10), (0.0, -0.15, 0.08), (-0.40, -0.70, 0.07),
             (-0.15, -0.72, 0.07), (0.10, -0.71, 0.07), (0.32, -0.66, 0.06), (-0.75, -0.30, 0.12)]
    m = np.zeros((S, S))
    for x, y, rad in maria:
        m = np.maximum(m, smooth(rad * 1.15, rad * 0.6, ang(vec(x, y)) + 0.045 * n1) * rng.uniform(0.8, 1.0))
    alb = 0.80 + 0.035 * n2 + 0.015 * n3 - m * (0.38 + 0.05 * n1)
    craters = [(-0.15, 0.68, 0.045, 1.6), (-0.30, -0.12, 0.05, 1.2), (-0.55, -0.08, 0.022, 1.4),
               (-0.67, -0.36, 0.02, 2.0), (-0.12, -0.78, 0.05, -1.0), (-0.88, 0.08, 0.06, -1.0),
               (0.12, 0.42, 0.03, 0.8), (0.48, 0.55, 0.035, 0.6), (-0.05, 0.05, 0.03, 0.6)]
    while len(craters) < 320:
        x, y = rng.uniform(-1, 1, 2)
        if x * x + y * y < 0.94:
            craters.append((x, y, min(0.006 * rng.uniform(0.05, 1) ** -0.55, 0.06), rng.uniform(0.2, 1.0) * (1 - 0.6 * m[int(c + y * Rr), int(c + x * Rr)])))
    for x, y, rho, br in craters:
        rp = int(rho * Rr * 2.5) + 3
        cx, cy = int(c + x * Rr), int(c + y * Rr)
        sl = (slice(max(cy - rp, 0), min(cy + rp, S)), slice(max(cx - rp, 0), min(cx + rp, S)))
        d = ang(vec(x, y), sl) / rho
        ux, uy = nx[sl] - x, ny[sl] - y
        un = np.hypot(ux, uy) + 1e-6
        lit = (ux * LX + uy * LY) / un
        if br < 0:
            alb[sl] = np.where(d < 0.9, alb[sl] * 0.62, alb[sl])
            br = 0.4
        e = br * (0.16 * np.exp(-((d - 1.0) / 0.16) ** 2) + 0.05 * np.exp(-((d - 1.3) / 0.45) ** 2) - 0.06 * (d < 0.8))
        e += 0.15 * (d < 1) * d * -lit + 0.06 * ((d > 1) & (d < 1.5)) * lit * (1.5 - d) * 2
        alb[sl] += e
    for (tx, ty, nr, amp, Lmax) in ((-0.15, 0.68, 26, 0.065, 1.2), (-0.30, -0.12, 16, 0.05, 0.45), (-0.55, -0.08, 10, 0.04, 0.25)):
        dx, dy = nx - tx, ny - ty
        for th in rng.uniform(0, 2 * np.pi, nr):
            along, perp = dx * math.cos(th) + dy * math.sin(th), -dx * math.sin(th) + dy * math.cos(th)
            L = Lmax * rng.uniform(0.4, 1.0)
            alb += amp * np.exp(-(perp / (0.006 + 0.03 * np.clip(along, 0, 2))) ** 2) * smooth(L, 0, along) * (along > 0.04)
        alb += amp * 0.6 * smooth(Lmax * 0.12 + 0.03, 0.02, np.hypot(dx, dy))
    lum, inside = np.clip(alb, 0, 1) * (0.88 + 0.12 * nz ** 0.5), (r2 <= 1).astype(float)
    lum, cov = (lum * inside).reshape(128, 2, 128, 2).sum((1, 3)), inside.reshape(128, 2, 128, 2).mean((1, 3))
    lum = lum / np.maximum(cov * 4, 1e-9)
    return edge_fill(rgba(np.repeat(lum[..., None], 3, -1), cov))

# ---- UI icons (16x16 pixel art, white glyph + dark outline) ----------------

def _ascii(d, rows):
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != '.':
                d.point((x, y), fill=1 if ch == '#' else 2)

def _cloud(d, y0=0):
    d.ellipse([1, 6 + y0, 7, 12 + y0], fill=1)
    d.ellipse([4, 2 + y0, 11, 9 + y0], fill=1)
    d.ellipse([8, 5 + y0, 14, 11 + y0], fill=1)
    d.rectangle([4, 8 + y0, 11, 12 + y0], fill=1)

def _arrow_left(d):
    d.polygon([(1, 7.5), (7, 1.5), (7, 13.5)], fill=1)
    d.rectangle([7, 6, 14, 9], fill=1)

def _star(d, cx, cy, ro, ri, fill=1):
    pts = []
    for k in range(10):
        a, r = -math.pi / 2 + k * math.pi / 5, ro if k % 2 == 0 else ri
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, fill=fill)

ICONS = [
    lambda d: (d.rectangle([1, 5, 14, 13], fill=1), d.rectangle([4, 3, 8, 5], fill=1),
               d.ellipse([5, 6, 11, 12], fill=2), d.ellipse([7, 8, 9, 10], fill=1), d.point((12, 7), fill=2)),
    lambda d: (d.polygon([(1, 8), (7, 2), (8, 2), (14, 8)], fill=1), d.rectangle([3, 8, 12, 14], fill=1),
               d.rectangle([11, 2, 12, 5], fill=1), d.rectangle([6, 10, 9, 14], fill=2)),
    lambda d: (d.rectangle([4, 1, 14, 10], fill=1), d.rectangle([1, 5, 11, 14], fill=1, outline=2),
               d.polygon([(3, 12), (5, 9), (7, 11), (8, 10), (10, 12)], fill=2), d.point((8, 7), fill=2)),
    lambda d: _ascii(d, ["................", "......####......", "..##..####..##..", "..############..",
                         "...##########...", "...####oo####...", ".#####oooo#####.", ".#####oooo#####.",
                         ".#####oooo#####.", ".#####oooo#####.", "...####oo####...", "...##########...",
                         "..############..", "..##..####..##..", "......####......", "................"]),
    lambda d: (d.ellipse([4, 4, 11, 11], fill=1),
               [d.line([(7.5 + 5.5 * math.cos(a), 7.5 + 5.5 * math.sin(a)), (7.5 + 7 * math.cos(a), 7.5 + 7 * math.sin(a))], fill=1, width=1)
                for a in np.arange(8) * math.pi / 4]),
    lambda d: (d.ellipse([1, 1, 14, 14], fill=1), d.ellipse([5, -1, 17, 11], fill=0)),
    lambda d: _cloud(d, 1),
    lambda d: (d.ellipse([1, 4, 6, 9], fill=1), d.ellipse([4, 1, 11, 8], fill=1), d.ellipse([9, 4, 14, 9], fill=1),
               d.rectangle([3, 6, 12, 9], fill=1), [d.line([(x, 11), (x - 1, 13)], fill=1) for x in (4, 8, 12)]),
    lambda d: (d.line([(7, 1), (7, 13)], fill=1), d.line([(2, 4), (12, 10)], fill=1), d.line([(2, 10), (12, 4)], fill=1),
               [d.point(p, fill=1) for p in ((6, 2), (8, 2), (6, 12), (8, 12))], d.point((7, 7), fill=2)),
    lambda d: d.polygon([(10, 1), (3, 8), (7, 8), (5, 14), (12, 6), (8, 6), (11, 1)], fill=1),
    lambda d: [d.rectangle([x0, y, x1, y + 1], fill=1) for x0, x1, y in ((4, 14, 2), (1, 11, 6), (3, 13, 10), (6, 12, 13))],
    lambda d: (d.polygon([(2, 13), (3, 8), (6, 4), (10, 2), (14, 1), (13, 5), (11, 9), (7, 12)], fill=1),
               d.line([(3, 12), (12, 3)], fill=2), d.line([(1, 14), (3, 12)], fill=1)),
    lambda d: (d.ellipse([1, 1, 14, 14], fill=1), d.line([(7, 3), (7, 8)], fill=2), d.line([(7, 8), (10, 10)], fill=2),
               [d.point(p, fill=2) for p in ((7, 2), (12, 7), (7, 12), (2, 7))]),
    lambda d: (d.rounded_rectangle([1, 4, 14, 11], radius=2, fill=1), d.ellipse([3, 6, 6, 9], fill=2),
               d.ellipse([9, 6, 12, 9], fill=2), d.polygon([(6, 12), (7, 9), (8, 9), (9, 12)], fill=0),
               d.rectangle([5, 2, 10, 3], fill=1)),
    lambda d: d.polygon([(4, 2), (4, 13), (12, 7.5)], fill=1),
    lambda d: (d.rectangle([3, 2, 6, 13], fill=1), d.rectangle([9, 2, 12, 13], fill=1)),
    _arrow_left, 'flip', 'rot_up', 'rot_down',
    lambda d: (d.rectangle([6, 2, 9, 13], fill=1), d.rectangle([2, 6, 13, 9], fill=1)),
    lambda d: d.rectangle([2, 6, 13, 9], fill=1),
    lambda d: (d.line([(3, 3), (12, 12)], fill=1, width=3), d.line([(3, 12), (12, 3)], fill=1, width=3)),
    lambda d: (d.rectangle([6, 1, 9, 6], fill=1), d.polygon([(3, 6), (12, 6), (7.5, 10.5)], fill=1),
               d.rectangle([1, 10, 2, 14], fill=1), d.rectangle([13, 10, 14, 14], fill=1), d.rectangle([1, 13, 14, 14], fill=1)),
    lambda d: (d.rectangle([3, 1, 12, 14], fill=1), d.rectangle([5, 3, 10, 7], outline=2),
               d.rectangle([5, 9, 10, 12], outline=2), d.point((11, 8), fill=2)),
    lambda d: (d.ellipse([1, 1, 14, 14], fill=1), d.polygon([(7.5, 2), (10, 7.5), (5, 7.5)], fill=2),
               d.polygon([(5, 7.5), (10, 7.5), (7.5, 13)], fill=1, outline=2), d.point((7, 7), fill=1)),
    lambda d: _ascii(d, ["................", "..........###...", "..........###...", "......#####.....",
                         ".....#.####.##..", "....#..####..#..", "...#..####......", "......###.......",
                         ".....#####......", "....##...##.....", "...##.....##....", ".###.......#....",
                         "...........##...", "................", "................", "................"]),
    lambda d: (d.line([(2, 8), (7, 3), (8, 3), (13, 8)], fill=1, width=2), d.line([(2, 13), (7, 8), (8, 8), (13, 13)], fill=1, width=2)),
    lambda d: _ascii(d, ["................", "......##........", "...##.##.##.....", "...##.##.##.##..",
                         "...##.##.##.##..", "...##.##.##.##..", "...##.##.##.##..", "...###########..",
                         ".#.###########..", ".##.##########..", "..#############.", "...###########..",
                         "....#########...", ".....#######....", ".....#######....", "................"]),
    lambda d: [d.rectangle([1, y, 14, y + 1], fill=1) for y in (2, 7, 12)],
    lambda d: d.ellipse([1, 1, 14, 14], outline=1, width=2),
    lambda d: d.ellipse([2, 2, 13, 13], fill=1),
    lambda d: ([d.ellipse([7.5 + 3.2 * math.cos(a) - 2.2, 6 + 3.2 * math.sin(a) - 2.2, 7.5 + 3.2 * math.cos(a) + 2.2, 6 + 3.2 * math.sin(a) + 2.2], fill=1)
                for a in np.arange(5) * 2 * math.pi / 5 - math.pi / 2],
               d.ellipse([6, 5, 9, 8], fill=2), d.line([(7, 10), (7, 14)], fill=1), d.line([(8, 12), (11, 11)], fill=1)),
    lambda d: (d.polygon([(1, 14), (7, 5), (9, 7), (10, 6), (14, 14)], fill=1), d.line([(7, 1), (7, 5)], fill=1),
               d.polygon([(8, 1), (11, 2), (8, 3)], fill=1), d.line([(5, 9), (7, 8), (9, 10), (10, 9)], fill=2)),
    lambda d: (d.ellipse([2, 7, 5, 13], fill=1), [d.point((x, 6), fill=1) for x in (2, 4, 5)],
               d.ellipse([9, 2, 12, 8], fill=1), [d.point((x, 1), fill=1) for x in (9, 11, 12)]),
    lambda d: (d.line([(1, 7), (8, 7), (1, 14), (8, 14)], fill=1, width=2), d.line([(9, 1), (13, 1), (9, 5), (13, 5)], fill=1, width=1)),
    lambda d: ([d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=2) for x, y in ((7.75, 5.2), (5, 10), (10.5, 10))],
               [d.ellipse([x - 4, y - 4, x + 4, y + 4], outline=1) for x, y in ((7.75, 5.2), (5, 10), (10.5, 10))]),
    lambda d: (d.ellipse([1, 4, 14, 11], fill=1), d.ellipse([5, 4, 10, 11], fill=2), d.point((6, 6), fill=1)),
    lambda d: _star(d, 7.5, 8, 7, 3),
    lambda d: (d.rectangle([2, 3, 13, 4], fill=1), d.rectangle([6, 1, 9, 2], fill=1),
               d.polygon([(3, 5), (12, 5), (11, 14), (4, 14)], fill=1), [d.line([(x, 7), (x, 12)], fill=2) for x in (6, 9)]),
]

def make_icons():
    atlas, glyphs = np.zeros((256, 256, 4), np.uint8), []
    for i, fn in enumerate(ICONS):
        if fn == 'flip':
            g = glyphs[16][:, ::-1]
        elif fn == 'rot_up':
            g = np.rot90(glyphs[16], -1)
        elif fn == 'rot_down':
            g = np.rot90(glyphs[16], 1)
        else:
            im = Image.new('L', (16, 16), 0)
            fn(ImageDraw.Draw(im))
            g = np.array(im)
        glyphs.append(g)
        on = g > 0
        p, dil = np.pad(on, 1), np.zeros_like(on)
        for dy in (0, 1, 2):
            for dx in (0, 1, 2):
                dil |= p[dy:dy + 16, dx:dx + 16]
        t = np.zeros((16, 16, 4), np.uint8)
        t[dil] = (28, 26, 34, 255)
        t[g == 1] = (250, 248, 240, 255)
        t[g == 2] = (28, 26, 34, 255)
        atlas[(i // 16) * 16:(i // 16) * 16 + 16, (i % 16) * 16:(i % 16) * 16 + 16] = t
    return atlas

# ---- Font ------------------------------------------------------------------

def make_font():
    src = open(os.path.join(ROOT, "tools", "font8x8_basic.h")).read()
    body = src[src.index("font8x8_basic[128][8]"):]
    rows = re.findall(r"\{([^{}]*)\}", body)
    glyphs = [[int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", r)] for r in rows]
    assert len(glyphs) == 128 and all(len(gl) == 8 for gl in glyphs)
    img = np.zeros((64, 128, 4), np.uint8)
    for c, gl in enumerate(glyphs):
        for y, byte in enumerate(gl):
            for x in range(8):
                if byte >> x & 1:
                    img[(c // 16) * 8 + y, (c % 16) * 8 + x] = 255
    return img

# ---- Sounds (22050 Hz, mono, 16-bit PCM) ----------------------------------
# Loops are built circularly: noise is synthesised in the frequency domain over
# exactly the loop length, modulators are periodic and every event's tail wraps
# around to the start, so the last sample flows seamlessly into the first.

SR = 22050

def ns(d):
    return int(round(d * SR))

def tt(n):
    return np.arange(n) / SR

def sfilt(x, lo=None, hi=None, order=2, circ=False, peaks=(), tilt=0.0):
    """Zero-phase FFT filter. circ=True filters a loop in the circular domain."""
    n = len(x)
    m = n if circ else n + SR // 4
    X, f = np.fft.rfft(x, m), np.fft.rfftfreq(m, 1 / SR)
    fs, g = np.maximum(f, 1.0), np.ones_like(f)
    if lo:
        g /= np.sqrt(1 + (lo / fs) ** (2 * order))
    if hi:
        g /= np.sqrt(1 + (fs / hi) ** (2 * order))
    if tilt:
        g *= (fs / 1000.0) ** tilt
    for fc, bw, gain in peaks:
        g = g + gain * np.exp(-0.5 * ((f - fc) / bw) ** 2)
    return np.fft.irfft(X * g, m)[:n]

def noise(rng, n, **kw):
    y = sfilt(rng.standard_normal(n), **kw)
    return y / (np.std(y) + 1e-12)

def slow(rng, n, fmax, circ=True):
    """Smooth random modulator in 0..1 (periodic when circ)."""
    y = sfilt(rng.standard_normal(n), hi=fmax, order=3, circ=circ)
    y -= y.min()
    return y / (y.max() + 1e-12)

def put_at(buf, i0, sig, wrap=True):
    idx = i0 + np.arange(len(sig))
    if wrap:
        np.add.at(buf, idx % len(buf), sig)
    else:
        k = (idx >= 0) & (idx < len(buf))
        buf[idx[k]] += sig[k]

def env_ar(n, att, rel):
    t = np.linspace(0, 1, n)
    return smooth(0, att, t) * smooth(1, 1 - rel, t)

def sweep(f, amp, harm=((1, 1.0),)):
    ph = 2 * np.pi * np.cumsum(f) / SR
    return amp * sum(a * np.sin(k * ph) for k, a in harm)

def decay(n, freq, tau, phase=0.0):
    t = tt(n)
    return np.sin(2 * np.pi * freq * t + phase) * np.exp(-t / tau)

def burst(rng, n, tau, lo=None, hi=None):
    t = tt(n)
    return sfilt(rng.standard_normal(n), lo=lo, hi=hi) * np.exp(-t / tau)

def reverb(x, rng, t60=1.0, mix=0.25, circ=False, damp=3500):
    L = min(ns(t60 * 1.2), len(x))
    t = tt(L)
    ir, n = sfilt(rng.standard_normal(L), hi=damp) * np.exp(-6.9 * t / t60) * smooth(0, 0.012, t), len(x)
    m = n if circ else n + L
    y = np.fft.irfft(np.fft.rfft(x, m) * np.fft.rfft(ir, m), m)[:n]
    y *= np.std(x) / (np.std(y) + 1e-12)
    return x * (1 - mix) + y * mix

def finish(x, peak_db, loop=False, fade_in=0.004, fade_out=0.03):
    x = np.asarray(x, float)
    if loop:
        x = x - x.mean()
    else:
        x = sfilt(x, lo=25)
        n = len(x)
        x = x * smooth(0, ns(fade_in), np.arange(n)) * smooth(n - 1, n - 1 - ns(fade_out), np.arange(n))
    return x * (10 ** (peak_db / 20) / (np.abs(x).max() + 1e-12))

def write_wav(name, x):
    data = np.round(np.clip(x, -1, 1) * 32767).astype('<i2')
    with wave.open(os.path.join(SND_DIR, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())

def s_wind():
    rng, n = R(500, 1), ns(8.0)
    gust = slow(rng, n, 0.35) ** 1.3
    body = noise(rng, n, lo=70, hi=900, circ=True, tilt=-0.6) * (0.3 + 0.7 * gust)
    out = body
    for fc, w in ((380, 0.25), (470, 0.45), (590, 0.7), (720, 1.0)):
        band = noise(rng, n, circ=True, lo=fc * 0.9, hi=fc * 1.1, order=4)
        out = out + 0.22 * band * gust ** (1 + 3 * w) * slow(rng, n, 1.2)
    leaves = noise(rng, n, lo=2500, hi=7000, circ=True) * gust ** 2 * (0.5 + 0.5 * slow(rng, n, 6.0))
    return finish(out + 0.12 * leaves, -9, loop=True)

def s_rain():
    rng, n = R(500, 2), ns(6.0)
    hiss = noise(rng, n, lo=900, hi=7500, circ=True, tilt=-0.3) * (0.85 + 0.15 * slow(rng, n, 0.5))
    rumble, imp = noise(rng, n, lo=60, hi=500, circ=True), np.zeros(n)
    k = rng.integers(0, n, 2600)
    np.add.at(imp, k, rng.uniform(-1, 1, 2600) ** 3)
    ir = burst(rng, ns(0.006), 0.0012, lo=2500, hi=8000)
    drops, plinks = np.fft.irfft(np.fft.rfft(imp) * np.fft.rfft(ir, n), n), np.zeros(n)
    for _ in range(70):
        f0, m = rng.uniform(1500, 4000), ns(0.03)
        put_at(plinks, int(rng.integers(0, n)), sweep(f0 * (1 + 2.5 * tt(m)), np.exp(-tt(m) / 0.006)) * rng.uniform(0.1, 0.5))
    x = 0.5 * hiss + 0.25 * rumble + 2.5 * drops / (np.std(drops) + 1e-9) * 0.12 + 0.5 * plinks
    return finish(reverb(x, rng, 0.4, 0.15, circ=True), -9, loop=True)

def bird_phrase(rng, kind):
    out = []
    if kind == 0:      # warbling song (robin-like)
        for _ in range(int(rng.integers(5, 9))):
            d = rng.uniform(0.06, 0.15)
            m = ns(d)
            f0, f1 = rng.uniform(2200, 4400, 2)
            f = np.linspace(f0, f1, m) * (1 + 0.03 * np.sin(2 * np.pi * rng.uniform(25, 60) * tt(m)))
            out += [sweep(f, env_ar(m, 0.15, 0.35), ((1, 1), (2, 0.12))), np.zeros(ns(rng.uniform(0.02, 0.07)))]
    elif kind == 1:    # accelerating descending trill with a flourish (chaffinch-like)
        for i in range(12):
            m, f0 = ns(0.035), 5200 - i * 140
            out += [sweep(np.linspace(f0, f0 - 1200, m), env_ar(m, 0.1, 0.5), ((1, 1), (2, 0.1))),
                    np.zeros(ns(0.05 - i * 0.002))]
        m = ns(0.16)
        out.append(sweep(np.linspace(2600, 4200, m) ** 1.0, env_ar(m, 0.1, 0.4), ((1, 1), (2, 0.15))))
    elif kind == 2:    # "tea-cher tea-cher" (great tit-like)
        for _ in range(3):
            for f in (5200, 3700):
                m = ns(0.085)
                out += [sweep(np.full(m, f) * np.linspace(1.02, 0.98, m), env_ar(m, 0.12, 0.3), ((1, 1), (2, 0.08))),
                        np.zeros(ns(0.035))]
            out.append(np.zeros(ns(0.07)))
    elif kind == 3:    # thin high "tsee" notes
        for _ in range(3):
            m = ns(0.13)
            out += [sweep(np.linspace(7600, 7000, m), env_ar(m, 0.3, 0.4)), np.zeros(ns(0.09))]
    else:              # chips
        for _ in range(int(rng.integers(2, 4))):
            m = ns(0.03)
            out += [sweep(np.linspace(4500, 3200, m), env_ar(m, 0.1, 0.6), ((1, 1), (2, 0.2))), np.zeros(ns(0.12))]
    return np.concatenate(out)

def s_birds():
    rng, n = R(500, 3), ns(8.0)
    x = 0.004 * noise(rng, n, lo=80, hi=500, circ=True) * (0.6 + 0.4 * slow(rng, n, 0.3))
    plan = [(0.2, 0, 0.9), (1.7, 4, 0.35), (2.5, 1, 0.7), (4.1, 2, 0.45), (5.3, 0, 0.8), (6.4, 3, 0.3), (7.3, 4, 0.5)]
    for t0, kind, amp in plan:
        ph = bird_phrase(rng, kind) * amp
        if amp < 0.5:
            ph = sfilt(ph, hi=5000)
        put_at(x, ns(t0 + rng.uniform(-0.1, 0.1)), ph)
    return finish(reverb(x, rng, 0.9, 0.22, circ=True), -6, loop=True)

def s_crickets():
    rng, n = R(500, 4), ns(6.0)
    x = np.zeros(n)
    for fc, per, amp, pulses in ((4400, 0.6, 1.0, 3), (4750, 0.5, 0.55, 4), (5100, 0.75, 0.35, 3), (4200, 1.0, 0.3, 2)):
        k, off = int(round(6.0 / per)), rng.uniform(0, per)
        for j in range(k):
            t0 = off + j * 6.0 / k + rng.normal(0, 0.01)
            for p in range(pulses):
                m = ns(0.016)
                f = fc * (1 - 0.025 * np.linspace(0, 1, m))
                put_at(x, ns(t0 + p * 0.03), amp * sweep(f, np.sin(np.pi * np.linspace(0, 1, m)) ** 2, ((1, 1), (2, 0.08))))
    chorus = noise(rng, n, lo=4000, hi=5600, circ=True, order=4) * (0.5 + 0.5 * np.sin(2 * np.pi * 27 * tt(n))) ** 2
    air = noise(rng, n, lo=60, hi=400, circ=True)
    return finish(x + 0.08 * chorus * slow(rng, n, 0.5) + 0.03 * air, -12, loop=True)

def s_stream():
    rng, n = R(500, 5), ns(6.0)
    bed = noise(rng, n, lo=250, hi=2600, circ=True, peaks=((700, 250, 0.6),)) * (0.6 + 0.4 * slow(rng, n, 9.0))
    x = 0.25 * bed + 0.1 * noise(rng, n, lo=2000, hi=6000, circ=True) * slow(rng, n, 4.0)
    for _ in range(420):
        f0 = math.exp(rng.uniform(math.log(350), math.log(1700)))
        dr = 0.043 * f0 + 0.0014 * f0 ** 1.5
        m = ns(min(5 / dr, 0.12))
        t = tt(m)
        b = sweep(f0 * (1 + 0.4 * dr * t), np.exp(-t * dr) * smooth(0, 0.001, t)) * rng.uniform(0.05, 0.35) * (500 / f0) ** 0.3
        put_at(x, int(rng.integers(0, n)), b)
    return finish(reverb(x, rng, 0.35, 0.2, circ=True), -9, loop=True)

def s_fire():
    rng, n = R(500, 6), ns(6.0)
    roar = noise(rng, n, lo=40, hi=320, circ=True) * (0.6 + 0.4 * slow(rng, n, 1.5))
    hiss = noise(rng, n, lo=2500, hi=8000, circ=True) * slow(rng, n, 3.0)
    x, rate = 0.5 * roar + 0.06 * hiss, slow(rng, n, 0.8)
    for _ in range(260):
        i = int(rng.integers(0, n))
        if rng.random() > 0.35 + 0.65 * rate[i]:
            continue
        if rng.random() < 0.85:
            c = burst(rng, ns(0.004), rng.uniform(0.0003, 0.0012), lo=1500) * rng.uniform(0.2, 1.0)
        else:
            m = ns(0.03)
            c = (burst(rng, m, 0.003, lo=600) + 0.6 * decay(m, rng.uniform(250, 700), 0.008)) * rng.uniform(0.8, 1.6)
        put_at(x, i, c)
    return finish(x, -6, loop=True)

def s_step(kind, var):
    rng, p = R(510, kind, var), 1 + (var - 0.5) * 0.12
    if kind == 0:      # grass: two soft rustles (heel, toe)
        n = ns(0.26)
        t = tt(n)
        e = np.exp(-t / 0.05) * smooth(0, 0.006, t) + 0.7 * np.exp(-np.clip(t - 0.07, 0, None) / 0.07) * smooth(0.07, 0.085, t)
        grit = 1 + 2.5 * np.abs(sfilt(rng.standard_normal(n) * (rng.random(n) > 0.97), hi=3000))
        x = noise(rng, n, lo=1200 * p, hi=6500) * e * grit + 0.4 * decay(n, 85 * p, 0.03) * smooth(0, 0.004, t)
    elif kind == 1:    # rock: hard click, short body, scattering grit
        n = ns(0.22)
        t = tt(n)
        x = 0.6 * burst(rng, n, 0.0015, lo=2000) + 0.8 * decay(n, 210 * p, 0.022) + 0.35 * decay(n, 470 * p, 0.012)
        for _ in range(16):
            put_at(x, ns(rng.uniform(0.005, 0.09)), burst(rng, ns(0.004), 0.0006, lo=3000) * rng.uniform(0.05, 0.3), wrap=False)
        x[ns(0.06):] += 0.35 * (0.6 * burst(rng, n - ns(0.06), 0.002, lo=1500) + decay(n - ns(0.06), 240 * p, 0.015))
    elif kind == 2:    # snow: compressing crunch of tiny grains
        n = ns(0.3)
        t, imp = tt(n), (rng.random(n) > 0.94) * rng.uniform(-1, 1, n)
        crunch = sfilt(np.convolve(imp, rng.standard_normal(40) * np.exp(-np.arange(40) / 8))[:n], lo=500 * p, hi=4500)
        e = smooth(0, 0.05, t) * np.exp(-np.clip(t - 0.05, 0, None) / 0.07)
        x = crunch / (np.std(crunch) + 1e-9) * e + 0.5 * decay(n, 70 * p, 0.04) * smooth(0, 0.01, t)
    else:              # wood: hollow plank thud
        n = ns(0.25)
        t = tt(n)
        x = (decay(n, 105 * p, 0.045) + 0.5 * decay(n, 190 * p, 0.03) + 0.35 * decay(n, 650 * p, 0.012)
             + 0.15 * decay(n, 1350 * p, 0.006) + 0.25 * burst(rng, n, 0.0015, lo=2000))
        x[ns(0.055):] += 0.4 * (decay(n - ns(0.055), 120 * p, 0.03) + 0.3 * decay(n - ns(0.055), 700 * p, 0.01))
    return finish(x, -6, fade_in=0.001, fade_out=0.02)

def s_door_open():
    rng, n = R(520), ns(0.85)
    t = tt(n)
    rate = 70 + 85 * np.sin(np.pi * np.clip(t / 0.75, 0, 1)) ** 0.7 + 25 * (slow(rng, n, 6, circ=False) - 0.5)
    ph = np.cumsum(rate) / SR
    tick = np.diff(np.floor(ph), prepend=0) > 0
    imp, m = tick * rng.uniform(0.6, 1.0, n) * smooth(0.02, 0.1, t) * smooth(0.8, 0.6, t), ns(0.05)
    ir = decay(m, 520, 0.018) + 0.6 * decay(m, 1150, 0.01) + 0.35 * decay(m, 2300, 0.006) + 0.2 * decay(m, 3400, 0.004)
    creak, latch = np.convolve(imp, ir)[:n], np.zeros(n)
    latch[:ns(0.03)] = decay(ns(0.03), 2900, 0.004) + 0.5 * burst(rng, ns(0.03), 0.002, lo=2000)
    x = creak / (np.abs(creak).max() + 1e-9) + 0.5 * latch + 0.15 * decay(n, 110, 0.06)
    return finish(reverb(x, rng, 0.4, 0.15), -5)

def s_door_close():
    rng, n = R(521), ns(0.5)
    t = tt(n)
    x = decay(n, 78, 0.08) + 0.6 * decay(n, 142, 0.05) + 0.6 * burst(rng, n, 0.015, hi=700) * smooth(0, 0.003, t)
    i = ns(0.055)
    x[i:] += 0.25 * (decay(n - i, 2500, 0.005) + 0.6 * decay(n - i, 4100, 0.003) + 0.4 * burst(rng, n - i, 0.0015, lo=2500))
    return finish(reverb(x, rng, 0.35, 0.2), -5, fade_out=0.08)

def s_shutter():
    rng, n = R(522), ns(0.25)
    x = np.zeros(n)
    for t0, f, a in ((0.0, 3200, 1.0), (0.095, 2700, 0.8)):
        i, m = ns(t0), ns(0.05)
        c = 0.7 * burst(rng, m, 0.0015, lo=2500) + decay(m, f, 0.006) + 0.4 * decay(m, f * 1.9, 0.003)
        put_at(x, i, a * c, wrap=False)
    i, j = ns(0.01), ns(0.09)
    x[i:j] += 0.05 * noise(rng, j - i, lo=1200, hi=3500) * np.sin(np.linspace(0, np.pi, j - i))
    return finish(x, -6, fade_in=0.0005, fade_out=0.03)

def bell(n, f, amp, decay_s=1.0, partials=((1, 1.0, 1.0), (2.0, 0.35, 0.6), (3.0, 0.12, 0.4), (4.2, 0.07, 0.25), (5.4, 0.04, 0.18))):
    t = tt(n)
    return amp * sum(a * np.sin(2 * np.pi * f * r * t) * np.exp(-t / (decay_s * d)) for r, a, d in partials) * smooth(0, 0.003, t)

def s_summon(up=True):
    rng, dur = R(523 if up else 524), 1.5 if up else 1.2
    n = ns(dur)
    t, notes = tt(n), [523.25, 587.33, 659.26, 783.99, 880.0, 1046.5, 1174.7, 1318.5, 1568.0, 1760.0, 2093.0]
    if not up:
        notes = notes[::-1]
    x, step = np.zeros(n), (dur * 0.62) / len(notes)
    for k, f in enumerate(notes):
        put_at(x, ns(k * step), bell(ns(0.7), f, 0.5, 0.35), wrap=False)
    sh = np.zeros(n)
    for _ in range(14):
        f0 = rng.uniform(700, 1400)
        glide = f0 * (1 + 1.6 * t / dur) if up else f0 * 2.4 / (1 + 1.6 * t / dur)
        trem = 0.6 + 0.4 * np.sin(2 * np.pi * rng.uniform(6, 11) * t + rng.uniform(0, 6.3))
        sh += sweep(glide, trem * rng.uniform(0.3, 1.0))
    shape = smooth(0, dur * 0.7, t) * smooth(dur, dur * 0.75, t) if up else smooth(0, 0.05, t) * smooth(dur, 0.2, t)
    x = x + 0.08 * sh / 14 * shape * 6 + 0.04 * noise(rng, n, lo=4000, hi=9000) * shape
    return finish(reverb(x, rng, 0.8, 0.35), -5, fade_out=0.15)

def s_thunder():
    rng, n = R(525), ns(3.2)
    t, brown = tt(n), np.cumsum(rng.standard_normal(n))
    low = sfilt(brown - np.linspace(brown[0], brown[-1], n), lo=25, hi=200)
    low /= np.std(low)
    mid, e = noise(rng, n, lo=120, hi=600) * (0.5 + 0.5 * slow(rng, n, 12, circ=False)), np.zeros(n)
    for t0, a, att, dec in ((0.0, 0.45, 0.04, 0.25), (0.3, 1.0, 0.12, 0.6), (0.95, 0.7, 0.15, 0.5), (1.6, 0.5, 0.2, 0.6), (2.2, 0.3, 0.2, 0.5)):
        tau = np.clip(t - t0, 0, None)
        e += a * (1 - np.exp(-tau / att)) * np.exp(-tau / dec) * (t >= t0)
    x = (low + 0.35 * mid) * e
    return finish(x, -4, fade_in=0.02, fade_out=0.4)

def s_owl():
    rng, n = R(526), ns(1.9)
    x = np.zeros(n)
    for t0, d, f0, f1 in ((0.05, 0.42, 410, 392), (0.68, 0.55, 395, 365)):
        m = ns(d)
        tm = tt(m)
        f, e = np.linspace(f0, f1, m) * (1 + 0.006 * np.sin(2 * np.pi * 5.5 * tm)), env_ar(m, 0.18, 0.45)
        v = sweep(f, e, ((1, 1.0), (2, 0.18), (3, 0.05)))
        v += 0.12 * e * noise(rng, m, lo=f0 * 0.7, hi=f0 * 1.6)
        put_at(x, ns(t0), v, wrap=False)
    return finish(reverb(x, rng, 1.2, 0.25), -6, fade_out=0.2)

def s_marmot():
    rng, n = R(527), ns(1.0)
    m = ns(0.3)
    tm = np.linspace(0, 1, m)
    f = 2550 + 520 * smooth(0, 0.1, tm) - 380 * smooth(0.55, 1.0, tm)
    e = smooth(0, 0.03, tm) * smooth(1, 0.75, tm)
    w = sweep(f, e, ((1, 1.0), (2, 0.15), (3, 0.04))) + 0.15 * e * noise(rng, m, lo=2000, hi=4000) * smooth(0.2, 0, tm)
    x = np.zeros(n)
    put_at(x, 0, w, wrap=False)
    put_at(x, ns(0.32), 0.18 * sfilt(w, hi=2500), wrap=False)
    return finish(reverb(x, rng, 0.8, 0.25), -6, fade_out=0.25)

def s_click():
    rng, n = R(528), ns(0.06)
    x = decay(n, 1500, 0.005) + 0.35 * decay(n, 3100, 0.0025) + 0.25 * burst(rng, n, 0.0008, lo=2000, hi=7000)
    return finish(x, -9, fade_in=0.0005, fade_out=0.02)

def s_chime():
    n = ns(1.6)
    x, i = bell(n, 1046.5, 1.0, 0.9), ns(0.09)
    x[i:] += bell(n - i, 1568.0, 0.6, 0.8)
    return finish(reverb(x, R(529), 0.7, 0.2), -6, fade_out=0.3)

def make_sounds():
    snd = {"wind_loop": s_wind(), "rain_loop": s_rain(), "birds_loop": s_birds(),
           "crickets_loop": s_crickets(), "stream_loop": s_stream(), "fire_loop": s_fire()}
    for k, name in enumerate(("grass", "rock", "snow", "wood")):
        for v in (1, 2):
            snd["step_%s_%d" % (name, v)] = s_step(k, v - 1)
    snd.update(door_open=s_door_open(), door_close=s_door_close(), shutter=s_shutter(), summon=s_summon(True),
               unsummon=s_summon(False), thunder=s_thunder(), owl=s_owl(), marmot=s_marmot(),
               ui_click=s_click(), chime=s_chime())
    return snd

# ---- Output ----------------------------------------------------------------

def to_u8(a):
    return a if a.dtype == np.uint8 else np.clip(np.round(a * 255), 0, 255).astype(np.uint8)

def save_png(path, arr):
    Image.fromarray(to_u8(arr), "RGBA").save(path, optimize=False, compress_level=9)

def write_preview(out, imgs):
    """Upscaled (nearest) sheets of every atlas over a checkerboard, plus one combined contact sheet."""
    os.makedirs(out, exist_ok=True)
    tinted = imgs["foliage"].copy()
    for i in list(range(16)) + [30, 31]:
        r, c = (i // 8) * 64, (i % 8) * 64
        tinted[r:r + 64, c:c + 64, :3] *= C(0.24, 0.44, 0.26) if 4 <= i < 8 else C(0.42, 0.62, 0.24)
    sheets = [("foliage", imgs["foliage"], 2), ("foliage_tinted", tinted, 2), ("materials", imgs["materials"], 2),
              ("icons", imgs["icons"], 4), ("terrain", imgs["terrain"], 4), ("moon", imgs["moon"], 4),
              ("font", imgs["font"], 4)]
    ims = {}
    for name, a, k in sheets:
        a = to_u8(a).astype(float) / 255
        h, w = a.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w]
        bg = np.where((((yy // 8) + (xx // 8)) % 2 == 0)[..., None], C(0.62, 0.72, 0.82), C(0.5, 0.6, 0.7))
        ims[name] = Image.fromarray(to_u8(a[..., :3] * a[..., 3:] + bg * (1 - a[..., 3:]))).resize((w * k, h * k), Image.NEAREST)
        ims[name].save(os.path.join(out, name + ".png"))
    sheet = Image.new("RGB", (2088, 2600), (40, 40, 46))
    for name, xy in (("foliage", (8, 8)), ("materials", (1056, 8)), ("foliage_tinted", (8, 1048)),
                     ("icons", (1056, 1048)), ("terrain", (8, 2096)), ("moon", (1056, 2080)), ("font", (1576, 2080))):
        sheet.paste(ims[name], xy)
    sheet.save(os.path.join(out, "contact_sheet.png"))

def main():
    for d in (TEX_DIR, SND_DIR, FONT_DIR):
        os.makedirs(d, exist_ok=True)
    imgs = {"foliage": make_foliage(), "terrain": make_terrain(), "materials": make_materials(),
            "moon": make_moon(), "icons": make_icons(), "font": make_font()}
    for name in ("foliage", "terrain", "materials", "moon", "icons"):
        save_png(os.path.join(TEX_DIR, name + ".png"), imgs[name])
    save_png(os.path.join(FONT_DIR, "font8x8.png"), imgs["font"])
    for name, x in make_sounds().items():
        write_wav(name, x)
    if "--preview" in sys.argv:
        write_preview(sys.argv[sys.argv.index("--preview") + 1], imgs)
    print("assets written")

if __name__ == "__main__":
    main()
