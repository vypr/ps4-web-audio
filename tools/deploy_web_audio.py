#!/usr/bin/env python3
"""Load the separate PS4 Web Audio payload in RAM through GoldHEN's port 9090.

Stops an active old Web Audio on port 8098. By default also stops the prior
Bluetooth Audio worker on port 8097 so PS4 Bluetooth stays out of the path.
NEVER changes AutoRun, package installs, or saved V14 rollback files.
"""
import argparse
import ftplib
import io
import json
import socket
import sys
import time
from pathlib import Path


def open_port(host, port, timeout=1):
    # Only probe HTTP listening ports. NEVER probe payloader port 9090 by itself.
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def ftp_stop(host, sentinel):
    try:
        with ftplib.FTP() as ftp:
            ftp.connect(host, 2121, timeout=8)
            ftp.login()
            ftp.storbinary(f"STOR {sentinel}", io.BytesIO(b"stop\n"))
        return True
    except ftplib.all_errors as e:
        print(f"Could not request stop for {sentinel}: {e}")
        return False


def stop_and_wait(host, port, sentinel, name):
    if not open_port(host, port):
        print(f"{name}: not running on {port}")
        return
    print(f"Stopping {name} (RAM worker only; saved payload unchanged)...")
    if ftp_stop(host, sentinel):
        for _ in range(18):
            if not open_port(host, port):
                print(f"{name}: stopped")
                return
            time.sleep(0.5)
    if open_port(host, port):
        print(f"WARNING: {name} still listens on {port}. Check service or reboot before retrying.")


def http_status(host):
    try:
        with socket.create_connection((host, 8098), timeout=3) as s:
            s.settimeout(3)
            s.sendall((f"GET /status HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n").encode())
            data=b''
            while len(data)<4096:
                d=s.recv(4096)
                if not d:break
                data+=d
        if b'\r\n\r\n' not in data:return None
        return json.loads(data.split(b'\r\n\r\n',1)[1])
    except (OSError, ValueError):
        return None


def main():
    parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('payload',type=Path)
    parser.add_argument('--host',default='192.168.1.104')
    parser.add_argument('--keep-bluetooth',action='store_true',help='Diagnostic only: leave BT Audio running; its HDMI mute may affect capture')
    args=parser.parse_args()
    if not args.payload.is_file():sys.exit(f'Missing payload: {args.payload}')
    data=args.payload.read_bytes()
    if len(data)<1024 or len(data)>1024*1024:sys.exit('Payload size is outside expected bounds')
    print(f'PS4 Web Audio V2: {len(data)}-byte standalone payload')
    print('This does not change GoldHEN AutoRun or your saved V14 files.')
    stop_and_wait(args.host,8098,'/data/ps4_web_audio/stop','Earlier Web Audio')
    if not args.keep_bluetooth:
        stop_and_wait(args.host,8097,'/data/bta_audio/stop','Bluetooth Audio V14/V15')
    # GoldHEN BinLoader consumes the first connection. Open only when sending.
    for attempt in range(12):
        try:
            with socket.create_connection((args.host,9090),timeout=6) as s:
                s.settimeout(2)
                s.sendall(data)
                try:s.shutdown(socket.SHUT_WR)
                except OSError:pass
                try:s.recv(64)
                except OSError:pass
            break
        except OSError as exc:
            if attempt==0:print(f'GoldHEN port 9090: {exc}; retrying if listener not ready')
            time.sleep(2)
    else:sys.exit('GoldHEN Payload Server is not accepting; enable it on PS4 (port 9090).')
    print('Payload sent. Checking http://'+args.host+':8098/status ...')
    for _ in range(25):
        s=http_status(args.host)
        if s:
            print('Verified:',json.dumps(s,indent=2))
            print(f'OPEN ON YOUR PC: http://{args.host}:8098/')
            print('Select your PC Bluetooth headphones, then click Listen to PS4.')
            return
        time.sleep(0.4)
    print('No status reply yet. Check PS4 notification, port 8098 and network/GoldHEN settings.')
    sys.exit(1)

if __name__=='__main__': main()
