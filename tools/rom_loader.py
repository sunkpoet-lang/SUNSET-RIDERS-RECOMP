"""Read-only Sunset Riders EAC loader. No CPU execution or ROM extraction."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import zlib


class SetError(ValueError):
    pass


def read_verified(archive: Path, specs: list[dict]) -> dict[str, bytes]:
    """Reject partial, ambiguous, wrong-sized and corrupt sets before assembly."""
    result = {}
    try:
        with zipfile.ZipFile(archive) as zipped:
            for expected in specs:
                name = expected['name']
                matches = [i for i in zipped.infolist()
                           if not i.is_dir() and i.filename.replace('\\', '/').rsplit('/', 1)[-1] == name]
                if len(matches) != 1:
                    raise SetError(f'{name}: expected exactly one member, found {len(matches)}')
                entry = matches[0]
                if entry.file_size != expected['size']:
                    raise SetError(f'{name}: wrong size {entry.file_size}, expected {expected["size"]}')
                with zipped.open(entry) as stream:
                    data = stream.read(expected['size'] + 1)
                if len(data) != expected['size']:
                    raise SetError(f'{name}: truncated or oversized data')
                crc = f'{zlib.crc32(data) & 0xffffffff:08x}'
                sha = hashlib.sha1(data).hexdigest()
                if crc != expected['crc32'] or sha != expected['sha1']:
                    raise SetError(f'{name}: CRC32/SHA-1 mismatch')
                result[name] = data
    except (zipfile.BadZipFile, RuntimeError, OSError) as error:
        raise SetError(f'{archive.name}: {error}') from error
    return result


def assemble_lanes(parts: list[tuple[bytes, int, int]], size: int, group: int, stride: int) -> bytes:
    """Place group-byte chunks in bus lanes; reject holes, overlaps and bounds errors."""
    if size <= 0 or group <= 0 or stride < group:
        raise SetError('invalid region geometry')
    output = bytearray(size)
    occupied = bytearray(size)
    for data, offset, expected_size in parts:
        if offset < 0 or len(data) != expected_size or len(data) % group:
            raise SetError('invalid lane data size or offset')
        for lane in range(group):
            chunk = data[lane::group]
            start = offset + lane
            end = start + (len(chunk) - 1) * stride if chunk else start
            if not chunk or end >= size:
                raise SetError('lane exceeds region')
            positions = slice(start, end + 1, stride)
            if any(occupied[positions]):
                raise SetError('overlapping ROM lanes')
            output[positions] = chunk
            occupied[positions] = bytes([1]) * len(chunk)
    if 0 in occupied:
        raise SetError('uninitialised ROM lane')
    return bytes(output)


def assemble_set(members: dict[str, bytes]) -> dict[str, bytes]:
    return {
        'maincpu': assemble_lanes([
            (members['064eac02.8e'], 0, 0x40000),
            (members['064eac03.8g'], 1, 0x40000),
            (members['064eab04.10e'], 0x80000, 0x20000),
            (members['064eab05.10g'], 0x80001, 0x20000)], 0xc0000, 1, 2),
        'k052109': assemble_lanes([
            (members['064e12.16k'], 0, 0x80000),
            (members['064e11.12k'], 2, 0x80000)], 0x100000, 2, 4),
        'k053245': assemble_lanes([
            (members['064e09.7l'], 0, 0x100000),
            (members['064e07.3l'], 2, 0x100000)], 0x200000, 2, 4),
        'audiocpu': bytes(members['064e01.2f']),
        'k053260': bytes(members['064e06.1d']),
        'eeprom': bytes(members['ssriders_eac.nv']),
    }


def load_set(archive: Path):
    manifest_path = Path(__file__).with_name('set-eac.json')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    before = hashlib.sha256(archive.read_bytes()).hexdigest()
    members = read_verified(archive, manifest['members'])
    regions = assemble_set(members)
    after = hashlib.sha256(archive.read_bytes()).hexdigest()
    if before != after:
        raise SetError('archive changed during verification')
    report = {
        'schema': 'ssriders-regions-v1', 'set': 'ssriders', 'variant': '4 Players ver EAC',
        'status': 'regions_verified', 'zip_sha256': before, 'rom_read_only': True,
        'mame_commit': manifest['mame_commit'],
        'loader_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'manifest_sha256': hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        'regions': {name: {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                    for name, data in regions.items()},
        'runtime': 'not_implemented', 'game_boot': 'not_run',
    }
    return regions, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path, help='New metadata-only JSON output')
    args = parser.parse_args()
    try:
        _, report = load_set(args.rom)
        # Exclusive creation: never overwrite a ROM, profile or previous receipt.
        with args.report.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
    except (SetError, OSError) as error:
        parser.exit(1, f'{error}\n')
    print('EAC regions verified; CPU/game not executed. Metadata: ' + str(args.report))


if __name__ == '__main__':
    main()
