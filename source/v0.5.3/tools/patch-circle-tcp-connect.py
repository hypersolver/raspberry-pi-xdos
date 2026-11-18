#!/usr/bin/env python3
"""Patch Circle TCP Connect to bound SYN wait to 5 seconds.

Only modifies lib/net/tcpconnection.cpp and lib/net/transportlayer.cpp.
Creates .xdos-v04.bak backups before the first change and is idempotent.
Fails closed if upstream method layout changed.
"""
from pathlib import Path
import sys
import re

if len(sys.argv) != 2:
    sys.exit('usage: patch-circle-tcp-connect.py /path/to/circle')
root=Path(sys.argv[1]).resolve()

def patch(path,method_start,method_end,old,new):
    file=root/path
    original=file.read_text()
    begin=original.find(method_start)
    end=original.find(method_end,begin+len(method_start))
    if begin<0 or end<0:
        sys.exit(f'ERROR: expected Circle function not found in {path}; nothing changed')
    method=original[begin:end]
    if new in method:
        print(f'ALREADY PATCHED: {path}')
        return
    if old.startswith('REGEX:'):
        pattern=old[len('REGEX:'):]
        matches=list(re.finditer(pattern,method,re.MULTILINE))
        if len(matches)!=1:
            sys.exit(f'ERROR: expected one Circle pattern in {path}, found {len(matches)}; nothing changed')
        method=re.sub(pattern,lambda m: new,method,count=1,flags=re.MULTILINE)
    else:
        if method.count(old)!=1:
            sys.exit(f'ERROR: unexpected Circle version/layout in {path}; nothing changed')
        method=method.replace(old,new,1)
    changed=original[:begin]+method+original[end:]
    backup=file.with_name(file.name+'.xdos-v04.bak')
    if not backup.exists():
        backup.write_text(original)
    file.write_text(changed)
    print(f'PATCHED: {path} (backup: {backup.name})')

patch(Path('lib/net/tcpconnection.cpp'),
      'int CTCPConnection::Connect (void)',
      'int CTCPConnection::Accept (',
      'm_Event.Wait ();',
      '''if (m_Event.WaitWithTimeout (5U * 1000000U))
        {
            // Bounded SYN handshake. Caller closes the failed connection.
            return -NET_ERROR_CONNECTION_TIMED_OUT;
        }''')
patch(Path('lib/net/transportlayer.cpp'),
      'int CTransportLayer::Connect (',
      'int CTransportLayer::Listen (',
      r'REGEX:if \(nResult < 0\)\s*\{\s*return nResult;\s*\}',
      '''if (nResult < 0)
    {
        // Avoid leaking a failed SYN connection on repeated page loads.
        ((CNetConnection *) m_pConnection[i])->Close ();
        return nResult;
    }''')
