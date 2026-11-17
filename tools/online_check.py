"""Online checks with your own ROM ZIP, on this PC (no network beyond 127.0.0.1, no window shown):

  trace    the main CPU in slices against stepping (SUNSET_LOCKSTEP), and with rollbacks every 7th frame: the same
           game, picture and sound; every state played again byte-identical to the one first saved
  seats    four headless PCs: a room of four starts with two, a third and a fourth join mid-game, three leave;
           every epoch's checkpoints equal on every PC that played it
  window   the online window, hidden, with made-up input: a host and a guest, a third PC with two players joins
           (four seats), checkpoints compared, pictures captured

  python tools/online_check.py --rom C:/my-roms/ssriders.zip [--build build-clean] [--only trace|seats|window]"""
from pathlib import Path
import argparse, json, os, random, re, subprocess, sys, time
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from rom_loader import load_set, SetError

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('--rom', required=True, type=Path)
p.add_argument('--build', type=Path, default=root / 'build-clean')
p.add_argument('--out', type=Path, default=None)
p.add_argument('--only', choices=['trace', 'seats', 'window'])
a = p.parse_args()
try:
    regions, _ = load_set(a.rom)
except (SetError, OSError) as e:
    sys.exit(str(e))
payload = b''.join(regions[n] for n in ['maincpu', 'eeprom', 'k053245', 'audiocpu', 'k053260', 'k052109'])
build = a.build.resolve()
out = (a.out or build / 'online-check').resolve(); out.mkdir(parents=True, exist_ok=True)
peer, window = build / 'sunset-seat-peer.exe', build / 'Sunset-Riders-Online.exe'
env = os.environ.copy(); env['PATH'] = str(build) + os.pathsep + env['PATH']
flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0

def epochs(logs):
    """Every epoch's checkpoints common to the PCs that played it (the last 16 frames left out: still unconfirmed)."""
    def read(path):
        r, epoch = {}, 1
        if not path.exists(): return r
        for line in path.read_text().splitlines():
            if line.startswith('# epoch'): epoch = int(line.split()[2]); continue
            v = line.split()
            if len(v) >= 2: r[(epoch, int(v[0]))] = v[1]
        return r
    data = {n: read(f) for n, f in logs.items()}
    result = []
    for epoch in sorted({e for d in data.values() for e, _ in d}):
        who = [n for n in data if any(e == epoch for e, _ in data[n])]
        keys = set.intersection(*[{k for k in data[n] if k[0] == epoch} for n in who])
        top = max((f for _, f in keys), default=0) - 16
        keys = [k for k in keys if k[1] <= top]
        result.append({'epoch': epoch, 'pcs': who, 'common_checkpoints': len(keys), 'differ': sum(1 for k in keys if len({data[n][k] for n in who}) > 1)})
    return result

def start(procs, name, folder, args):
    log = open(folder / f'{name}.txt', 'wb')
    q = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, env=env, creationflags=flags)
    q.stdin.write(payload); q.stdin.close()
    procs[name] = (q, log)

def finish(procs, timeout):
    for q, log in procs.values():
        try: q.wait(timeout=timeout)
        except subprocess.TimeoutExpired: q.kill(); q.wait()
        log.close()

def trace():
    folder = out / 'trace'; folder.mkdir(exist_ok=True)
    def norm(line):
        # (the game, the picture and the sound; diagnostic counters only are left out: the CPUs' greatest lead, where
        # the diagnostic recording began, the board clock latched at a video edge, the whole-state CRCs)
        for pattern in (r'sound [0-9a-f]{8} video [0-9a-f]{8}', r'board [0-9a-f]{8}', r'"max_sound_lead_ticks":\d+', r'"mixed_output_begin":\d+'):
            line = re.sub(pattern, '', line)
        v = line.split()
        if 'host' in v: del v[v.index('host') + 3]
        return ' '.join(v[:-2])
    runs = {}
    for name, extra in (('sliced', {}), ('stepped', {'SUNSET_LOCKSTEP': '1'}), ('rolled', {'SUNSET_ROLL': '1'})):
        e = dict(env, SUNSET_TRACE='1', **extra); log = folder / f'{name}.log'
        q = subprocess.run([str(peer), 'host', '1', '3600', '11', str(log)], input=payload, env=e, capture_output=True, creationflags=flags)
        runs[name] = {'exit': q.returncode, 'says': q.stderr.decode(errors='replace').strip()[-300:], 'lines': [norm(x) for x in log.read_text().splitlines()] if log.exists() else []}
    s, t, r = (runs[n]['lines'] for n in ('sliced', 'stepped', 'rolled'))
    return {'frames': 3600, 'checkpoints': len(s), 'sliced_equals_stepped': bool(s) and s == t,
            'rolled_equals_sliced': bool(r) and r == s and runs['rolled']['exit'] == 0,
            'says': {n: runs[n]['says'] for n in runs}, 'passed': bool(s) and s == t and r == s and runs['rolled']['exit'] == 0}

def seats():
    folder = out / 'seats'; folder.mkdir(exist_ok=True)
    port, procs = str(63500 + random.randint(0, 300)), {}
    run = lambda name, role, frames, seed, *x: start(procs, name, folder, [str(peer), role, port, str(frames), str(seed), str(folder / f'{name}.log'), *x])
    run('host', 'host', 2400, 1, 'late', 'start', '2'); time.sleep(1.5)
    run('guest', 'join', 900, 2, 'start', '2'); time.sleep(8)
    run('third', 'join', 1500, 3, 'start', '2'); time.sleep(6)
    run('fourth', 'join', 1200, 4, 'start', '2')
    host = procs.pop('host'); finish(procs, 420)
    # (a host left alone waits for players, as it should: once every guest is done, the check stops it)
    try: host[0].wait(timeout=15)
    except subprocess.TimeoutExpired: host[0].kill(); host[0].wait()
    host[1].close(); procs['host'] = host
    e = epochs({n: folder / f'{n}.log' for n in procs})
    return {'epochs': e, 'passed': bool(e) and len(e) >= 4 and all(x['differ'] == 0 for x in e)}

def windows():
    folder = out / 'window'; folder.mkdir(exist_ok=True)
    for old in folder.glob('replays-*/*.fareplay'): old.unlink()
    port, procs = str(63800 + random.randint(0, 300)), {}
    def run(name, frames, seed, *x):
        start(procs, name, folder, [str(window), *x, '--name', name.capitalize(), '--auto', str(seed), '--frames', str(frames), '--loopback',
                                     '--log', str(folder / f'{name}.log'), '--capture', str(folder / name), '--replays', str(folder / f'replays-{name}')])
    run('host', 2400, 1, '--host', port, '--players', '4', '--start', '2', '--late'); time.sleep(1.5)
    run('guest', 2400, 2, '--join', f'127.0.0.1:{port}', '--players', '4', '--start', '2'); time.sleep(10)
    run('third', 1500, 3, '--join', f'127.0.0.1:{port}', '--players', '4', '--start', '2', '--locals', '2')
    finish(procs, 300)
    e = epochs({n: folder / f'{n}.log' for n in procs})
    captures = sorted(f.name for f in folder.glob('*.bmp'))
    exits = {n: q.returncode for n, (q, _) in procs.items()}
    # Every window's replay of the whole match, played back headless: to its end, every checksum matching
    replays = {}
    for name in procs:
        files = sorted((folder / f'replays-{name}').glob('*.fareplay'))
        if not files: replays[name] = 'missing'; continue
        q = subprocess.run([str(window), '--replay', str(files[-1]), '--headless'], input=payload, env=env, capture_output=True, creationflags=flags)
        said = [l for l in q.stderr.decode(errors='replace').splitlines() if l.startswith('replay:')]
        replays[name] = f'{files[-1].name}: ' + (said[-1] if said else f'exit {q.returncode}')
    played = all('played to its end' in v and 'matched' in v for v in replays.values())
    return {'epochs': e, 'exits': exits, 'captures': captures, 'replays': replays,
            'passed': bool(e) and all(x['differ'] == 0 for x in e) and all(v == 0 for v in exits.values()) and len(captures) >= 4 and played}

report = {}
for name, check in (('trace', trace), ('seats', seats), ('window', windows)):
    if a.only and a.only != name: continue
    began = time.monotonic(); report[name] = check(); report[name]['seconds'] = round(time.monotonic() - began, 1)
    print(f"{name}: {'passed' if report[name]['passed'] else 'FAILED'} ({report[name]['seconds']} s)", flush=True)
report['passed'] = all(v['passed'] for v in report.values())
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
sys.exit(0 if report['passed'] else 1)
