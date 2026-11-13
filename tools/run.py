"""Launch the experimental EAC runtime using a separately supplied, verified ROM ZIP."""
from pathlib import Path
import argparse, subprocess, sys
from rom_loader import load_set, SetError

def main():
    root = Path(__file__).resolve().parents[1]
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', required=True, type=Path)
    p.add_argument('--exe', type=Path, default=root/'build/Sunset-Riders-Lab.exe')
    p.add_argument('runtime_args', nargs=argparse.REMAINDER)
    a = p.parse_args()
    try:
        regions, _ = load_set(a.rom)
        payload = b''.join(regions[n] for n in ['maincpu','eeprom','k053245','audiocpu','k053260','k052109'])
        extra = a.runtime_args
        if extra[:1] == ['--']: extra = extra[1:]
        if not extra: extra = ['--window']
        result = subprocess.run([str(a.exe.resolve()), *extra], input=payload)
        return result.returncode
    except (SetError, OSError) as e:
        print(str(e), file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
