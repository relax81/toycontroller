# Test of the HTTP API (docs/API.md) against a running device. Python 3, standard library only.
#   python api_test.py [host] [--collar]     (default host: toycontroller.local)
# WARNING: the test switches outputs on and off (ch1-4, pump, buzzer), sets pwm values and restores what it changed.
# Do not run it with a pump or other loads connected.
# Everything that sends 433 MHz signals (collar.beep / collar.vibe with collar.en on) only runs with --collar;
# without it those checks are reported as SKIP. Only use --collar with the collar out of reach or switched off.
# Not covered (needs a Bluetooth client): the "held" list while Bluetooth holds an output; ble.toy with a new value.
import http.client, json, socket, sys, time, threading

ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
HOST = ARGS[0] if ARGS else 'toycontroller.local'
HOST = socket.gethostbyname(HOST)  # resolve once: a .local lookup per request would distort the timer checks
COLLAR = '--collar' in sys.argv[1:]
ok = fail = skipped = 0

def skip(name, why='needs --collar (sends 433 MHz signals)'):
    global skipped
    skipped += 1
    print('SKIP', name, '-', why)

def check(name, cond, info=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '->', info)

def req(method, path, body=None, headers=None, raw=False):
    c = http.client.HTTPConnection(HOST, 80, timeout=5)
    h = dict(headers or {})
    data = None
    if body is not None:
        data = body if isinstance(body, (bytes, str)) else json.dumps(body)
        h.setdefault('Content-Type', 'application/json')
    c.request(method, path, body=data, headers=h)
    r = c.getresponse()
    txt = r.read().decode()
    hdrs = dict((k.lower(), v) for k, v in r.getheaders())
    c.close()
    if raw: return r.status, txt, hdrs
    try: return r.status, json.loads(txt) if txt else None, hdrs
    except Exception: return r.status, txt, hdrs

def state():
    s, j, _ = req('GET', '/api/state')
    return j

def settle(): time.sleep(0.25)

# ---------------------------------------------------------------- keys / state / cors
s, keys, h = req('GET', '/api/keys')
check('keys 200 json', s == 200 and isinstance(keys, dict) and keys.get('api') == 1, (s, str(keys)[:100]))
names = [k['k'] for k in keys['keys']]
check('keys: count and fields', len(names) >= 40 and all(x in keys['keys'][0] for x in ('type', 'min', 'max', 'rw', 'desc')), len(names))
check('keys: cors header', h.get('access-control-allow-origin') == '*', h)
check('keys: ble.toy restarts_device', [k for k in keys['keys'] if k['k'] == 'ble.toy'][0].get('restarts_device') is True)
check('keys: held_by on ch1.pwm', [k for k in keys['keys'] if k['k'] == 'ch1.pwm'][0].get('held_by') == 'ble.hold.ch1')
check('keys: cmds', {c['c'] for c in keys['cmds']} == {'collar.beep', 'collar.vibe', 'collar.shock', 'all_off'})
writable = [k for k in keys['keys'] if k['rw'] == 'rw']
print('keys:', len(names), 'writable:', len(writable), 'bytes:', len(json.dumps(keys)))

st = state()
check('state ok', st and st['ok'] and len(st['d']) == len(names) and st['timers'] == [], st)
orig = dict(st['d'])
s, t, h = req('OPTIONS', '/api/set', raw=True)
check('options 204 + private network', s == 204 and h.get('access-control-allow-private-network') == 'true'
      and 'POST' in h.get('access-control-allow-methods', '') and h.get('access-control-allow-origin') == '*', (s, h))
for route in ('/api/state', '/api/keys', '/api/cmd', '/api/toggle'):
    s, t, h = req('OPTIONS', route, raw=True)
    check('options ' + route, s == 204, s)

# ---------------------------------------------------------------- set (POST json, GET query)
s, j, h = req('POST', '/api/set', {'ch1.pwm': 42, 'ch2.on': 33})
check('POST set ok', s == 200 and j['ok'] is True and j['applied'] == 2, (s, j))
check('set: cors + no-store', h.get('access-control-allow-origin') == '*' and h.get('cache-control') == 'no-store', h)
settle(); st = state()
check('POST set visible in state', st['d']['ch1.pwm'] == 42 and st['d']['ch2.on'] == 33, st['d'])

s, j, _ = req('POST', '/api/set', {'ch1.pwm': 60, 'buzzer.bpm': 999, 'ble.connected': 1, 'nope': 1, 'ch1.en': 'x', 'ch3.pwm': 1.5})
codes = {e['k']: e['code'] for e in (j or {}).get('errors', [])}
check('partial errors: 400 + applied 1', s == 400 and j['ok'] is False and j['applied'] == 1 and codes == {
    'buzzer.bpm': 'range', 'ble.connected': 'readonly', 'nope': 'unknown', 'ch1.en': 'type', 'ch3.pwm': 'type'}, (s, j))
rng = [e for e in j['errors'] if e['k'] == 'buzzer.bpm'][0]
check('range error has min/max', rng.get('min') == 1 and rng.get('max') == 255, rng)
settle(); check('valid key of a partial call was applied', state()['d']['ch1.pwm'] == 60)

s, j, _ = req('GET', '/api/set?ch1.pwm=55&pump.pwm=20&buzzer.en=true&buzzer.en=false')
check('GET set (several keys)', s == 200 and j['applied'] == 4, (s, j))
settle(); st = state()
check('GET set visible', st['d']['ch1.pwm'] == 55 and st['d']['pump.pwm'] == 20 and st['d']['buzzer.en'] is False, st['d'])
s, j, _ = req('GET', '/api/set?ch1.pwm=abc')
check('GET set type error', s == 400 and j['errors'][0]['code'] == 'type', (s, j))
s, j, _ = req('GET', '/api/set?ch1.pwm=101')
check('GET set range error', s == 400 and j['errors'][0] == {'k': 'ch1.pwm', 'code': 'range', 'min': 0, 'max': 100}, (s, j))
s, j, _ = req('POST', '/api/set', {'t': 'set', 'd': {'ch3.pwm': 12}})
check('ws-style body {"t":"set","d":{..}}', s == 200 and j['applied'] == 1, (s, j))
s, j, _ = req('POST', '/api/set?ch4.pwm=7', {'ch4.on': 5})
check('POST body + query together', s == 200 and j['applied'] == 2, (s, j))

# ---------------------------------------------------------------- bad input
s, j, _ = req('POST', '/api/set', '{bad json')
check('invalid json -> 400 parse', s == 400 and j['code'] == 'parse', (s, j))
s, j, _ = req('POST', '/api/set', '[1,2]')
check('json array -> 400 parse', s == 400 and j['code'] == 'parse', (s, j))
s, j, _ = req('POST', '/api/set', '{}')
check('empty object -> 400 empty', s == 400 and j['code'] == 'empty', (s, j))
s, j, _ = req('POST', '/api/set', '{"d":{}}')
check('empty d object -> 400 empty (used to crash keys())', s == 400 and j['code'] == 'empty', (s, j))
s, j, _ = req('POST', '/api/set', '{"ch3.pwm":9}', headers={'Content-Type': 'application/x-www-form-urlencoded'})
check('JSON body with form content type (curl -d): accepted', s == 200 and j['applied'] == 1, (s, j))
s, j, _ = req('POST', '/api/set', 'ch3.pwm=8&ch4.pwm=8&for=5', headers={'Content-Type': 'application/x-www-form-urlencoded'})
check('form fields ch3.pwm=8&ch4.pwm=8', s == 200 and j['applied'] == 2, (s, j))
s, j, _ = req('POST', '/api/cmd', 'c=all_off', headers={'Content-Type': 'application/x-www-form-urlencoded'})
check('cmd as form field', s == 200, (s, j))
s, j, _ = req('GET', '/api/set')
check('no keys -> 400 empty', s == 400 and j['code'] == 'empty', (s, j))
big = json.dumps({'ch1.pwm': 1, 'pad': 'x' * 600})
s, j, _ = req('POST', '/api/set', big)
check('body 600 B -> 413', s == 413 and j['code'] == 'too_large', (s, j))
edge = '{"ch1.pwm":5' + ' ' * (512 - 13) + '}'
check('edge body is 512 B', len(edge) == 512, len(edge))
s, j, _ = req('POST', '/api/set', edge)
check('body exactly 512 B accepted', s == 200, (s, j))
s2 = socket.create_connection((HOST, 80), timeout=5)
s2.sendall(b'POST /api/set HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Type: application/json\r\n\r\n'
           b'a\r\n{"ch1.pwm"\r\n0\r\n\r\n')
resp = s2.recv(2048).decode(errors='replace'); s2.close()
check('chunked body -> 411', ' 411 ' in resp.split('\r\n')[0], resp[:80])
s, j, _ = req('GET', '/api/nothing', raw=True)
check('unknown route still 404', s == 404, s)

# ---------------------------------------------------------------- toggle
req('GET', '/api/set?ch1.en=0'); settle()
s, j, _ = req('GET', '/api/toggle?k=ch1.en')
check('toggle on', s == 200 and j['ok'] and j['value'] is True and j['k'] == 'ch1.en', (s, j))
settle(); check('toggle visible', state()['d']['ch1.en'] is True)
s, j, _ = req('GET', '/api/toggle?k=ch1.en')
check('toggle off', s == 200 and j['value'] is False, (s, j))
settle()
s, j, _ = req('GET', '/api/toggle?k=ch1.pwm'); check('toggle int key -> type', s == 400 and j['errors'][0]['code'] == 'type', (s, j))
s, j, _ = req('GET', '/api/toggle?k=ble.connected'); check('toggle readonly', s == 400 and j['errors'][0]['code'] == 'readonly', (s, j))
s, j, _ = req('GET', '/api/toggle?k=zzz'); check('toggle unknown', s == 400 and j['errors'][0]['code'] == 'unknown', (s, j))
s, j, _ = req('GET', '/api/toggle'); check('toggle without k', s == 400 and j['code'] == 'missing_k', (s, j))

# ---------------------------------------------------------------- cmd, 429
req('GET', '/api/set?collar.en=0'); settle()
if state()['d']['collar.en'] is False:  # disabled: the device answers 409 and sends nothing
    s, j, _ = req('GET', '/api/cmd?c=collar.beep')
    check('collar disabled -> 409', s == 409 and j['code'] == 'disabled', (s, j))
else:
    skip('collar disabled -> 409', 'collar.en could not be switched off')
if COLLAR:
    req('GET', '/api/set?collar.en=1'); settle()
    s, j, _ = req('GET', '/api/cmd?c=collar.beep')
    check('collar.beep ok', s == 200 and j['ok'], (s, j))
    s, j, _ = req('POST', '/api/cmd', {'c': 'collar.vibe'})
    check('second collar command within 300 ms -> 429', s == 429 and j['code'] == 'rate_limited' and 0 < j['retry_ms'] <= 300, (s, j))
    time.sleep(0.4)
    s, j, _ = req('POST', '/api/cmd', {'c': 'collar.vibe'})
    check('collar command after the gap ok', s == 200, (s, j))
else:
    skip('collar.beep ok')
    skip('second collar command within 300 ms -> 429')
    skip('collar command after the gap ok')
s, j, _ = req('GET', '/api/cmd?c=all_off'); check('all_off is not rate limited', s == 200, (s, j))
s, j, _ = req('GET', '/api/cmd?c=all_off'); check('all_off again', s == 200, (s, j))
s, j, _ = req('GET', '/api/cmd?c=bogus'); check('unknown cmd -> 400', s == 400 and j['code'] == 'unknown_cmd', (s, j))
s, j, _ = req('GET', '/api/cmd'); check('cmd without c -> 400', s == 400 and j['code'] == 'missing_c', (s, j))
s, j, _ = req('POST', '/api/cmd', '{bad'); check('cmd invalid json -> 400', s == 400 and j['code'] == 'parse', (s, j))
settle(); check('all_off switched collar.en off', state()['d']['collar.en'] is False)

# ---------------------------------------------------------------- ble.toy same value
cur = orig['ble.toy']
s, j, _ = req('POST', '/api/set', {'ble.toy': cur})
check('ble.toy same value: ok, no restart_in_s', s == 200 and 'restart_in_s' not in j, (s, j))
s, j, _ = req('GET', '/api/set?ble.toy=99')
check('ble.toy out of range', s == 400 and j['errors'][0]['code'] == 'range', (s, j))

# ---------------------------------------------------------------- for= timers
s, j, _ = req('GET', '/api/set?ch1.en=1&for=2')
check('for=2: timer reported', s == 200 and j['timers'] == [{'k': 'ch1.en', 'for_s': 2}], (s, j))
settle(); st = state()
check('timer listed in state, key on', st['d']['ch1.en'] is True and st['timers'] and st['timers'][0]['k'] == 'ch1.en' and 1 <= st['timers'][0]['left_s'] <= 2, st)
time.sleep(2.3); st = state()
check('timer expired: key off, list empty', st['d']['ch1.en'] is False and st['timers'] == [], st)

req('GET', '/api/set?ch1.en=1&for=2'); time.sleep(1.2)
s, j, _ = req('GET', '/api/set?ch1.en=1&for=3')
check('second call replaces the timer', s == 200 and j['timers'][0]['for_s'] == 3, (s, j))
time.sleep(2.0); check('still on after the first would have expired', state()['d']['ch1.en'] is True)
time.sleep(1.6); check('off after the replaced timer', state()['d']['ch1.en'] is False)

req('GET', '/api/set?ch2.en=1&for=30'); settle()
req('GET', '/api/set?ch2.en=0'); settle()
check('set to 0 cancels the timer', state()['timers'] == [])
req('GET', '/api/set?ch2.en=1&for=2'); settle()
req('GET', '/api/set?ch2.en=1'); settle()
check('explicit set without for= cancels the timer', state()['timers'] == [])
time.sleep(2.3); check('...and the key stays on', state()['d']['ch2.en'] is True)
req('GET', '/api/set?ch2.en=0')

s, j, _ = req('POST', '/api/set?for=2', {'ch3.en': True, 'ch3.pwm': 10})
check('for on non-en keys: for_ignored', s == 200 and j['for_ignored'] == ['ch3.pwm'] and len(j['timers']) == 1, (s, j))
s, j, _ = req('POST', '/api/set', {'pump.en': True, 'for': 2})
check('for as JSON member', s == 200 and j['timers'] == [{'k': 'pump.en', 'for_s': 2}], (s, j))
s, j, _ = req('GET', '/api/toggle?k=buzzer.en&for=2')
check('toggle with for=', s == 200 and j['value'] is True and j['timers'][0]['k'] == 'buzzer.en', (s, j))
settle(); st = state()
check('three timers running', len(st['timers']) == 3, st['timers'])
req('GET', '/api/cmd?c=all_off'); settle()
check('all_off clears every timer', state()['timers'] == [])
for bad in ('0', '3601', 'abc', '-5', '1.5'):
    s, j, _ = req('GET', '/api/set?ch1.en=1&for=' + bad)
    check('for=%s rejected before applying' % bad, s == 400 and j['applied'] == 0 and j['errors'][0]['k'] == 'for', (s, j))
settle(); check('rejected for= did not switch anything on', state()['d']['ch1.en'] is False)
s, j, _ = req('GET', '/api/set?ch1.en=1&for=3600')
check('for=3600 accepted', s == 200, (s, j))
req('GET', '/api/cmd?c=all_off'); settle()

# ---------------------------------------------------------------- 30 keys, 512 B (stack measurement)
body = {}
for k in writable:
    if k['k'] in ('ble.toy',): continue
    body[k['k']] = orig[k['k']]
    if len(json.dumps(body, separators=(',', ':'))) > 470 or len(body) >= 30: break
raw = json.dumps(body, separators=(',', ':'))
print('30-key body: %d keys, %d bytes' % (len(body), len(raw)))
s, j, _ = req('POST', '/api/set', raw)
check('30 keys / body <= 512 B accepted', s == 200 and j['applied'] == len(body) and len(raw) <= 512, (s, j, len(raw)))

# ---------------------------------------------------------------- loop calls (Stream Deck / Home Assistant style)
t0 = time.time(); bad = 0
for i in range(150):
    s, j, _ = req('GET', '/api/state')
    if s != 200: bad += 1
check('150 sequential GET /api/state (new connection each)', bad == 0, bad)
print('   %.1f ms per request' % ((time.time() - t0) / 150 * 1000))
bad = 0
for i in range(100):
    s, j, _ = req('GET', '/api/set?ch4.pwm=%d' % (i % 100))
    if s != 200: bad += 1
check('100 sequential GET set', bad == 0, bad)
errs = []
def worker(n):
    for i in range(40):
        try:
            s, j, _ = req('GET', '/api/state' if i % 2 else '/api/set?ch4.pwm=%d' % (i + n))
            if s != 200: errs.append(s)
        except Exception as e:
            errs.append(repr(e))
ths = [threading.Thread(target=worker, args=(n,)) for n in range(4)]
[t.start() for t in ths]; [t.join() for t in ths]
check('4 threads x 40 mixed requests', not errs, errs[:5])
time.sleep(0.5)
final = state()
check('device alive after the load', final and final['ok'])

# ---------------------------------------------------------------- restore
restore = {k['k']: orig[k['k']] for k in writable if k['k'] != 'ble.toy'}
for chunk in (list(restore.items())[i:i + 20] for i in range(0, len(restore), 20)):
    s, j, _ = req('POST', '/api/set', dict(chunk))
    check('restore chunk', s == 200, (s, j))
settle(); st = state()
diff = {k: (orig[k], st['d'][k]) for k in orig if k in restore and orig[k] != st['d'][k]}
check('state restored', not diff, diff)
print('\nresult: %d passed, %d failed, %d skipped%s' % (ok, fail, skipped, '' if COLLAR else ' (run with --collar to include the collar checks)'))
sys.exit(1 if fail else 0)
