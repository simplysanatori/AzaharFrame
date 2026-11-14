#!/usr/bin/env python3
"""projscreen sharpness check: the top screen drawn into the projection layer, against an ideal
resample of the same picture onto the same eye pixels.

  proj_sharp.py EYES.png PICTURE.png [--eye left|right] [--gap 8] [--out DIR]

EYES.png is the projection layer's swapchain image (both eyes side by side, as xrmock's
MOCK_XR_DUMP_FRAME writes it) drawn with AZAHARFRAME_XR_PROJ_MARKS=1; PICTURE.png the top screen's
swapchain image of the same paused game moment (packed: the left eye in the upper half, the right
one kXrPackedGap below). The 15 magenta dots (frame_xr/vis/proj_screen.h kProjMarks) give the
homography from the picture to the eye's pixels; through it the picture is resampled onto those
pixels as well as a resample can be (8 x 8 points a pixel, each a bilinear tap, averaged in linear
light: a box filter over the pixel) and, for contrast, with one bilinear tap a pixel (no filter for
the minification). The eye buffer is compared with both, away from the dots: mean difference
(0..255), PSNR, and the gradient energy (sharpness) as a share of the ideal's. It says how far the
app's own filter (footprint taps, proj_screen.h) is from ideal on its grid; how much the eye
buffer's density itself costs against a quad (which SteamVR resamples once, at the display) is
the minification it reports: picture texels per eye pixel at the screen's middle.
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image

MARKS = [(-0.5, 0.5), (-0.25, 0.5), (0.0, 0.5), (0.25, 0.5), (0.5, 0.5),
         (-0.5, 0.0), (-0.25, 0.0), (0.0, 0.0), (0.25, 0.0), (0.5, 0.0),
         (-0.5, -0.5), (-0.25, -0.5), (0.0, -0.5), (0.25, -0.5), (0.5, -0.5)]


def to_linear(c):
    c = c / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055) * 255.0


def find_marks(img):
    """The dots' weighted centres (x, y in pixels, pixel centres at +0.5), as xrmock finds them."""
    r, g, b = img[..., 0].astype(int), img[..., 1].astype(int), img[..., 2].astype(int)
    core = (r >= 128) & (g <= 8) & (np.abs(r - b) <= 8)
    mag = (g <= 8) & (np.abs(r - b) <= 12)
    lin = to_linear(img[..., 0].astype(np.float64))
    seen = np.zeros(core.shape, bool)
    out = []
    ys, xs = np.nonzero(core)
    h, w = core.shape
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]:
            continue
        stack = [(y0, x0)]
        seen[y0, x0] = True
        comp = []
        while stack:
            y, x = stack.pop()
            comp.append((y, x))
            for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= yy < h and 0 <= xx < w and core[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = True
                    stack.append((yy, xx))
        if len(comp) > 80:
            continue
        by, bx = max(comp, key=lambda p: r[p])
        y_lo, y_hi, x_lo, x_hi = max(by - 4, 0), min(by + 5, h), max(bx - 4, 0), min(bx + 5, w)
        gy_, gx_ = np.mgrid[y_lo:y_hi, x_lo:x_hi]
        disc = (gx_ - bx) ** 2 + (gy_ - by) ** 2 <= 20
        if mag[y_lo:y_hi, x_lo:x_hi][disc].mean() < 0.9:
            continue  # magenta in the picture, not ringed by the dot's disc (as xrmock)
        wgt = lin[y_lo:y_hi, x_lo:x_hi] * mag[y_lo:y_hi, x_lo:x_hi]
        if wgt.sum() <= 0:
            continue
        gy, gx = np.mgrid[y_lo:y_hi, x_lo:x_hi]
        out.append(((wgt * (gx + 0.5)).sum() / wgt.sum(), (wgt * (gy + 0.5)).sum() / wgt.sum()))
    return out


def match(pts):
    if len(pts) != 15:
        return None
    by_y = sorted(pts, key=lambda p: p[1])
    rows = [sorted(by_y[i * 5:(i + 1) * 5]) for i in range(3)]
    return [p for row in rows for p in row]


def homography(src, dst):
    a, rhs = [], []
    for (u, v), (x, y) in zip(src, dst):
        a.append([u, v, 1, 0, 0, 0, -x * u, -x * v])
        rhs.append(x)
        a.append([0, 0, 0, u, v, 1, -y * u, -y * v])
        rhs.append(y)
    h, *_ = np.linalg.lstsq(np.array(a, float), np.array(rhs, float), rcond=None)
    return np.array([[h[0], h[1], h[2]], [h[3], h[4], h[5]], [h[6], h[7], 1.0]])


def bilinear(tex, s, t):
    """tex (H, W, 3) linear; s, t texel coordinates (pixel centres at +0.5), clamped."""
    h, w, _ = tex.shape
    x = np.clip(s - 0.5, 0, w - 1)
    y = np.clip(t - 0.5, 0, h - 1)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    return ((tex[y0, x0] * (1 - fx) + tex[y0, x1] * fx) * (1 - fy) +
            (tex[y1, x0] * (1 - fx) + tex[y1, x1] * fx) * fy)


def grad_energy(img):
    gx = np.diff(img, axis=1)[:-1]
    gy = np.diff(img, axis=0)[:, :-1]
    return float(np.sqrt(gx ** 2 + gy ** 2).mean())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("eyes")
    ap.add_argument("picture")
    ap.add_argument("--eye", choices=("left", "right"), default="left")
    ap.add_argument("--gap", type=int, default=8)
    ap.add_argument("--out")
    ap.add_argument("--ss", type=int, default=8, help="points a pixel for the ideal (per axis)")
    a = ap.parse_args()
    eyes = np.asarray(Image.open(a.eyes).convert("RGB"))
    ew = eyes.shape[1] // 2
    eye_img = eyes[:, :ew] if a.eye == "left" else eyes[:, ew:]
    pic = np.asarray(Image.open(a.picture).convert("RGB"))
    half = (pic.shape[0] - a.gap) // 2
    pic_eye = pic[:half] if a.eye == "left" else pic[half + a.gap:half + a.gap + half]
    tex = to_linear(pic_eye.astype(np.float64))
    th, tw, _ = tex.shape
    pts = match(find_marks(eye_img))
    if pts is None:
        print("the 15 dots were not found in the eye image")
        print("RESULT: FAIL")
        return 1
    hm = homography(MARKS, pts)
    hinv = np.linalg.inv(hm)
    # The screen's region on the eye: where the inverse lands inside -1..1, away from the dots
    # (6 pixels) and the edges (3 pixels).
    eh = eye_img.shape[0]
    gy, gx = np.mgrid[0:eh, 0:ew].astype(np.float64)

    def to_uv(x, y):
        d = hinv[2, 0] * x + hinv[2, 1] * y + hinv[2, 2]
        return ((hinv[0, 0] * x + hinv[0, 1] * y + hinv[0, 2]) / d,
                (hinv[1, 0] * x + hinv[1, 1] * y + hinv[1, 2]) / d)

    u, v = to_uv(gx + 0.5, gy + 0.5)
    inside = (np.abs(u) < 0.97) & (np.abs(v) < 0.97)
    for (mx, my) in pts:
        inside &= (gx + 0.5 - mx) ** 2 + (gy + 0.5 - my) ** 2 > 36.0
    ys, xs = np.nonzero(inside)
    # Minification at the middle: texels per eye pixel across and up (the Jacobian of the map).
    cx, cy = pts[7]
    du = to_uv(cx + 1, cy)[0] - to_uv(cx, cy)[0]
    dv = to_uv(cx, cy + 1)[1] - to_uv(cx, cy)[1]
    mx_, my_ = abs(du) * tw / 2, abs(dv) * th / 2
    # The ideal: ss x ss points a pixel, each bilinear, averaged in linear light.
    acc = np.zeros((len(ys), 3))
    n = a.ss
    for j in range(n):
        for i in range(n):
            uu, vv = to_uv(xs + (i + 0.5) / n, ys + (j + 0.5) / n)
            acc += bilinear(tex, (uu + 1) / 2 * tw, (1 - vv) / 2 * th)
    ideal = to_srgb(acc / (n * n))
    uu, vv = to_uv(xs + 0.5, ys + 0.5)
    plain = to_srgb(bilinear(tex, (uu + 1) / 2 * tw, (1 - vv) / 2 * th))
    ours = eye_img[ys, xs].astype(np.float64)

    def compare(ref):
        d = np.abs(ours - ref)
        mse = ((ours - ref) ** 2).mean()
        return d.mean(), 10 * math.log10(255.0 ** 2 / max(mse, 1e-9))

    def put(vals):
        im = np.zeros((eh, ew, 3))
        im[ys, xs] = vals
        return im

    ideal_im, plain_im, ours_im = put(ideal), put(plain), put(ours)
    box = inside[ys.min():ys.max() + 1, xs.min():xs.max() + 1]

    def sharp(im):
        return grad_energy(im[ys.min():ys.max() + 1, xs.min():xs.max() + 1].mean(axis=2) * box)

    e_ideal, e_ours, e_plain = sharp(ideal_im), sharp(ours_im), sharp(plain_im)
    print("screen on the %s eye: %d pixels compared; minification at the middle %.2f x %.2f picture "
          "texels per eye pixel (picture %dx%d per eye)" % (a.eye, len(ys), mx_, my_, tw, th))
    for name, ref in (("ideal (box filter, %dx%d points)" % (n, n), ideal),
                      ("one bilinear tap a pixel", plain)):
        md, psnr = compare(ref)
        print("eye buffer against %s: mean difference %.2f / 255, PSNR %.1f dB" % (name, md, psnr))
    print("gradient energy (sharpness): eye buffer %.3f, ideal %.3f (%.0f %%), one tap %.3f (%.0f %%)"
          % (e_ours, e_ideal, 100 * e_ours / e_ideal, e_plain, 100 * e_plain / e_ideal))
    if a.out:
        os.makedirs(a.out, exist_ok=True)
        cxi, cyi = int(cx), int(cy)
        r = 128
        crops = []
        for im in (ours_im, ideal_im, plain_im):
            crops.append(np.clip(im[cyi - r:cyi + r, cxi - r:cxi + r], 0, 255).astype(np.uint8))
        # The picture itself over the same field, at its own resolution, scaled to the crop.
        u0, v0 = to_uv(cxi - r, cyi - r)
        u1, v1 = to_uv(cxi + r, cyi + r)
        s0, s1 = int((u0 + 1) / 2 * tw), int((u1 + 1) / 2 * tw)
        t0, t1 = int((1 - v0) / 2 * th), int((1 - v1) / 2 * th)
        native = Image.fromarray(pic_eye[min(t0, t1):max(t0, t1), min(s0, s1):max(s0, s1)]).resize(
            (2 * r, 2 * r), Image.LANCZOS)
        crops.append(np.asarray(native))
        strip = np.concatenate([np.kron(c, np.ones((2, 2, 1), np.uint8)) for c in crops], axis=1)
        path = os.path.join(a.out, "sharp-%s.png" % a.eye)
        Image.fromarray(strip).save(path)
        print("crops (2x, left to right: eye buffer, ideal, one tap, the picture's own texels): %s" % path)
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
