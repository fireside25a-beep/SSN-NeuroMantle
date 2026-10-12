#!/usr/bin/env python3
"""Build a byte-reproducible Vanta source and binary archive."""
import datetime
import hashlib
import os
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else ROOT.parent / 'Vanta-Native-1.2.1.zip'
EPOCH = int(os.environ.get('SOURCE_DATE_EPOCH', '1791676800'))
DATE = datetime.datetime.fromtimestamp(EPOCH, datetime.timezone.utc)
if not 1980 <= DATE.year <= 2107:
    raise SystemExit('SOURCE_DATE_EPOCH is outside ZIP timestamp range')
TIMESTAMP = (DATE.year, DATE.month, DATE.day, DATE.hour, DATE.minute, DATE.second)
FILES = sorted(path for path in ROOT.rglob('*') if path.is_file()
               and 'build' not in path.relative_to(ROOT).parts
               and path.name != 'SOURCE_MANIFEST.sha256')
if not FILES or any(path.stat().st_size == 0 for path in FILES):
    raise SystemExit('empty source tree or empty file')
manifest = ''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  '
                   f'{path.relative_to(ROOT).as_posix()}\n' for path in FILES)
(ROOT / 'SOURCE_MANIFEST.sha256').write_text(manifest)
FILES.append(ROOT / 'SOURCE_MANIFEST.sha256')
OUT.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(OUT, 'w', compression=zipfile.ZIP_DEFLATED,
                     compresslevel=9) as archive:
    for path in sorted(FILES):
        name = 'Vanta-Native-1.2.1/' + path.relative_to(ROOT).as_posix()
        info = zipfile.ZipInfo(name, TIMESTAMP)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = ((0o100755 if path.stat().st_mode & 0o111 else 0o100644) << 16)
        archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                         compresslevel=9)
with zipfile.ZipFile(OUT) as archive:
    if archive.testzip() is not None:
        raise SystemExit('archive verification failed')
print(f'{hashlib.sha256(OUT.read_bytes()).hexdigest()}  {OUT.name}')
