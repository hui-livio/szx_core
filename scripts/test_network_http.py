"""Exercise the HTTP client against independent raw loopback protocol fixtures."""
import argparse
import os
from pathlib import Path
import socketserver
import subprocess
from threading import Thread
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test", type=Path, required=True)
    args = parser.parse_args()
    responses = {
        b"/chunked": b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n",
        b"/eof": b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nhello",
        b"/truncated": b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\nshort",
        b"/malformed": b"NOT-HTTP\r\n\r\n",
        b"/cookies": b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\nSet-Cookie: first=1; HttpOnly\r\nSet-Cookie: second=2; Secure\r\nConnection: close\r\n\r\n",
    }
    hints = b"HTTP/1.1 103 Early Hints\r\nLink: </a.css>; rel=preload\r\nSet-Cookie: interim=1\r\n\r\n"
    final = b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello"
    responses.update({
        b"/hints-combined": hints + final,
        b"/hints-truncated": hints + b"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nhi",
        b"/hints-only": hints,
        b"/not-modified-small": b"HTTP/1.1 304 Not Modified\r\nContent-Length: 10\r\n\r\n",
        b"/not-modified-large": b"HTTP/1.1 304 Not Modified\r\nContent-Length: 100\r\n\r\n",
        b"/no-content": b"HTTP/1.1 204 No Content\r\n\r\n",
    })

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(5)
            request = b""
            while b"\r\n\r\n" not in request:
                block = self.request.recv(4096)
                if not block:
                    return
                request += block
                if len(request) > 65536:
                    return
            path = request.split(b" ", 2)[1]
            if path == b"/hints-split":
                self.request.sendall(hints)
                time.sleep(0.15)
                self.request.sendall(b"HTTP/1.1 100 Continue\r\n\r\n")
                time.sleep(0.15)
                self.request.sendall(final)
                return
            self.request.sendall(responses[path])

    with socketserver.ThreadingTCPServer(("127.0.0.1", 0), Handler) as server:
        worker = Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            environment = {**os.environ, "SZX_TEST_HTTP_WIRE_URL": f"http://127.0.0.1:{server.server_address[1]}"}
            test = args.test.resolve(strict=True)
            result = subprocess.run([str(test), "--gtest_filter=HttpClientWireTest.*"],
                                    cwd=test.parent, env=environment, capture_output=True, text=True, timeout=30)
            print(result.stdout)
            print(result.stderr)
            assert result.returncode == 0 and "[  PASSED  ] 1 test." in result.stdout, "Wire test did not pass"
        finally:
            server.shutdown()
            worker.join(5)


if __name__ == "__main__":
    main()
