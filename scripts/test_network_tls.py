"""Test TLS trust/host validation against temporary loopback HTTPS servers.

Requires Python cryptography and a network test binary built with TLS enabled.
"""
import argparse
from contextlib import ExitStack
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
from threading import Thread

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test", type=Path, required=True)
    args = parser.parse_args()
    test = args.test.resolve(strict=True)
    now = datetime.now(timezone.utc)
    with tempfile.TemporaryDirectory(prefix="szx-network-tls-") as temporary, ExitStack() as resources:
        root = Path(temporary)
        ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Temporary network test CA")])
        ca = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(ca_key.public_key())
              .serial_number(x509.random_serial_number()).not_valid_before(now - timedelta(minutes=5))
              .not_valid_after(now + timedelta(days=1)).add_extension(x509.BasicConstraints(ca=True, path_length=0), True)
              .sign(ca_key, hashes.SHA256()))
        ca_path = root / "ca.pem"
        ca_path.write_bytes(ca.public_bytes(serialization.Encoding.PEM))

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def do_GET(self):
                self.send_response(200)
                self.send_header("Content-Length", "6")
                self.end_headers()
                self.wfile.write(b"tls-ok")

            def log_message(self, *args):
                pass

        urls = []
        for index, host in enumerate(("127.0.0.1", "127.0.0.2")):
            key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
            cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, host)]))
                    .issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number())
                    .not_valid_before(now - timedelta(minutes=5)).not_valid_after(now + timedelta(days=1))
                    .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address(host))]), False)
                    .sign(ca_key, hashes.SHA256()))
            cert_path, key_path = root / f"cert-{index}.pem", root / f"key-{index}.pem"
            cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
            key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                                   serialization.NoEncryption()))
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(cert_path, key_path)
            server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            resources.callback(server.server_close)
            server.socket = context.wrap_socket(server.socket, server_side=True)
            worker = Thread(target=server.serve_forever, daemon=True)
            worker.start()
            resources.callback(worker.join, 5)
            resources.callback(server.shutdown)
            urls.append(f"https://127.0.0.1:{server.server_port}/")
        environment = {**os.environ, "SZX_TEST_HTTPS_URL": urls[0], "SZX_TEST_HTTPS_WRONG_HOST_URL": urls[1],
                       "SZX_TEST_HTTPS_CA": str(ca_path)}
        result = subprocess.run([str(test), "--gtest_filter=HttpClientTlsTest.*"], env=environment,
                                cwd=root, text=True, capture_output=True, timeout=45)
        print(result.stdout)
        print(result.stderr)
        assert result.returncode == 0 and "[  PASSED  ] 1 test." in result.stdout, "TLS test did not pass"


if __name__ == "__main__":
    main()
