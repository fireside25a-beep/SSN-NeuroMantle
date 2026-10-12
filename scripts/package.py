#!/usr/bin/env python3
"""Build a deterministic source and native-binary ZIP with a SHA-256 manifest."""
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import sys
import zipfile

root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else root.parent/'NeuroMantle-0.3.0.zip'
source_epoch=int(os.environ.get('SOURCE_DATE_EPOCH','1791763200'))
stamp=datetime.fromtimestamp(source_epoch,timezone.utc)
if not 1980<=stamp.year<=2107:raise SystemExit('invalid SOURCE_DATE_EPOCH')
when=(stamp.year,stamp.month,stamp.day,stamp.hour,stamp.minute,stamp.second)
files=[]
for p in sorted(root.rglob('*')):
    if not p.is_file():continue
    rel=p.relative_to(root)
    parts=rel.parts
    if any(x in {'.git','build','__pycache__','lib'} for x in parts):continue
    if p.suffix in {'.pyc','.o','.a'} or p.name=='SOURCE_MANIFEST.sha256':continue
    if parts[0]=='bin' and rel.as_posix()!='bin/neuromantle':continue
    if parts[:3]==('vendor','devcli','bin') and rel.as_posix()!='vendor/devcli/bin/neuro':continue
    if p.stat().st_size==0:raise SystemExit('empty file: '+str(rel))
    files.append(p)
if not files:raise SystemExit('empty source tree')
manifest=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(root).as_posix()+'\n' for p in files)
(root/'SOURCE_MANIFEST.sha256').write_text(manifest)
files.append(root/'SOURCE_MANIFEST.sha256')
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(files):
        rel='NeuroMantle-0.3.0/'+p.relative_to(root).as_posix()
        info=zipfile.ZipInfo(rel,when)
        info.compress_type=zipfile.ZIP_DEFLATED
        info.external_attr=((0o100755 if p.stat().st_mode&0o111 else 0o100644)<<16)
        z.writestr(info,p.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
with zipfile.ZipFile(out) as z:
    if z.testzip() is not None:raise SystemExit('ZIP integrity failure')
print(hashlib.sha256(out.read_bytes()).hexdigest(),out.name,len(files),'files')
