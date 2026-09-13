"""Local client for the already-running Blender MCP add-on. No external services."""
import argparse
import json
from pathlib import Path
import socket

parser = argparse.ArgumentParser()
parser.add_argument('command', choices=['scene', 'object', 'execute', 'screenshot'])
parser.add_argument('argument', nargs='?')
parser.add_argument('--output')
args = parser.parse_args()
if args.command == 'scene':
    request = {'type': 'get_scene_info', 'params': {}}
elif args.command == 'object':
    request = {'type': 'get_object_info', 'params': {'name': args.argument}}
elif args.command == 'execute':
    request = {'type': 'execute_code', 'params': {'code': Path(args.argument).read_text(encoding='utf-8-sig')}}
else:
    request = {'type': 'get_viewport_screenshot', 'params': {'filepath': args.argument, 'max_size': 1600}}
with socket.create_connection(('127.0.0.1', 9876), timeout=15) as client:
    client.settimeout(60)
    client.sendall(json.dumps(request).encode())
    received = b''
    while True:
        packet = client.recv(65536)
        if not packet:
            raise RuntimeError('Blender closed the connection before a complete response')
        received += packet
        try:
            response = json.loads(received)
            break
        except (ValueError, UnicodeDecodeError):
            if len(received) > 16 * 1024 * 1024:
                raise RuntimeError('Unexpectedly large Blender response')
if args.output:
    Path(args.output).write_text(json.dumps(response, indent=2), encoding='utf-8')
print(json.dumps(response))
if response.get('status') != 'success':
    raise SystemExit(1)
