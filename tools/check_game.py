"""Optional user-ROM regression. Writes private diagnostic captures under a new output directory."""
import argparse, hashlib, json, subprocess, time
from pathlib import Path
from rom_loader import load_set

def main():
    root = Path(__file__).resolve().parents[1]
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', required=True, type=Path)
    p.add_argument('--exe', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    before = hashlib.sha256(a.rom.read_bytes()).hexdigest()
    regions, _ = load_set(a.rom)
    data = b''.join(regions[n] for n in ['maincpu','eeprom','k053245','audiocpu','k053260','k052109'])
    capture = a.output.resolve()/'four-player'
    args = [str(a.exe.resolve()), '--window','--menu-ui-test','--bar-ui-test','--scenario',str(root/'tests/four-player.txt'),'--frames','1500','--ui-test','--capture',str(capture)]
    start = time.monotonic()
    run = subprocess.run(args,input=data,capture_output=True,timeout=180)
    (a.output/'stdout.log').write_bytes(run.stdout)
    (a.output/'stderr.log').write_bytes(run.stderr)
    if run.returncode: raise RuntimeError(run.stderr.decode(errors='replace'))
    result = json.loads(run.stdout)
    expected = json.loads((root/'tests/native-reference.json').read_text())
    mismatches = {k: [v,result.get(k)] for k,v in expected.items() if result.get(k)!=v}
    if mismatches: raise RuntimeError('native regression mismatch: '+str(mismatches))
    image_sha = hashlib.sha256(Path(str(capture)+'.ppm').read_bytes()).hexdigest()
    if image_sha != '4cc295f12f0d6aa5984e414db3875642e50901abe7221e7c0951a6c5d2b6c5de':
        raise RuntimeError('native frame differs from accepted r51 master')
    if hashlib.sha256(a.rom.read_bytes()).hexdigest()!=before: raise RuntimeError('ROM changed')
    summary = {'status':'passed','runtime':result,'exe_sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),'native_ppm_sha256':image_sha,'rom_unchanged':True,'wall_seconds':round(time.monotonic()-start,2)}
    (a.output/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__': main()
