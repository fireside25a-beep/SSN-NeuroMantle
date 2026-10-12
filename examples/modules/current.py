#!/usr/bin/env python3
"""Deterministic module: compute next-tick drive from an admitted spike route."""
import sys
if len(sys.argv)!=4:
    raise SystemExit(2)
tick,source,target=map(int,sys.argv[1:])
if min(tick,source,target)<0:
    raise SystemExit(2)
print(9+(tick+source+target)%3)
