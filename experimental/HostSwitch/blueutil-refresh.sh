#!/bin/sh
# Optional local backend wrapper: refresh inquiry data only before explicit pairing.
backend="$HOME/Library/Application Support/GlideMouseSwitch/backend/blueutil"
if [ "$1" = "--pair" ]; then
 /usr/bin/perl -e 'alarm 5; exec @ARGV' "$backend" --format json --inquiry 3 2>/dev/null | /usr/bin/python3 -c 'import sys,json,os,re
try:
 d=json.load(sys.stdin);d=[d] if isinstance(d,dict) else d;t=os.environ.get("GLIDEMOUSE_APPROVE_TARGET","");found=any(re.sub("[:-]","",x.get("address","")).upper()==t for x in d);print("Configured mouse discovery seen="+str(found),file=sys.stderr)
except Exception: print("Configured mouse discovery metadata unavailable",file=sys.stderr)'
fi
exec "$backend" "$@"
