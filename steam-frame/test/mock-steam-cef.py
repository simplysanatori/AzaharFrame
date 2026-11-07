#!/usr/bin/env python3
"""A stand-in for Steam's CEF debugging port, for steam-frame/test-release.sh: it answers the few Runtime.evaluate
expressions install.sh / uninstall.sh / steam-cef.py send (shortcut lookup, AddShortcut, SetShortcutExe/StartDir,
SetCustomArtworkForApp, SetShortcutIcon, RemoveShortcut), keeps the shortcut list in a JSON file and logs every call
(long arguments cut).  PORT 0 picks a free port; the port in use is written to STATE.json.port.
   mock-steam-cef.py PORT STATE.json LOG.txt"""
import base64, hashlib, json, re, socketserver, struct, sys, zlib
from http.server import BaseHTTPRequestHandler

PORT, STATE, LOG = int(sys.argv[1]), sys.argv[2], sys.argv[3]

def load():
    try: return json.load(open(STATE))
    except Exception: return {"shortcuts": {}}

def save(st):
    json.dump(st, open(STATE, "w"), indent=1)

def log(s):
    open(LOG, "a").write(s + "\n")

def handle_js(expr):
    st = load()
    log(expr if len(expr) < 300 else expr[:90] + "...(%d bytes)" % len(expr))
    if "allApps" in expr:
        m = re.search(r"display_name === '([^']*)'", expr)
        for appid, sc in st["shortcuts"].items():
            if m and sc["name"] == m.group(1): return int(appid)
        return 0
    m = re.search(r"AddShortcut\('([^']*)', '([^']*)', '([^']*)', '([^']*)'\)", expr)
    if m:
        name, exe, start, opts = m.groups()
        appid = (zlib.crc32(('"%s"' % exe + name).encode()) & 0xffffffff) | 0x80000000
        st["shortcuts"][str(appid)] = {"name": name, "exe": '"%s"' % exe, "start": '"%s"' % start, "opts": opts, "art": {}}
        save(st)
        return appid
    m = re.search(r"SetShortcutExe\((\d+), '([^']*)'\)", expr)
    if m: st["shortcuts"][m.group(1)]["exe"] = m.group(2)
    m = re.search(r"SetShortcutStartDir\((\d+), '([^']*)'\)", expr)
    if m: st["shortcuts"][m.group(1)]["start"] = m.group(2)
    m = re.search(r"SetCustomArtworkForApp\((\d+), '([^']*)', 'png', (\d)\)", expr)
    if m: st["shortcuts"][m.group(1)]["art"][m.group(3)] = len(base64.b64decode(m.group(2)))
    # steam-cef.py sends the icon path as JSON, so with double quotes
    m = re.search(r"SetShortcutIcon\((\d+), (\"(?:[^\"\\]|\\.)*\"|'[^']*')\)", expr)
    if m: st["shortcuts"][m.group(1)]["icon"] = json.loads(m.group(2)) if m.group(2)[0] == '"' else m.group(2)[1:-1]
    m = re.search(r"RemoveShortcut\((\d+)\)", expr)
    if m: st["shortcuts"].pop(m.group(1), None)
    save(st)
    return 1

def recv_exact(f, n):
    b = f.read(n)
    if len(b) < n: raise EOFError
    return b

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/json":
            body = json.dumps([{"title": "SharedJSContext", "webSocketDebuggerUrl": "ws://127.0.0.1:%d/ws" % self.server.server_address[1]}]).encode()
            self.send_response(200); self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
            return
        key = self.headers.get("Sec-WebSocket-Key")
        if not key: self.send_error(404); return
        acc = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101); self.send_header("Upgrade", "websocket"); self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", acc); self.end_headers()
        f = self.rfile
        try:
            while True:
                b1, b2 = recv_exact(f, 2)
                n = b2 & 0x7f
                if n == 126: n = struct.unpack(">H", recv_exact(f, 2))[0]
                elif n == 127: n = struct.unpack(">Q", recv_exact(f, 8))[0]
                mask = recv_exact(f, 4) if b2 & 0x80 else b"\0\0\0\0"
                data = bytes(c ^ mask[i % 4] for i, c in enumerate(recv_exact(f, n)))
                if b1 & 0x0f == 8: return
                req = json.loads(data)
                val = handle_js(req["params"]["expression"])
                out = json.dumps({"id": req["id"], "result": {"result": {"type": "number", "value": val}}}).encode()
                hdr = bytearray([0x81])
                if len(out) < 126: hdr.append(len(out))
                elif len(out) < 65536: hdr.append(126); hdr += struct.pack(">H", len(out))
                else: hdr.append(127); hdr += struct.pack(">Q", len(out))
                self.wfile.write(bytes(hdr) + out); self.wfile.flush()
        except EOFError:
            return

class S(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True

srv = S(("127.0.0.1", PORT), H)
open(STATE + ".port", "w").write(str(srv.server_address[1]))
srv.serve_forever()
