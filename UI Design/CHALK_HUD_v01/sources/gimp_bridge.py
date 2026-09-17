"""Task-local client for the installed GIMP MCP plugin (localhost only)."""
import json
import socket
import sys
from pathlib import Path


def request(payload):
    with socket.create_connection(("127.0.0.1", 9877), timeout=5) as sock:
        sock.settimeout(60)
        sock.sendall(json.dumps(payload).encode("utf-8") + b"\n")
        data = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data += chunk
            try:
                result = json.loads(data.decode("utf-8"))
                return result
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue
    raise RuntimeError("GIMP closed the connection before returning complete JSON")


if __name__ == "__main__":
    if sys.argv[1] == "execute":
        payload = {"cmds": [Path(sys.argv[2]).read_text(encoding="utf-8")]}
    else:
        payload = {"type": sys.argv[1], "params": json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}}
    print(json.dumps(request(payload), ensure_ascii=True, indent=2))
