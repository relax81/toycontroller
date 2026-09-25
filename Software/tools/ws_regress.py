# Regression test of the WebSocket protocol (protocol.cpp) against a running device, raw sockets, Python 3.
#   python ws_regress.py [host]      (default: toycontroller.local)
#   python ws_regress.py [host] [--collar]
# The collar.beep check (433 MHz signal) only runs with --collar, otherwise it is reported as SKIP.
# Only use --collar with the collar out of reach or switched off. The other checks change pwm values and
# restore them; do not run the test with a pump or other loads connected.
import socket, os, base64, struct, json, time, sys

ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
HOST = ARGS[0] if ARGS else 'toycontroller.local'
HOST = socket.gethostbyname(HOST)  # resolve once: a .local lookup per request would distort the timer checks
COLLAR = '--collar' in sys.argv[1:]

class WS:
    def __init__(self):
        self.s = socket.create_connection((HOST, 80), timeout=3)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(('GET /ws HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                        'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n' % (HOST, key)).encode())
        buf = b''
        while b'\r\n\r\n' not in buf:
            buf += self.s.recv(1024)
        assert b' 101 ' in buf.split(b'\r\n')[0], buf
        self.rest = buf.split(b'\r\n\r\n', 1)[1]

    def send(self, text):
        data = text.encode()
        hdr = bytearray([0x81])
        n = len(data)
        if n < 126: hdr.append(0x80 | n)
        else: hdr += bytes([0x80 | 126]) + struct.pack('>H', n)
        mask = os.urandom(4)
        self.s.sendall(bytes(hdr) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    # A frame is only taken out of the buffer when it is complete: a receive timeout in the middle of a frame
    # must not lose the header (the old code did, and every later reply was then lost).
    def _fill(self, timeout):
        self.s.settimeout(timeout)
        try:
            chunk = self.s.recv(4096)
        except socket.timeout:
            return False
        if not chunk:
            raise EOFError
        self.rest += chunk
        return True

    def _frame(self):
        r = self.rest
        if len(r) < 2:
            return None
        n = r[1] & 0x7F
        off = 2
        if n == 126:
            if len(r) < 4: return None
            n = struct.unpack('>H', r[2:4])[0]
            off = 4
        elif n == 127:
            if len(r) < 10: return None
            n = struct.unpack('>Q', r[2:10])[0]
            off = 10
        if len(r) < off + n:
            return None
        op = r[0] & 0x0F
        data = bytes(r[off:off + n])
        self.rest = r[off + n:]
        return op, data

    def recv(self, timeout=2.0):
        end = time.time() + timeout
        while True:
            f = self._frame()
            if f:
                op, data = f
                if op == 1:
                    return data.decode()
                if op == 9:  # ping: answer with a pong (as a browser does)
                    mask = os.urandom(4)
                    self.s.sendall(bytes([0x8A, 0x80 | len(data)]) + mask + bytes(b ^ mask[k % 4] for k, b in enumerate(data)))
                if op == 8:
                    self.closed = True
                    return None
                continue
            left = end - time.time()
            if left <= 0:
                return None
            try:
                if not self._fill(left):
                    return None
            except EOFError:
                self.closed = True
                return None

    def recv_json_until(self, pred, timeout=3.0):
        end = time.time() + timeout
        while time.time() < end:
            m = self.recv(max(0.1, end - time.time()))
            if m is None: continue
            try: j = json.loads(m)
            except Exception: continue
            if pred(j): return j
        return None

    def close(self):
        # clean close handshake: a hard close with unread data sends an RST, which the AsyncTCP version
        # on the device does not always survive (see docs/STATUS.md)
        try:
            payload = struct.pack('>H', 1000)
            mask = os.urandom(4)
            self.s.sendall(bytes([0x88, 0x80 | len(payload)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))
            end = time.time() + 1.5
            while time.time() < end:
                if self.recv(0.3) is None and getattr(self, 'closed', False): break
                try:
                    if not self.s.recv(4096, socket.MSG_PEEK): break
                except socket.timeout: continue
                except Exception: break
        except Exception:
            pass
        try:
            self.s.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass
        try: self.s.close()
        except Exception: pass

ok = fail = skipped = 0
def skip(name, why='needs --collar (sends 433 MHz signals)'):
    global skipped
    skipped += 1
    print('SKIP', name, '-', why)

def check(name, cond, info=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, info)

a = WS(); b = WS()
a.send('{"t":"get","id":1}'); b.send('{"t":"get","id":1}')
sa = a.recv_json_until(lambda j: j.get('t') == 'state')
sb = b.recv_json_until(lambda j: j.get('t') == 'state')
check('state A', sa is not None and len(sa['d']) >= 30, sa and len(sa['d']))
check('state B', sb is not None)
print('keys:', len(sa['d']))
check('has hold + toy keys', all(k in sa['d'] for k in ('ble.hold.buzzer', 'ble.toy', 'pump.on', 'pump.off')))

# valid set -> ack, patch for both tabs
a.send('{"t":"set","id":2,"d":{"ch1.pwm":37,"ch2.on":25}}')
ack = a.recv_json_until(lambda j: j.get('t') in ('ack', 'err'))
check('set ack', ack and ack['t'] == 'ack' and ack.get('id') == 2, ack)
pa = a.recv_json_until(lambda j: j.get('t') == 'patch')
pb = b.recv_json_until(lambda j: j.get('t') == 'patch')
check('patch A', pa and pa['d'].get('ch1.pwm') == 37 and pa['d'].get('ch2.on') == 25, pa)
check('patch B (second tab)', pb and pb['d'].get('ch1.pwm') == 37, pb)

# mixed errors
a.send('{"t":"set","id":3,"d":{"ch1.pwm":60,"buzzer.bpm":999,"ble.connected":1,"nope":1,"ch1.en":"x","ch3.pwm":1.5}}')
e = a.recv_json_until(lambda j: j.get('t') in ('ack', 'err') and j.get('id') == 3)
codes = {x['k']: x['code'] for x in (e or {}).get('errors', [])}
check('mixed errors', e and e['t'] == 'err' and e.get('applied') == 1 and codes == {
    'buzzer.bpm': 'range', 'ble.connected': 'readonly', 'nope': 'unknown', 'ch1.en': 'type', 'ch3.pwm': 'type'}, e)
rng = [x for x in (e or {}).get('errors', []) if x['k'] == 'buzzer.bpm']
check('range info', rng and rng[0].get('min') == 1 and rng[0].get('max') == 255, e)

# cmd
a.send('{"t":"set","id":30,"d":{"collar.en":false}}')  # make sure the collar is off: the beep below must be refused
a.recv_json_until(lambda j: j.get('id') == 30)
time.sleep(0.3)
a.send('{"t":"cmd","id":4,"c":"collar.beep"}')
e = a.recv_json_until(lambda j: j.get('id') == 4)
check('collar disabled', e and e.get('code') == 'disabled', e)
a.send('{"t":"cmd","id":5,"c":"bogus"}')
e = a.recv_json_until(lambda j: j.get('id') == 5)
check('unknown cmd', e and e.get('code') == 'unknown_cmd', e)
a.send('{"t":"cmd","id":6}')
e = a.recv_json_until(lambda j: j.get('id') == 6)
check('cmd without c', e and e.get('code') == 'type', e)
a.send('{"t":"cmd","id":7,"c":"all_off"}')
e = a.recv_json_until(lambda j: j.get('id') == 7)
check('all_off ack', e and e['t'] == 'ack', e)

# collar enabled: beep ok (sends a 433 MHz frame, only with --collar)
if COLLAR:
    a.send('{"t":"set","id":8,"d":{"collar.en":true}}')
    a.recv_json_until(lambda j: j.get('id') == 8)
    time.sleep(0.15)  # the set is applied by loop(); a real UI needs a click for the beep anyway
    a.send('{"t":"cmd","id":9,"c":"collar.beep"}')
    e = a.recv_json_until(lambda j: j.get('id') == 9)
    check('collar.beep ack when enabled', e and e['t'] == 'ack', e)
    a.send('{"t":"set","id":10,"d":{"collar.en":false}}')
    a.recv_json_until(lambda j: j.get('id') == 10)
else:
    skip('collar.beep ack when enabled')

# empty d object must not crash the device
a.send('{"t":"set","id":20,"d":{}}')
e = a.recv_json_until(lambda j: j.get('id') == 20)
check('WS set with empty d: ack, no crash', e and e['t'] == 'ack', e)

# parse errors
a.send('{bad json')
e = a.recv_json_until(lambda j: j.get('code') == 'parse')
check('parse error', e is not None, e)
a.send('{"t":"set","d":"x"}')
e = a.recv_json_until(lambda j: j.get('code') == 'type')
check('set without object', e is not None, e)
a.send('{"t":"zzz"}')
e = a.recv_json_until(lambda j: j.get('code') == 'unknown_type')
check('unknown type', e is not None, e)
a.send('{"t":"get","k":["ch1.pwm","nope"]}')
e = a.recv_json_until(lambda j: j.get('t') in ('err', 'state'))
check('get with unknown key', e is not None, e)

# ble.toy set to the CURRENT value: accepted, no restart
cur = sa['d']['ble.toy']
a.send('{"t":"set","id":11,"d":{"ble.toy":%d}}' % cur)
e = a.recv_json_until(lambda j: j.get('id') == 11)
check('ble.toy same value ack', e and e['t'] == 'ack', e)
time.sleep(2.5)
try:
    c = WS()  # a fresh connection: b has not been read for a while and may have been closed by the stall protection
    c.send('{"t":"get","id":12}')
    st = c.recv_json_until(lambda j: j.get('t') == 'state', 3)
    check('no restart after same-toy set', st is not None)
    c.close()
except Exception as ex:
    check('no restart after same-toy set', False, ex)

# restore
a.send('{"t":"set","id":13,"d":{"ch1.pwm":0,"ch2.on":0}}')
e = a.recv_json_until(lambda j: j.get('id') == 13)
check('restore', e and e['t'] == 'ack', e)
a.close(); b.close()
print('\nresult: %d passed, %d failed, %d skipped%s' % (ok, fail, skipped, '' if COLLAR else ' (run with --collar to include the collar checks)'))
sys.exit(1 if fail else 0)
