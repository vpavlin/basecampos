#!/usr/bin/env python3
"""vm-tap.py X Y [W H] — tap the VM's screen at pixel X,Y (screen W x H,
default 720x1440) through QMP absolute input (virtio-tablet). vm-run.sh opens
the QMP socket."""
import json, socket, sys, time, os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
x, y = int(sys.argv[1]), int(sys.argv[2])
w, h = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (720, 1440)

s = socket.socket(socket.AF_UNIX)
s.connect(os.path.join(root, "vm", "qmp.sock"))
f = s.makefile("rw")
f.readline()
def cmd(c, **a):
    f.write(json.dumps({"execute": c, "arguments": a} if a else {"execute": c}) + "\n"); f.flush()
    while True:
        r = json.loads(f.readline())
        if "return" in r or "error" in r:
            return r
cmd("qmp_capabilities")
ax = lambda v, m: int(v * 32767 / (m - 1))
move = [{"type": "abs", "data": {"axis": "x", "value": ax(x, w)}},
        {"type": "abs", "data": {"axis": "y", "value": ax(y, h)}}]
cmd("input-send-event", events=move)
time.sleep(0.05)
cmd("input-send-event", events=[{"type": "btn", "data": {"down": True, "button": "left"}}])
time.sleep(0.08)
print(cmd("input-send-event", events=[{"type": "btn", "data": {"down": False, "button": "left"}}]))
