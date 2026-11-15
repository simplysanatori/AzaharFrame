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
          fovaxis: [--margin-x 4 --margin-y 4 --scale-mode stretch|fov]: the Render Margin
          levels per axis and the Scale Mode, for the split and the caps against warping
          (Layout::SplitScales: the scales' part of a gain within 0.6 to 1.6, Stretch's two
          axes within 1.5 of each other); stretch with 4/4 is every build before. Render
          Margin applies with Stretch only (fov: the scales alone). A "Fill View:" line after
          the "on" line gives the size, the scales and the mode.
  hold.py --selftest      (synthetic traces: rigid fails at gain 0.6 and passes at gain 1;
                           warp passes at both; imglag: on a compositor that shows an image a
                           refresh late, an app anchored to the picture it sends fails and one
                           anchored to the picture on view passes, with irregular cadences too)

imglag (moved here from the r30/r31 handoff scripts): with MOCK_XR_IMAGE_LAG=1 the mock shows the
image an endFrame carries one refresh late and names the picture on view in each quad (img=), so
the picture a refresh shows is taken from it. Run it with --camera warp on such a trace: an app at
image lag 0 fails (each picture's first refresh carries the next one's anchor), at lag 1 passes.

fovscale, the roll check (--roll, on a trace made with MOCK_XR_ROLL, MOCK_XR_SWAY=0 and no turn):
with Horizontal and Vertical Scale unequal, a picture held on a later refresh is the game camera's
roll since (1:1 with the head) through M = diag(1 / gain_x, 1 / gain_y). With the scales as Field of
View the gains are equal, so it is a plain rotation: the held quad keeps its width over height on
every refresh, its roll in the room is the picture's (the same on each of its refreshes), and over
the run the quads roll as far as the head. In Stretch the gains differ and the held quad is the
rectangle nearest a shear: its width over height changes with the roll since the picture (the
control, which must fail). PASS: width over height within its median on
every refresh (--shape-tol, 0.02 %), a picture's room roll the same on its refreshes
(--spread-tol, 0.005 degrees), and the quads' room roll following the head's 1:1 (a least-squares
slope against the head's roll the pictures' age before, within 2 %). Roll fast (MOCK_XR_ROLL=120,20):
the shear grows with the roll since the picture.

  hold.py TRACE --roll [--eye left|right]
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
ROLL = re.compile(r"^ roll=([-\d.]+)")  # fovscale: MOCK_XR_ROLL
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
    parse.rolls = []  # fovscale: every refresh's head roll (MOCK_XR_ROLL), degrees
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
        rm = ROLL.match(rest)
        parse.rolls.append(float(rm.group(1)) if rm else 0.0)
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
    # imglag: the pictures the step check leaves out, counted (irregular paths are not judged).
    skipped = {"on one refresh": 0, "irregular pacing": 0, "turning back or unsteady": 0}
    for n in range(1, len(pics) - 1):
        a, b = pics[n], pics[n + 1]
        # r31: k refreshes a picture (2 at 60 pictures a second, 4 at 30), the same as the last
        if len(a) < 2:
            skipped["on one refresh"] += 1
            continue
        if len(b) < 1 or len(pics[n - 1]) != len(a):
            skipped["irregular pacing"] += 1
            continue  # not steady pacing here
        # The head's turn between the two pictures' first refreshes (their cameras' turn over
        # gain_x); skip the ends of the sweep, where it turns back.
        turn = b[0][1] - a[0][1]
        step1, step2 = a[1][1] - a[0][1], b[0][1] - a[-1][1]
        if abs(step1 - step2) > 1e-3 or abs(turn) < 1e-4 or any(
                abs(a[i + 1][1] - a[i][1] - step1) > 1e-3 for i in range(len(a) - 1)):
            skipped["turning back or unsteady"] += 1
            continue
        # r31: and steady from `before` refreshes before picture n's first refresh (its camera's
        # pose; the age is about 5 refreshes on the mock) to picture n+1's first.
        if heads is not None and not steady(heads, a[0][4] - before, b[0][4]):
            skipped["turning back or unsteady"] += 1
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
    out("pictures checked %d of %d; left out: %s" % (
        max(0, len(pics) - 2 - sum(skipped.values())), len(pics),
        ", ".join("%d %s" % (v, k) for k, v in skipped.items())))
    check.skipped = skipped
    if stats:
        out("per picture point (degrees across, up): pairs, worst |step1 - step2|, mean step "
            "refresh 1 -> 2, mean step refresh 2 -> next picture (degrees)")
        for (across, up), st in sorted(stats.items(), key=lambda kv: (kv[0][1], kv[0][0])):
            out("  %+3.0f, %+3.0f: %4d  worst %.4f  steps %.4f / %.4f"
                % (across, up, st[0], st[1], st[2] / st[0], st[3] / st[0]))
    return pairs, worst, fails


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qconj(q):
    return (-q[0], -q[1], -q[2], q[3])


def rollq(deg):
    a = math.radians(deg) / 2
    return (0.0, 0.0, math.sin(a), math.cos(a))


def check_roll(rows, rolls, shape_tol=0.0002, spread_tol=0.005, out=print):
    """fovscale: the roll check (module doc). rows from parse(), rolls: parse.rolls (each row's
    refresh index is r[4]). Returns (ok, worst shape change, worst picture roll spread, quads'
    roll range over the head's)."""
    if not rows:
        out("no top-screen quads in LOCAL space in the trace (MOCK_XR_TRACE_POSE=5, mode 3?)")
        return False, 0.0, 0.0, 0.0
    shapes = []
    room = []  # (picture, the quad's roll in the room)
    for r in rows:
        head = qmul(yawq(r[1]), rollq(rolls[r[4]]))
        rel = qmul(qconj(head), r[3][1])
        ax = qrot(rel, (1.0, 0.0, 0.0))
        room.append((r[2], rolls[r[4]] + math.degrees(math.atan2(ax[1], ax[0]))))
        shapes.append(r[3][2] / r[3][3])
    med = sorted(shapes)[len(shapes) // 2]
    worst_shape = max(abs(x / med - 1.0) for x in shapes)
    by_pic = {}
    for pic, ang in room:
        by_pic.setdefault(pic, []).append(ang)
    spread = max((max(a) - min(a)) for a in by_pic.values())
    head_range = max(rolls) - min(rolls) if rolls else 0.0
    # 1:1: the quads' room roll against the head's some refreshes before (the pictures' age, the
    # lag with the least scatter), as a least-squares slope.
    best = None
    for lag in range(0, 17):
        xs, ys = [], []
        for r, (_, ang) in zip(rows, room):
            if r[4] - lag >= 0:
                xs.append(rolls[r[4] - lag])
                ys.append(ang)
        if len(xs) < 10:
            continue
        mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
        sxx = sum((x - mx) ** 2 for x in xs)
        if sxx <= 0:
            continue
        slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
        res = math.sqrt(sum((y - my - slope * (x - mx)) ** 2 for x, y in zip(xs, ys)) / len(xs))
        if best is None or res < best[2]:
            best = (lag, slope, res)
    lag, ratio, res = best if best else (0, 0.0, 0.0)
    out("roll: head %.2f to %.2f degrees; the quads' room roll follows the head's %d refreshes "
        "before x%.4f (scatter %.4f degrees); width over height median %.5f, worst change %.5f "
        "%%; a picture's room roll on its refreshes the same to %.5f degrees (%d pictures, %d "
        "refreshes)" % (min(rolls), max(rolls), lag, ratio, res, med, 100.0 * worst_shape, spread,
                        len(by_pic), len(rows)))
    ok = (head_range > 1.0 and worst_shape <= shape_tol and spread <= spread_tol and
          abs(ratio - 1.0) <= 0.02)
    return ok, worst_shape, spread, ratio


def synthetic_roll(gx, gy, speed=120.0, amp=20.0, seconds=2.0, d=3.0, w=6.0, h=3.6, age=3):
    """fovscale: a level mode 3 screen rolled (MOCK_XR_ROLL), pictures on 2 refreshes drawn for the
    head `age` refreshes before, held as HeldQuad holds them: the camera's roll since through
    A = M R M^-1, the nearest rectangle (each axis' length, the mean of their turns)."""
    lines = []
    hz = 120.0
    n = 0
    rolls = {}

    def roll_at(k):
        period = 4.0 * amp / speed
        ph = ((k / hz) % period) / period
        return amp * (4.0 * ph - 1.0 if ph < 0.5 else 3.0 - 4.0 * ph)

    for k in range(int(seconds * hz)):
        if k % 2 == 0:
            lines.append("release sc3")
            n += 1
            rolls[n] = roll_at(k - age)
        rho = roll_at(k)
        dr = math.radians(rho - rolls[n])  # the camera's roll since, in the quad's frame
        c, s_ = math.cos(-dr), math.sin(-dr)
        # A = M R M^-1, R a roll by -dr: columns A e_x, A e_y
        axx, axy = c, s_ * gx / gy
        ayx, ayy = -s_ * gy / gx, c
        sw, sh = w * math.hypot(axx, axy), h * math.hypot(ayx, ayy)
        turn = 0.5 * (math.degrees(math.atan2(axy, axx)) + math.degrees(math.atan2(-ayx, ayy)))
        q = qmul(rollq(rho), rollq(turn))
        pos = qrot(rollq(rho), (0.0, 0.0, -d))
        lines.append("endFrame layers=1 t=%.6f yaw=0.000000 roll=%.6f quad[sc3:1x1 at 0,0 eye=1 "
                     "sp=local size=%.5fx%.5f pos=%.5f,%.5f,%.5f q=%.5f,%.5f,%.5f,%.5f]"
                     % (k / hz, rho, sw, sh, pos[0], pos[1] + 1.6, pos[2], q[0], q[1], q[2], q[3]))
    return lines


def scale_factor(percent):
    """fov100: the screen's size on an axis over the picture's for Horizontal or Vertical Scale
    `percent` (Layout::ScaleFactor): 1 + percent / 100 from -30 up, as every earlier build;
    0.7 e^((percent + 30) / 70) below, 0.2575 at -100."""
    p = max(-100, min(100, int(round(percent))))
    if p >= -30:
        return 1.0 + p / 100.0
    return 0.7 * math.exp((p + 30) / 70.0)


# fovaxis: the split of the scales and the caps against warping, as Layout::SplitScales and
# Layout::GainStretch (immersive_layout.cpp), for the gains the app gives a screen.
FOV_FACTOR_MAX = 1.6      # Layout::kFovFactorMax (fov100: 2.8)
CROP_FACTOR_MIN = 0.25    # Layout::kCropFactorMin (fovscale, fov100: 1.0, a stretch below)
LOG_STRETCH_LO = math.log(1.0 / 1.6)   # 1 / kScaleGainMax
LOG_STRETCH_HI = math.log(1.0 / 0.6)   # 1 / kScaleGainMin
LOG_RATIO = math.log(1.5)              # kMaxStretchRatio


def axis_split(f, k, fov):
    """Layout::ScaleSplit: (drawn, stretch) of one axis, Render Margin factor f, scale size k,
    Field of View share fov (0 Stretch, 1 Field of View)."""
    if fov <= 0.0:
        return f, k
    want = f * (k if fov >= 1.0 else k ** fov)
    d = min(max(want, CROP_FACTOR_MIN), max(FOV_FACTOR_MAX, f))
    return d, f * k / d


def split_scales(fx, kx, fy, ky, fov, caps):
    """Layout::SplitScales: ((drawn, stretch) across, (drawn, stretch) up and down)."""
    x = list(axis_split(fx, kx, fov))
    y = list(axis_split(fy, ky, fov))
    w = max(0.0, min(1.0, caps)) * (1.0 - max(0.0, min(1.0, fov)))
    if w > 0.0:
        def redraw(a, f, k, to_log):
            frm = math.log(a[1])
            if to_log == frm:
                return
            want = f * k / math.exp(frm + w * (to_log - frm))
            a[0] = min(max(want, min(1.0, a[0])), max(FOV_FACTOR_MAX, a[0]))
            a[1] = f * k / a[0]
        tx = min(max(math.log(x[1]), LOG_STRETCH_LO), LOG_STRETCH_HI)
        ty = min(max(math.log(y[1]), LOG_STRETCH_LO), LOG_STRETCH_HI)
        d = tx - ty
        if abs(d) > LOG_RATIO:
            e = (abs(d) - LOG_RATIO) * 0.5 * (1.0 if d > 0 else -1.0)
            tx, ty = tx - e, ty + e
        redraw(x, fx, kx, tx)
        redraw(y, fy, ky, ty)
        for p in range(2):
            lx, ly = math.log(x[1]), math.log(y[1])
            if abs(lx - ly) <= LOG_RATIO + 1e-5:
                break
            sign = 1.0 if lx > ly else -1.0
            if p == 0:
                redraw(y, fy, ky, lx - sign * LOG_RATIO)
            else:
                redraw(x, fx, kx, ly + sign * LOG_RATIO)
    return tuple(x), tuple(y)


def gain_stretch(r, caps):
    """Layout::GainStretch: the stretch the gain follows, within 1/1.6 to 1/0.6 by `caps`."""
    if caps <= 0.0:
        return r
    l = math.log(r)
    to = min(max(l, LOG_STRETCH_LO), LOG_STRETCH_HI)
    return r if to == l else math.exp(l + caps * (to - l))


def scale_gains(base, sx, sy, margin_x=4, margin_y=4, mode="stretch", caps=1.0):
    """The gains (across, up and down) of a screen whose base gain is `base` (1 / size at fit 1,
    Detect Game FOV settled), as SolveHeadLocked gives them."""
    # Render Margin applies with Stretch only (Layout::MarginFactor), and there the caps act on
    # the game's own frame with the margin's padding around it (SplitScales), so the stretches,
    # and with them the gains, are the margin-free split's in both modes.
    x, y = split_scales(1.0, scale_factor(sx), 1.0, scale_factor(sy),
                        1.0 if mode == "fov" else 0.0, caps)
    gx = min(max(base / gain_stretch(x[1], caps), 0.4), 2.0)
    gy = min(max(base / gain_stretch(y[1], caps), 0.4), 2.0)
    return gx, gy


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
        gx = scale_gains(b, sx, sy, *gains_from_log.margins, mode=gains_from_log.mode)[0]
        sp = sorted(max(cs) - min(cs) for cs in
                    ([camera_yaw(r, camera, gx) for r in p] for p in pics))
        return sp[len(sp) // 2] if sp else 1e9

    best = min((cost(b / 200.0), b / 200.0) for b in range(80, 401))
    lo = best[1] - 0.005
    best = min((cost(lo + k / 10000.0), lo + k / 10000.0) for k in range(0, 101))
    b = best[1]
    gx, gy = scale_gains(b, sx, sy, *gains_from_log.margins, mode=gains_from_log.mode)  # fovaxis
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
    # fovaxis: Fill View sets the size and both scales once ("xr: immersive: Fill View: size
    # 1.034, Horizontal Scale 35%, Vertical Scale 60% (Field of View), ..."): the last one after
    # the "on" line is what the screen has, over what was given.
    fl = re.compile(r"^\[\s*([\d.]+)\].*immersive: Fill View: size ([\d.]+), Horizontal Scale "
                    r"(-?\d+)%, Vertical Scale (-?\d+)% \((Stretch|Field of View)\)")
    for line in open(log, errors="replace"):
        m = fl.search(line)
        if m and float(m.group(1)) >= t:
            size, scale_x, scale_y = float(m.group(2)), int(m.group(3)), int(m.group(4))
            gains_from_log.mode = "stretch" if m.group(5) == "Stretch" else "fov"
            out("Fill View at %.2f s: size %.3f, scales %d %%, %d %% (%s)"
                % (float(m.group(1)), size, scale_x, scale_y, m.group(5)))
    # fovscale: the app logs its gains to 4 decimals whenever they move (Detect Game FOV, a scale,
    # Scale Mode): the last such line is the gains in use, with no rounding and no fit (the review
    # of r38: a stale gain from the "mode N on" line failed the check).
    gl = re.compile(r"^\[\s*([\d.]+)\].*immersive: gains ([\d.]+) across, ([\d.]+) up and down")
    latest = None
    for line in open(log, errors="replace"):
        m = gl.search(line)
        if m:
            latest = tuple(float(v) for v in m.groups())
    if latest is not None:
        out("gains: %.4f across, %.4f up, as logged at %.2f s (the last change)" % (latest[1], latest[2],
                                                                                   latest[0]))
        return latest[1], latest[2], max(t, latest[0])
    if size is not None:
        base = 1.0 / min(max(size, 0.6), 2.5)
        # fovaxis: the split and the caps (Stretch, Render Margin 1.4x unless given)
        cx, cy = scale_gains(base, scale_x or 0, scale_y or 0, *gains_from_log.margins,
                             mode=gains_from_log.mode)
        if abs(cx - gx) <= 0.0051 and abs(cy - g) <= 0.0051:
            out("gains: logged %.2f across, %.2f up; exact from size %.3f: %.5f, %.5f" % (gx, g, size, cx, cy))
            return cx, cy, t
        out("gains: logged %.2f across, %.2f up; size %.3f gives %.4f, %.4f, which do not round to "
            "them (the screen fitted or Detect Game FOV): the logged ones used" % (gx, g, size, cx, cy))
    else:
        out("gains: logged %.2f across, %.2f up (2 decimals)" % (gx, g))
    return gx, g, t


gains_from_log.margins = (4, 4)   # fovaxis: --margin-x, --margin-y (Render Margin levels)
gains_from_log.mode = "stretch"   # fovaxis: --scale-mode


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
    # fovscale: the roll check: equal gains (Field of View) pass, unequal (Stretch) fail.
    for gx, gy, want in ((0.714, 0.714, True), (1.0, 1.0, True), (0.714 / 0.7, 0.714 / 0.85, False),
                         (0.714 / 1.3, 0.714, False)):
        fd, path = tempfile.mkstemp(suffix=".trace", dir=os.path.dirname(os.path.abspath(__file__)))
        with os.fdopen(fd, "w") as f:
            f.write("\n".join(synthetic_roll(gx, gy)) + "\n")
        rows = parse(path, 1)
        got = check_roll(rows, parse.rolls, out=lambda s: None)
        os.remove(path)
        print("selftest roll, gains %.3f across, %.3f up: shape change %.5f %%, picture roll spread "
              "%.5f degrees, x%.4f the head's -> %s (want %s)"
              % (gx, gy, 100.0 * got[1], got[2], got[3], "PASS" if got[0] else "FAIL",
                 "PASS" if want else "FAIL"))
        ok &= got[0] == want
    # fov100: the scales' sizes, as Layout::ScaleFactor: the line of earlier builds from -30 up,
    # continuous below it and never 0.
    sizes = [scale_factor(p) for p in range(-100, 101, 5)]
    good = (all(scale_factor(p) == 1.0 + p / 100.0 for p in range(-30, 101, 5))
            and abs(scale_factor(-100) - 0.7 / math.e) < 1e-12
            and all(b > a > 0.0 for a, b in zip(sizes, sizes[1:]))
            and abs(scale_factor(-35) - 0.65) < 0.002 and scale_factor(-1000) == scale_factor(-100))
    print("selftest scale sizes: -100 %% %.4f, -30 %% %.2f, +100 %% %.2f -> %s (want PASS)"
          % (scale_factor(-100), scale_factor(-30), scale_factor(100), "PASS" if good else "FAIL"))
    ok &= good
    # fovaxis: the gains with the caps, against the same cases as the unit test
    # ("FrameXr fovaxis: hold.py's table", immersive_scale.cpp): base 1, Render Margin 1.4x (no
    # change to the gains: the caps act on the game's own frame).
    table = [  # (sx, sy, mode, gain across, gain up and down)
        (-30, -15, "stretch", 1.0 / 0.7, 1.0 / 0.85),  # Mario Kart 7's preset: inside the caps
        (-30, 0, "stretch", 1.0 / 0.7, 1.0),
        (-30, -15, "fov", 1.0, 1.0),                   # crops: true shape, the base gain
        (-100, -100, "fov", 1.0, 1.0),
        (0, 0, "stretch", 1.0, 1.0),
        (30, 0, "fov", 1.0, 1.0),                      # the scales alone (no margin): 1.3x drawn
        (70, 0, "fov", 1.6 / 1.7, 1.0),                # 1.7x wanted, 1.6x drawn, the rest a stretch
        (100, 100, "fov", 0.8, 0.8),                   # 2x wanted: 1.6x drawn, stretch 1.25
        (-100, 0, "stretch", 1.6, 1.6),                # the frame cannot draw less: the tall axis draws 1.6x
        (100, 0, "stretch", 1.0 / 1.5, 1.0),           # the frame drawn 1.33x: the ratio 1.5
        (-30, 30, "stretch", 1.0 / 0.7, 1.0 / 1.05),   # ratio 1.86 before: now 1.5, on the tall axis
        (-100, -100, "stretch", 1.6, 1.6),
    ]
    good = True
    for sx, sy, mode, wx, wy in table:
        gx, gy = scale_gains(1.0, sx, sy, 4, 4, mode)
        # the frame's split (Render Margin's padding goes around it in Stretch; none in fov)
        x, y = split_scales(1.0, scale_factor(sx), 1.0, scale_factor(sy), 1.0 if mode == "fov" else 0.0, 1.0)
        fine = (0.6 - 1e-6 <= gx <= 1.6 + 1e-6 and 0.6 - 1e-6 <= gy <= 1.6 + 1e-6
                and all(math.isfinite(v) and v > 0 for v in x + y)
                and x[0] <= FOV_FACTOR_MAX + 1e-6 and y[0] <= FOV_FACTOR_MAX + 1e-6)
        if mode == "stretch":
            fine &= x[0] >= 1.0 - 1e-6 and y[0] >= 1.0 - 1e-6  # Stretch keeps the whole frame
            free = all(1.0 + 1e-6 < d < FOV_FACTOR_MAX - 1e-6 for d in (x[0], y[0]))
            if free:  # nothing at a limit: the ratio within 1.5
                fine &= abs(math.log(x[1] / y[1])) <= LOG_RATIO + 1e-5
        for got, want in ((gx, wx), (gy, wy)):
            if want is not None:
                fine &= abs(got - want) < 1e-5
        print("selftest caps %+d %% / %+d %% %s: draws %.3fx %.3fx, stretch %.3f %.3f, gains %.4f %.4f -> %s"
              % (sx, sy, mode, x[0], y[0], x[1], y[1], gx, gy, "PASS" if fine else "FAIL"))
        good &= fine
    ok &= good
    # fovaxis: a Fill View line after the "on" line gives the size and scales the gains follow.
    import os
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("[  10.00] xr: immersive mode 3 on (test), gain 0.97, yaw gain 0.97, size 1.000\n")
        f.write("[  12.50] xr: immersive: Fill View: size 1.034, Horizontal Scale 35%, Vertical "
                "Scale 60% (Field of View), height 2.0% over\n")
        name = f.name
    saved = gains_from_log.mode
    got = gains_from_log(name, 1.0, 0, 0, out=lambda *_: None)
    gains_from_log.mode = saved
    os.unlink(name)
    want = 1.0 / 1.034
    good = got is not None and abs(got[0] - want) < 1e-6 and abs(got[1] - want) < 1e-6
    print("selftest Fill View line: gains %s -> %s (want PASS)"
          % ("none" if got is None else "%.5f %.5f" % got[:2], "PASS" if good else "FAIL"))
    ok &= good
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
    ap.add_argument("--margin-x", type=int, default=4, help="fovaxis: Render Margin level across (0-4)")
    ap.add_argument("--margin-y", type=int, default=4, help="fovaxis: Render Margin level up and down")
    ap.add_argument("--scale-mode", choices=("stretch", "fov"), default="stretch",
                    help="fovaxis: Scale Mode for the predicted gains")
    ap.add_argument("--from", dest="t_from", type=float, help="trace time to start at, s")
    ap.add_argument("--to", dest="t_to", type=float, help="trace time to stop at, s")
    ap.add_argument("--roll", action="store_true", help="the roll check (fovscale)")
    ap.add_argument("--shape-tol", type=float, default=0.0002,
                    help="--roll: width over height within this share of its median")
    ap.add_argument("--spread-tol", type=float, default=0.005,
                    help="--roll: a picture's room roll the same on its refreshes to this, degrees")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if a.roll:
        if not a.trace:
            ap.error("a trace is needed")
        rows = parse(a.trace, 1 if a.eye == "left" else 2, a.t_from, a.t_to)
        ok = check_roll(rows, parse.rolls, a.shape_tol, a.spread_tol)[0]
        print("RESULT: %s" % ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    if a.trace and a.log and (a.gain_x is None or a.gain_y is None):
        gains_from_log.margins = (a.margin_x, a.margin_y)  # fovaxis
        gains_from_log.mode = a.scale_mode
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
