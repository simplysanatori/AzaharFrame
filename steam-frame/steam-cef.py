#!/usr/bin/env python3
"""Evaluate JavaScript in Steam's SharedJSContext over the CEF remote debugging port.
Usage: steam-cef.py 'JS expression'   (awaits promises; prints JSON result)
       steam-cef.py --art APPID DIR [--dry-run]   (set a shortcut's library art and icon from DIR)
       steam-cef.py --screen theater|dashboard APPID [SECONDS]   (where SteamVR shows the app's
           flat window: theater = the big screen, dashboard = back inside the dashboard)
Steam's debugging port is 8080; AZAHARFRAME_CEF_PORT names another (for tests)."""
import json, os, socket, struct, subprocess, sys, time, base64, urllib.request

def ws_connect(url):
    # ws://host:port/path
    rest = url[len('ws://'):]
    hostport, path = rest.split('/', 1)
    host, port = hostport.split(':')
    s = socket.create_connection((host, int(port)), timeout=30)
    key = base64.b64encode(os.urandom(16)).decode()
    req = (f"GET /{path} HTTP/1.1\r\nHost: {hostport}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n")
    s.sendall(req.encode())
    buf = b''
    while b'\r\n\r\n' not in buf:
        chunk = s.recv(4096)
        if not chunk: raise RuntimeError('handshake failed')
        buf += chunk
    if b' 101 ' not in buf.split(b'\r\n')[0]:
        raise RuntimeError('handshake rejected: ' + buf.decode(errors='replace'))
    return s

def ws_send(s, text):
    data = text.encode()
    hdr = bytearray([0x81])
    n = len(data)
    if n < 126: hdr.append(0x80 | n)
    elif n < 65536: hdr.append(0x80 | 126); hdr += struct.pack('>H', n)
    else: hdr.append(0x80 | 127); hdr += struct.pack('>Q', n)
    mask = os.urandom(4)
    hdr += mask
    s.sendall(bytes(hdr) + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

def recv_exact(s, n):
    buf = b''
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk: raise RuntimeError('connection closed')
        buf += chunk
    return buf

def ws_recv(s):
    while True:
        b1, b2 = recv_exact(s, 2)
        op = b1 & 0x0f
        n = b2 & 0x7f
        if n == 126: n = struct.unpack('>H', recv_exact(s, 2))[0]
        elif n == 127: n = struct.unpack('>Q', recv_exact(s, 8))[0]
        if b2 & 0x80: mask = recv_exact(s, 4)
        else: mask = None
        payload = recv_exact(s, n)
        if mask: payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        if op == 1: return payload.decode()
        if op == 8: raise RuntimeError('closed')
        # ignore ping/pong/binary

def evaluate(expr, title='SharedJSContext'):
    port = os.environ.get('AZAHARFRAME_CEF_PORT') or '8080'   # Steam's debugging port; the variable is for tests
    pages = json.load(urllib.request.urlopen('http://127.0.0.1:%s/json' % port, timeout=5))
    page = next(p for p in pages if p.get('title') == title)
    s = ws_connect(page['webSocketDebuggerUrl'])
    ws_send(s, json.dumps({'id': 1, 'method': 'Runtime.evaluate',
                           'params': {'expression': expr, 'awaitPromise': True, 'returnByValue': True}}))
    while True:
        msg = json.loads(ws_recv(s))
        if msg.get('id') == 1:
            s.close()
            if 'error' in msg: raise RuntimeError(msg['error'])
            r = msg['result']
            if 'exceptionDetails' in r:
                raise RuntimeError(json.dumps(r['exceptionDetails'].get('exception', r['exceptionDetails']))[:2000])
            return r['result'].get('value')

def apply_art(appid, artdir, dry_run=False):
    """Set the library art of a non-Steam shortcut from ARTDIR (portrait, wide, hero, logo, icon .png).
    The images are large, so they are sent from here and not as a command line argument.
    With dry_run it only prints each call (and does not connect to Steam).
    The icon (a full-bleed 512 px square) is what the library and SteamVR's dock show."""
    for name, kind in (('portrait', 0), ('hero', 1), ('logo', 2), ('wide', 3)):
        path = os.path.join(artdir, name + '.png')
        if not os.path.isfile(path):
            print('no ' + path + ', skipped', file=sys.stderr)
            continue
        if dry_run:
            print("[dry run] would call Steam (CEF): SteamClient.Apps.SetCustomArtworkForApp(%d, <%s, %d bytes, base64>, 'png', %d)"
                  % (int(appid) if str(appid).isdigit() else 0, path, os.path.getsize(path), kind))
            continue
        b64 = base64.b64encode(open(path, 'rb').read()).decode()
        evaluate("SteamClient.Apps.SetCustomArtworkForApp(%d, '%s', 'png', %d)" % (int(appid), b64, kind))
    icon = os.path.join(artdir, 'icon.png')
    if os.path.isfile(icon):
        if dry_run:
            print("[dry run] would call Steam (CEF): SteamClient.Apps.SetShortcutIcon(%s, %s)" % (appid, json.dumps(icon)))
        else:
            evaluate("SteamClient.Apps.SetShortcutIcon(%d, %s)" % (int(appid), json.dumps(icon)))
    else:
        print('no ' + icon + ', the icon is not set', file=sys.stderr)

VRCMD_PATHS = ('/opt/steamvr/bin/linuxarm64/vrcmd', '/opt/steamvr/bin/linux64/vrcmd')

def vrcmd(*args):
    exe = os.environ.get('AZAHARFRAME_VRCMD') or next((p for p in VRCMD_PATHS if os.path.isfile(p)), None)
    if not exe:
        raise RuntimeError('no vrcmd')
    env = dict(os.environ)
    env.pop('LD_PRELOAD', None)  # Steam's overlay library, not for a SteamVR tool
    return subprocess.run([exe] + list(args), capture_output=True, text=True, timeout=10, env=env).stdout

def overlay_exists(key):
    """vrcmd --overlays lists one overlay per line as 'key' -- 'name', ..."""
    return any(line.startswith("'%s' " % key) for line in vrcmd('--overlays').splitlines())

def screen(where, appid, seconds=8.0):
    """Moves the frame of the app's flat window (gamescope's overlay valve.steam.desktopgame.APPID) to
    the theater screen or back into the dashboard, through vrcmd --dock-overlay (SteamVR's dashboard,
    onVrCmdDockOverlayRequested), then hides the dashboard. The SteamVR dashboard docks a game window
    in the theater on its own only when the frame first comes alive and the app is not the scene app
    (dashboard.autoShowGameTheater, on by default); after an OpenXR session the frame already exists,
    so nothing moves it, and with that setting off a flat window only shows while the dashboard is open.
    The dock is only asked for once the overlay exists: for an unknown key the dashboard docks
    whichever frame is active instead."""
    key = 'valve.steam.desktopgame.%d' % int(appid)
    deadline = time.monotonic() + seconds
    while not overlay_exists(key):
        if where != 'theater' or time.monotonic() >= deadline:
            print('%s: no overlay %s, nothing moved' % (where, key))
            return 1
        time.sleep(0.25)
    if where == 'theater':
        time.sleep(0.75)  # the dashboard makes the window's frame a moment after the overlay
        if not overlay_exists(key):
            print('theater: overlay %s went away, nothing moved' % key)
            return 1
    vrcmd('--dock-overlay', where, key)
    vrcmd('--hidedashboard')
    print('%s: docked %s, dashboard hidden' % (where, key))
    return 0

if __name__ == '__main__':
    if len(sys.argv) in (4, 5) and sys.argv[1] == '--screen' and sys.argv[2] in ('theater', 'dashboard'):
        sys.exit(screen(sys.argv[2], sys.argv[3], float(sys.argv[4]) if len(sys.argv) == 5 else 8.0))
    elif len(sys.argv) in (4, 5) and sys.argv[1] == '--art':
        apply_art(sys.argv[2], sys.argv[3], dry_run=sys.argv[4:] == ['--dry-run'])   # steam-cef.py --art APPID DIR [--dry-run]
    else:
        print(json.dumps(evaluate(sys.argv[1]), indent=1))
