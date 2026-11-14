#!/usr/bin/env python3
"""heldquad "even steps" check, from an xrmock trace made with MOCK_XR_TRACE_POSE=5.

A game picture is shown on two 120 Hz refreshes. Under a steady head turn, scenery must move in
the room by the same step from refresh 1 to refresh 2 of a picture as from refresh 2 to the next
picture's first refresh: that is what a picture drawn fresh every refresh would do. Held rigid
(AZAHARFRAME_IMMERSIVE_HOLD=rigid) the step is 0 and then 2 x s x the turn of a refresh (s, the
magnified picture's swim, is 0 only at gain 1), so the eye sees it twice; held as the game would
draw it now (warp) the steps are even.

What it needs: the trace (each endFrame's display time and head yaw, the top screen's quad pose
and size per refresh, the swapchain releases that say which picture is on view) and the gains
from the emulator's log line ("gain 0.50, yaw gain 0.72"): the yaw gain is --gain-x, the pitch
gain --gain-y. Mode 3 (the quad on its eye), MOCK_XR_TURN (a yaw), the screen level (pitch 0 and
no Immersive Offset), steady pacing (each picture on 2 refreshes). Nothing of the game is
modelled: each picture's camera is the one before it turned by gain_x x the head's turn between
their first refreshes.

For 15 picture points (the middle, 20 and 40 degrees off to both sides, in the middle row and in
rows 15 degrees up and down) it follows the scenery a picture shows there into the next picture
and compares the two steps. PASS: every step pair within --tol (0.01) degrees plus --rel (10 %)
of the step. The share is needed: scenery drawn fresh every refresh does not step evenly either
at a gain other than 1, as its swim changes while it crosses the screen (a second-order term,
about (1 - gain^2) sin(2e) x the refresh's turn^2: 0.01 degrees at 100 degrees a second, 40
degrees off, gain 1.4), while rigid's error is first order (a step of 0 and then twice the
swim: 100 % of the step).

  hold.py TRACE --gain-x 0.72 --gain-y 0.50 [--eye left|right] [--ipd 0.064] [--neck 0]
  hold.py TRACE --log azahar_log.txt [--size 1.672 --scale-x -30 --scale-y 0] [--from S] [--to S]
          (r31: the gains from the log's last "immersive mode N on" line, which prints them to 2
          decimals only; with the placement's size and the scales, the exact 1 / size / (1 + scale)
          when that rounds to the logged values, as SolveHeadLocked gives with fit 1 and Detect
          Game FOV settled. --from defaults to 1 s after that line's time.)
  hold.py --selftest      (synthetic traces: rigid fails at gain 0.6 and passes at gain 1;
                           warp passes at both; imglag: on a compositor that shows an image a
                           refresh late, an app anchored to the picture it sends fails and one
                           anchored to the picture on view passes, with irregular cadences too)

imglag (moved here from the r30/r31 handoff scripts): with MOCK_XR_IMAGE_LAG=1 the mock shows the
image an endFrame carries one refresh late and names the picture on view in each quad (img=), so
the picture a refresh shows is taken from it. Run it with --camera warp on such a trace: an app at
image lag 0 fails (each picture's first refresh carries the next one's anchor), at lag 1 passes.
"""
import argparse
import math
import re
import sys

QUAD = re.compile(
    r"quad\[sc(\d+):\d+x\d+ at \d+,\d+ eye=(\d) sp=(\w+) size=([-\d.]+)x([-\d.]+) "
    r"pos=([-\d.]+),([-\d.]+),([-\d.]+) q=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)"
    r"(?: img=(\d+))?\]")
END = re.compile(r"endFrame layers=\d+ t=([-\d.]+) yaw=([-\d.]+)(.*)$")
REL = re.compile(r"release sc(\d+)")


def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2 * (y * vz - z * vy)
    ty = 2 * (z * vx - x * vz)
    tz = 2 * (x * vy - y * vx)
    return (vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz),
            vz + w * tz + (x * ty - y * tx))


def yawq(deg):
    a = math.radians(deg) / 2
    return (0.0, math.sin(a), 0.0, math.cos(a))


def add(a, b):
    return tuple(p + q for p, q in zip(a, b))


def sub(a, b):
    return tuple(p - q for p, q in zip(a, b))


def scale(a, k):
    return tuple(p * k for p in a)


def angles(d):
    """Room yaw (positive left) and elevation of a direction, degrees."""
    x, y, z = d
    return (math.degrees(math.atan2(-x, -z)), math.degrees(math.atan2(y, math.hypot(x, z))))


def eye_at(yaw_deg, eye, ipd, neck):
    """The mock's eye in LOCAL space (xrmock headAt, LOCAL at the head's start)."""
    a = math.radians(yaw_deg)
    head = (-neck * math.sin(a), 0.0, neck - neck * math.cos(a))
    return add(head, qrot(yawq(yaw_deg), (-ipd / 2 if eye == 1 else ipd / 2, 0.0, 0.0)))


def parse(path, eye, t_from=None, t_to=None):
    """Refreshes: (t, head yaw, picture number, quad (pos, q, w, h)) for the top screen's eye."""
    releases = {}
    rows = []
    heads = []  # r31: every refresh's (t, head yaw), for the steady-turn window
    for line in open(path, errors="replace"):
        m = REL.search(line)
        if m and "endFrame" not in line:
            releases[int(m.group(1))] = releases.get(int(m.group(1)), 0) + 1
            continue
        m = END.search(line)
        if not m:
            continue
        t, yaw, rest = float(m.group(1)), float(m.group(2)), m.group(3)
        if (t_from is not None and t < t_from) or (t_to is not None and t > t_to):
            continue
        heads.append((t, yaw))
        for q in QUAD.finditer(rest):
            # r31: the mock traces the space in lower case ("sp=local").
            if int(q.group(2)) != eye or q.group(3).lower() != "local":
                continue
            sc = int(q.group(1))
            quad = ((float(q.group(6)), float(q.group(7)), float(q.group(8))),
                    (float(q.group(9)), float(q.group(10)), float(q.group(11)), float(q.group(12))),
                    float(q.group(4)), float(q.group(5)))
            # imglag: with MOCK_XR_IMAGE_LAG=1 the mock names the picture on view (img=, the image
            # the endFrame before carried); else it is the last one released.
            pic = int(q.group(13)) if q.group(13) is not None else releases.get(sc, 0)
            rows.append((t, yaw, pic, quad, len(heads) - 1))
            break
    parse.heads = heads
    return rows


def steady(heads, k0, k1):
    """r31: the head turned at one speed over refreshes k0 to k1 (a picture is drawn some
    refreshes before it is shown, so its camera's turn is the display's only on a steady turn)."""
    if k0 < 1 or k1 >= len(heads):
        return False
    d = heads[k0][1] - heads[k0 - 1][1]
    dt = heads[k0][0] - heads[k0 - 1][0]
    for k in range(k0, k1 + 1):
        if abs(heads[k][1] - heads[k - 1][1] - d) > 1e-3 or abs(heads[k][0] - heads[k - 1][0] - dt) > 1e-4:
            return False
    return True


def wrap(deg):
    """An angle difference in (-180, 180] degrees (r31)."""
    return (deg + 180.0) % 360.0 - 180.0


def quad_yaw(r):
    x, y, z, w = r[3][1]
    return math.degrees(2 * math.atan2(y, w))


def camera_yaw(r, hold, gx):
    """r31: the head yaw a picture was drawn for, read from one refresh's quad (a level screen,
    no Immersive Offset). rigid: the quad faces it. warp: the quad is the eye's head-locked quad
    turned by A = M R M^-1, whose x axis turns by atan(-gain_x tan(gain_x x turn since)), so the
    turn since is read back from it."""
    if hold == "rigid":
        return quad_yaw(r)
    ang = math.radians(quad_yaw(r) - r[1])
    since = math.degrees(math.atan(-math.tan(ang) / gx)) / gx
    return r[1] - since


def check(rows, gx, gy, eye, ipd, neck, tol, rel=0.1, out=print, heads=None, before=10,
          camera=None, second=0.0):
    """The even-steps check on parsed refreshes. Returns (pairs checked, worst, failures)."""
    if not rows:
        out("no top-screen quads in LOCAL space in the trace (MOCK_XR_TRACE_POSE=5, mode 3?)")
        return 0, 0.0, 1
    # Group the refreshes by picture.
    pics = []
    for r in rows:
        if pics and pics[-1][0][2] == r[2]:
            pics[-1].append(r)
        else:
            pics.append([r])
    # The picture's base size and distance: the median over the trace (the held size changes
    # only to the second order).
    def eye_rel(r):
        return sub(r[3][0], eye_at(r[1], eye, ipd, neck))
    ds = sorted(math.sqrt(sum(c * c for c in eye_rel(r))) for r in rows)
    ws = sorted(r[3][2] for r in rows)
    hs = sorted(r[3][3] for r in rows)
    d, w, h = ds[len(ds) // 2], ws[len(ws) // 2], hs[len(hs) // 2]
    if camera == "warp":
        # r31: the picture's own size: a held quad is resized by the warp with the turn since its
        # picture (width x |A e_x| k, height x k, k = 1 / sqrt(sin^2 r / gain_x^2 + cos^2 r), r the
        # camera's turn since), so the median of the sizes seen is not it; undo that per refresh.
        bw, bh = [], []
        for r in rows:
            rr = math.radians(gx * (r[1] - camera_yaw(r, "warp", gx)))
            k = 1.0 / math.sqrt(math.sin(rr) ** 2 / gx ** 2 + math.cos(rr) ** 2)
            bw.append(r[3][2] / (math.hypot(math.cos(rr), math.sin(rr) * gx) * k))
            bh.append(r[3][3] / k)
        bw.sort()
        bh.sort()
        w, h = bw[len(bw) // 2], bh[len(bh) // 2]
    tx, ty = gx * w / 2 / d, gy * h / 2 / d
    # Picture points: screen angles (degrees across, up) as picture coordinates.
    pts = []
    for up in (0.0, 15.0, -15.0):
        for across in (0.0, 20.0, -20.0, 40.0, -40.0):
            u = math.tan(math.radians(across)) / (w / 2 / d)
            v = math.tan(math.radians(up)) / (h / 2 / d)
            if abs(u) <= 1.0 and abs(v) <= 1.0:
                pts.append((across, up, u, v))

    def where(r, u, v):
        pos, q, qw, qh = r[3]
        p = add(pos, qrot(q, (u * qw / 2, v * qh / 2, 0.0)))
        return angles(sub(p, eye_at(r[1], eye, ipd, neck)))

    # r31: each picture's camera from its quads (--camera rigid|warp): a picture is drawn some
    # whole number of refreshes before it is shown (4, sometimes 3 or 5, as the game's 59.83
    # pictures a second drift against the 120 Hz display), so the head's turn between two
    # pictures' first refreshes is not always their cameras' turn. Two checks of the reading
    # itself: the same camera on every refresh of a picture, and ages of whole refreshes.
    cams = {}
    if camera:
        period = None
        if heads and len(heads) > 1:
            period = sorted(heads[k][0] - heads[k - 1][0] for k in range(1, len(heads)))[len(heads) // 2]
        same_worst = 0.0
        ages = {}
        frac_worst = 0.0
        for p in pics:
            c = [camera_yaw(r, camera, gx) for r in p]
            cams[p[0][2]] = c[0]
            if len(c) > 1:
                same_worst = max(same_worst, max(c) - min(c))
            # The age: the refresh before the first whose head yaw is the camera's.
            if heads and period and steady(heads, p[0][4] - 16, p[0][4]):
                k0 = p[0][4]
                best = None
                for k in range(k0, max(k0 - 16, 0) - 1, -1):
                    dist = abs(heads[k][1] - c[0])
                    if best is None or dist < best[0]:
                        best = (dist, k0 - k)
                if best:
                    ages[best[1]] = ages.get(best[1], 0) + 1
                    frac_worst = max(frac_worst, best[0])
        out("cameras read from the quads (%s): the same on a picture's refreshes to %.4f degrees; "
            "ages in refreshes (steady turn) %s; worst distance of a camera from the head at a whole refresh %.4f "
            "degrees" % (camera, same_worst, dict(sorted(ages.items())), frac_worst))
        check.read = (same_worst, ages, frac_worst)
    # r31: each refresh against a fresh picture drawn for the head at that refresh (the camera
    # gain_x x the head, a head-locked quad of the base size on the eye): how far the scenery a
    # point of the held picture shows sits from where the fresh picture would put it. Rigid: s x
    # the turn since the picture; warp: ~0. Needs the cameras (--camera).
    if camera:
        dev = {}  # refresh of the picture (1, 2) -> worst, sum, n
        jump = [0.0, 0.0, 0]  # the error's change from refresh 1 to 2: worst, sum, n
        for p in pics:
            if len(p) < 2:  # r31: 2 refreshes at 60 pictures a second, 4 at 30
                continue
            errs = tuple({} for _ in p)
            for i, r in enumerate(p):
                psi = r[1]
                e = eye_at(psi, eye, ipd, neck)
                for across, up, u, v in pts:
                    ang = math.radians(gx * (cams[p[0][2]] - psi))
                    rx, ry, rz = u * tx, v * ty, -1.0
                    c, sn = math.cos(ang), math.sin(ang)
                    nx, nz = c * rx + sn * rz, -sn * rx + c * rz
                    if nz >= -1e-6:
                        continue
                    u2, v2 = nx / -nz / tx, ry / -nz / ty
                    fresh = add(e, qrot(yawq(psi), (u2 * w / 2, v2 * h / 2, -d)))
                    want = angles(sub(fresh, e))
                    got = where(r, u, v)
                    dd = math.hypot(wrap(got[0] - want[0]), got[1] - want[1])
                    errs[i][(across, up)] = (wrap(got[0] - want[0]), got[1] - want[1])
                    st = dev.setdefault(i + 1, [0.0, 0.0, 0])
                    st[0] = max(st[0], dd)
                    st[1] += dd
                    st[2] += 1
            for k in range(1, len(p)):
                for key, e1 in errs[k - 1].items():
                    e2 = errs[k].get(key)
                    if e2 is not None:
                        j = math.hypot(e2[0] - e1[0], e2[1] - e1[1])
                        jump[0] = max(jump[0], j)
                        jump[1] += j
                        jump[2] += 1
        out("against a fresh picture: the error's change from a picture's refresh 1 to 2 (what "
            "shows twice): worst %.4f, mean %.4f degrees (%d points)"
            % (jump[0], jump[1] / max(jump[2], 1), jump[2]))
        check.jump = jump
        for i in sorted(dev):
            out("against a fresh picture, refresh %d of a picture: worst %.4f, mean %.4f degrees "
                "(%d points)" % (i, dev[i][0], dev[i][1] / max(dev[i][2], 1), dev[i][2]))
        check.dev = dev
    pairs = fails = 0
    worst = 0.0
    stats = {}  # (across, up) -> [pairs, worst |s1 - s2|, sum |s1|, sum |s2|]
    for n in range(1, len(pics) - 1):
        a, b = pics[n], pics[n + 1]
        # r31: k refreshes a picture (2 at 60 pictures a second, 4 at 30), the same as the last
        if len(a) < 2 or len(b) < 1 or len(pics[n - 1]) != len(a):
            continue  # not steady pacing here
        # The head's turn between the two pictures' first refreshes (their cameras' turn over
        # gain_x); skip the ends of the sweep, where it turns back.
        turn = b[0][1] - a[0][1]
        step1, step2 = a[1][1] - a[0][1], b[0][1] - a[-1][1]
        if abs(step1 - step2) > 1e-3 or abs(turn) < 1e-4 or any(
                abs(a[i + 1][1] - a[i][1] - step1) > 1e-3 for i in range(len(a) - 1)):
            continue
        # r31: and steady from `before` refreshes before picture n's first refresh (its camera's
        # pose; the age is about 5 refreshes on the mock) to picture n+1's first.
        if heads is not None and not steady(heads, a[0][4] - before, b[0][4]):
            continue
        cam = gx * turn  # picture n+1's camera from picture n's, degrees (a yaw)
        if camera:
            cam = gx * (cams[b[0][2]] - cams[a[0][2]])
        for across, up, u, v in pts:
            # The scenery at (u, v) of picture n, in picture n+1.
            rx, ry, rz = u * tx, v * ty, -1.0
            c, s = math.cos(math.radians(-cam)), math.sin(math.radians(-cam))
            nx, nz = c * rx + s * rz, -s * rx + c * rz
            if nz >= -1e-6:
                continue
            u2, v2 = nx / -nz / tx, ry / -nz / ty
            if abs(u2) > 1.0 or abs(v2) > 1.0:
                continue
            ps = [where(r, u, v) for r in a]
            p3 = where(b[0], u2, v2)
            s2 = (wrap(p3[0] - ps[-1][0]), p3[1] - ps[-1][1])
            # every step within the picture against the step to the next one; the worst kept
            e, s1 = -1.0, None
            for i in range(len(ps) - 1):
                si = (wrap(ps[i + 1][0] - ps[i][0]), ps[i + 1][1] - ps[i][1])
                ei = math.hypot(si[0] - s2[0], si[1] - s2[1])
                if ei > e:
                    e, s1 = ei, si
            step = max(math.hypot(*s1), math.hypot(*s2))
            pairs += 1
            worst = max(worst, e)
            st = stats.setdefault((across, up), [0, 0.0, 0.0, 0.0])
            st[0] += 1
            st[1] = max(st[1], e)
            st[2] += math.hypot(*s1)
            st[3] += math.hypot(*s2)
            # r31: plus the second order a fresh picture has too: about 0.45 x (the refresh's turn
            # in radians)^2 (an ideal warp, synthetic: 0.034 degrees at 250 degrees a second),
            # where rigid's error is first order (2 x s x the turn).
            turn_r = math.radians(abs(step1))
            if e > tol + rel * step + second * math.degrees(turn_r * turn_r):
                fails += 1
                if fails <= 10:
                    out("FAIL picture %d at %+.0f, %+.0f degrees: steps %.4f, %.4f across and "
                        "%.4f, %.4f up (degrees)" % (a[0][2], across, up, s1[0], s2[0], s1[1], s2[1]))
    if stats:
        out("per picture point (degrees across, up): pairs, worst |step1 - step2|, mean step "
            "refresh 1 -> 2, mean step refresh 2 -> next picture (degrees)")
        for (across, up), st in sorted(stats.items(), key=lambda kv: (kv[0][1], kv[0][0])):
            out("  %+3.0f, %+3.0f: %4d  worst %.4f  steps %.4f / %.4f"
                % (across, up, st[0], st[1], st[2] / st[0], st[3] / st[0]))
    return pairs, worst, fails


def fit_gain(rows, camera, sx, sy, out=print):
    """r31 (Detect Game FOV Once / Always: the gain follows the game's measured camera and is not
    logged after the first line): the base gain at which each picture's camera, read from its
    quads, is the same on all its refreshes (warp only). The ages then landing on whole refreshes
    is the independent check. Returns (gain_x, gain_y)."""
    pics = {}
    for r in rows:
        pics.setdefault(r[2], []).append(r)
    pics = [p for p in pics.values() if len(p) >= 2]

    def cost(b):
        gx = min(max(b / (1.0 + sx / 100.0), 0.4), 2.0)
        sp = sorted(max(cs) - min(cs) for cs in
                    ([camera_yaw(r, camera, gx) for r in p] for p in pics))
        return sp[len(sp) // 2] if sp else 1e9

    best = min((cost(b / 200.0), b / 200.0) for b in range(80, 401))
    lo = best[1] - 0.005
    best = min((cost(lo + k / 10000.0), lo + k / 10000.0) for k in range(0, 101))
    b = best[1]
    gx = min(max(b / (1.0 + sx / 100.0), 0.4), 2.0)
    gy = min(max(b / (1.0 + sy / 100.0), 0.4), 2.0)
    out("gains fitted from the quads (each picture's camera the same on its refreshes, median "
        "spread %.4f degrees): base %.4f, across %.5f, up %.5f" % (best[0], b, gx, gy))
    return gx, gy


def gains_from_log(log, size=None, scale_x=None, scale_y=None, out=print):
    """(gain_x, gain_y, time of the line) from the log's last "immersive mode N on" line, made
    exact from the placement's size and the scales when they round to the logged values."""
    pat = re.compile(r"^\[\s*([\d.]+)\].*immersive mode \d on .*gain ([\d.]+), yaw gain ([\d.]+), "
                     r"size ([\d.]+)")
    last = None
    for line in open(log, errors="replace"):
        m = pat.search(line)
        if m:
            last = tuple(float(g) for g in m.groups())
    if last is None:
        return None
    t, g, gx, sz = last
    if size is not None:
        base = 1.0 / min(max(size, 0.6), 2.5)
        cx = min(max(base / (1.0 + (scale_x or 0) / 100.0), 0.4), 2.0)
        cy = min(max(base / (1.0 + (scale_y or 0) / 100.0), 0.4), 2.0)
        if abs(cx - gx) <= 0.0051 and abs(cy - g) <= 0.0051:
            out("gains: logged %.2f across, %.2f up; exact from size %.3f: %.5f, %.5f" % (gx, g, size, cx, cy))
            return cx, cy, t
        out("gains: logged %.2f across, %.2f up; size %.3f gives %.4f, %.4f, which do not round to "
            "them (the screen fitted or Detect Game FOV): the logged ones used" % (gx, g, size, cx, cy))
    else:
        out("gains: logged %.2f across, %.2f up (2 decimals)" % (gx, g))
    return gx, g, t


def synthetic(hold, gx, gy, speed=100.0, seconds=1.0, d=3.0, w=6.0, h=3.6, ipd=0.064, age=3,
              jitter=False, app_lag=0, comp_lag=0, cadence=None):
    """A trace of a level mode 3 screen on the left eye, as the app would submit it: pictures
    drawn every 2 refreshes with the head pose `age` refreshes before their first one, held rigid
    or warped (A = M R M^-1 for the camera's yaw since; the quad scaled back to its distance).
    imglag: `comp_lag` 1, the compositor shows the image the frame before carried (img= in the
    trace, as MOCK_XR_IMAGE_LAG=1); `app_lag`, the picture the app anchors the quad to (0 the
    newest, 1 the one the frame before carried). `cadence`: refreshes with a release (default
    every second one)."""
    lines = []
    hz = 120.0
    eye = 1
    poses = {}  # release count -> the head yaw it was drawn for
    n = 0
    sent = 0  # the picture the frame before carried
    for k in range(int(seconds * hz)):
        t = k / hz
        yaw = speed * t  # one way only: no turn-back in a second at 100 degrees a second
        if (k % 2 == 0) if cadence is None else cadence(k):
            lines.append("release sc3")
            n += 1
            # r31 jitter: every 7th picture drawn a refresh later (age one less), as on the Frame.
            poses[n] = speed * (k - age + (1 if jitter and (k // 2) % 7 == 3 else 0)) / hz
        if n == 0:
            continue
        anchor = sent if app_lag and sent else n
        shown = sent if comp_lag and sent else n
        sent = n
        pic_pose = poses[anchor]
        e = eye_at(yaw, eye, ipd, 0.0)
        # The head-locked quad in the head's frame (on the eye), turned into LOCAL.
        dpsi = yaw - pic_pose  # head turn since the picture, degrees
        if hold == "rigid":
            q = yawq(pic_pose)
            pos = add(e, qrot(q, (0.0, 0.0, -d)))
            sw, sh = w, h
        else:
            r = math.radians(gx * dpsi)  # the camera's turn, in the quad's frame
            # R = conj(cam_now) cam_then: a yaw by -r. A = M R M^-1.
            c, s = math.cos(-r), math.sin(-r)
            ax = (c, 0.0, -s * gx)          # A e_x
            cz = (s / gx * -d, 0.0, c * -d)  # A (0, 0, -d)
            k_ = d / math.hypot(cz[0], cz[2])
            ang = math.atan2(-ax[2], ax[0])  # x axis turned left by this
            q = yawq(yaw + math.degrees(ang))
            pos = add(e, qrot(yawq(yaw), scale(cz, k_)))
            sw, sh = w * math.hypot(ax[0], ax[2]) * k_, h * k_
        lines.append("endFrame layers=1 t=%.6f yaw=%.6f quad[sc3:1x1 at 0,0 eye=%d sp=local "
                     "size=%.5fx%.5f pos=%.5f,%.5f,%.5f q=%.5f,%.5f,%.5f,%.5f%s]"
                     % (t, yaw, eye, sw, sh, pos[0], pos[1], pos[2], q[0], q[1], q[2], q[3],
                        " img=%d" % shown if comp_lag else ""))
    return lines


def selftest():
    import tempfile
    import os
    ok = True
    cases = [(h, g, w, False, None) for h, g, w in (
        ("rigid", 0.6, False), ("rigid", 1.0, True), ("warp", 0.6, True), ("warp", 1.0, True),
        ("warp", 1.4, True), ("rigid", 1.4, False))]
    # r31: ages that change by a refresh, cameras read from the quads (assumed ages fail warp).
    cases += [("warp", 0.6, True, True, "warp"), ("warp", 1.4, True, True, "warp"),
              ("rigid", 0.6, False, True, "rigid"), ("rigid", 1.0, True, True, "rigid"),
              ("warp", 0.6, False, True, None)]
    cases = [c + (0, 0) for c in cases]
    # imglag: a compositor that shows the image a refresh late (MOCK_XR_IMAGE_LAG=1): an app that
    # anchors to the picture it sends (lag 0) fails, one anchored to the picture on view passes,
    # at gain 1 too (rigid or warp: the anchor is a whole picture off).
    cases += [("warp", 0.6, False, False, "warp", 0, 1), ("warp", 1.0, False, False, "warp", 0, 1),
              ("warp", 0.6, True, False, "warp", 1, 1), ("warp", 1.4, True, False, "warp", 1, 1),
              ("warp", 0.6, True, True, "warp", 1, 1), ("rigid", 1.0, True, False, "rigid", 1, 1),
              ("warp", 0.6, False, False, "warp", 1, 0)]
    cases = [c + (None,) for c in cases]
    # imglag: irregular cadence (every 11th picture missing, so the one before it is on view for
    # 4 refreshes): the pairing by the image sent holds; lag 0 still fails.
    irregular = lambda k: k % 2 == 0 and k % 22 != 10  # noqa: E731
    cases += [("warp", 0.6, True, False, "warp", 1, 1, irregular),
              ("warp", 0.6, False, False, "warp", 0, 1, irregular)]
    for hold, g, want, jitter, camera, app_lag, comp_lag, cadence in cases:
        fd, path = tempfile.mkstemp(suffix=".trace", dir=os.path.dirname(os.path.abspath(__file__)))
        with os.fdopen(fd, "w") as f:
            f.write("\n".join(synthetic(hold, g, g, jitter=jitter, app_lag=app_lag,
                                        comp_lag=comp_lag, cadence=cadence)) + "\n")
        rows = parse(path, 1)
        pairs, worst, fails = check(rows, g, g, 1, 0.064, 0.0, 0.01, 0.1, out=lambda s: None,
                                    heads=parse.heads, before=0 if camera else 10, camera=camera)
        os.remove(path)
        got = pairs > 0 and fails == 0
        jump = getattr(check, "jump", None)
        if camera == "warp" and jump is not None:
            got = got and jump[0] < 0.01  # as main(): the held refresh against a fresh picture
        check.jump = None
        print("selftest %-5s gain %.1f%s%s%s: %d step pairs, worst %.4f degrees -> %s (want %s)"
              % (hold, g, ", ages jitter" if jitter else "", ", cameras read" if camera else "",
                 (", app lag %d on compositor lag %d" % (app_lag, comp_lag) if comp_lag or app_lag
                  else "") + (", irregular cadence" if cadence else ""), pairs, worst, "PASS" if got else "FAIL", "PASS" if want else "FAIL"))
        ok &= got == want
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("trace", nargs="?")
    ap.add_argument("--gain-x", type=float)
    ap.add_argument("--gain-y", type=float)
    ap.add_argument("--eye", choices=("left", "right"), default="left")
    ap.add_argument("--ipd", type=float, default=0.064)
    ap.add_argument("--neck", type=float, default=0.0)
    ap.add_argument("--tol", type=float, default=0.01)
    ap.add_argument("--rel", type=float, default=0.1)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--camera", choices=("rigid", "warp"),
                    help="read each picture's camera from its quads, as held this way (r31)")
    ap.add_argument("--second", type=float, default=0.6,
                    help="second-order allowance x (turn per refresh, rad)^2, degrees (r31)")
    ap.add_argument("--fit-gain", action="store_true",
                    help="fit the gains from the warp's quads (Detect Game FOV Once/Always, r31)")
    ap.add_argument("--before", type=int, default=None,
                    help="refreshes of steady turn needed before a picture (its age), r31")
    ap.add_argument("--log", help="the emulator log: the gains from it (r31)")
    ap.add_argument("--size", type=float, help="the placement's size, for exact gains")
    ap.add_argument("--scale-x", type=float, default=0.0)
    ap.add_argument("--scale-y", type=float, default=0.0)
    ap.add_argument("--from", dest="t_from", type=float, help="trace time to start at, s")
    ap.add_argument("--to", dest="t_to", type=float, help="trace time to stop at, s")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if a.trace and a.log and (a.gain_x is None or a.gain_y is None):
        got = gains_from_log(a.log, a.size, a.scale_x, a.scale_y)
        if got is None:
            print("no immersive mode line in %s" % a.log)
            print("RESULT: FAIL")
            return 1
        a.gain_x, a.gain_y, t_on = got
        if a.t_from is None:
            a.t_from = t_on + 1.0
    if not a.trace or a.gain_x is None or a.gain_y is None:
        ap.error("a trace and --gain-x and --gain-y (or --log) are needed")
    eye = 1 if a.eye == "left" else 2
    if a.fit_gain:
        rows0 = parse(a.trace, eye, a.t_from, a.t_to)
        a.gain_x, a.gain_y = fit_gain(rows0, a.camera or "warp", a.scale_x, a.scale_y)
    print("gain across %.5f, up %.5f; trace from %s s to %s s" % (a.gain_x, a.gain_y, a.t_from, a.t_to))
    rows = parse(a.trace, eye, a.t_from, a.t_to)
    pairs, worst, fails = check(rows, a.gain_x, a.gain_y, eye, a.ipd, a.neck, a.tol, a.rel,
                                heads=parse.heads,
                                before=a.before if a.before is not None else (0 if a.camera else 10),
                                camera=a.camera, second=a.second)
    print("%d step pairs, worst difference %.4f degrees, %d over %.3f + %.0f %% of the step"
          % (pairs, worst, fails, a.tol, 100 * a.rel))
    ok = pairs > 0 and fails == 0
    jump = getattr(check, "jump", None)
    if a.camera == "warp" and jump is not None:
        print("held refresh against a fresh picture: worst change %.4f degrees (limit 0.01)" % jump[0])
        ok = ok and jump[0] < 0.01
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
