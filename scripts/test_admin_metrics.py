"""Machine metrics HTTP/RBAC smoke test on a disposable loopback database.

Usage: python scripts/test_admin_metrics.py --service build/bin/szx_admin_service
Requires mysql/mysqld (MySQL 8 or compatible MariaDB) and Python cryptography.
Never connects to the existing application database or service.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from test_admin_database import free_port, running, wait_until


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--service", type=Path, required=True)
    parser.add_argument("--rbac", action="store_true", help="Also verify the custom permission lifecycle")
    args = parser.parse_args()
    service = args.service.resolve(strict=True)
    mysql, mysqld = shutil.which("mysql"), shutil.which("mysqld")
    if not mysql or not mysqld:
        parser.error("mysql and mysqld are required")
    maria = "MariaDB" in subprocess.check_output([mysqld, "--version"], text=True)
    with tempfile.TemporaryDirectory(prefix="szx-metrics-test-") as temporary:
        root = Path(temporary)
        data = root / "data"
        if maria:
            installer = shutil.which("mariadb-install-db")
            if not installer:
                parser.error("mariadb-install-db is required for MariaDB")
            initialize = [installer, "--no-defaults", f"--datadir={data}",
                          "--auth-root-authentication-method=normal", "--skip-test-db"]
        else:
            initialize = [mysqld, "--no-defaults", "--initialize-insecure", f"--datadir={data}"]
        result = subprocess.run(initialize, capture_output=True, text=True, timeout=90)
        if result.returncode:
            raise RuntimeError(result.stderr)
        database_port, http_port = free_port(), free_port()
        client = [mysql, "--no-defaults", "--protocol=TCP", "--host=127.0.0.1",
                  f"--port={database_port}", "--user=root", "--batch", "--skip-column-names"]

        def sql(statement):
            result = subprocess.run(client, input=statement, text=True, capture_output=True, timeout=20)
            if result.returncode:
                raise RuntimeError(result.stderr)
            return result.stdout.strip()

        def database_ready():
            try:
                return sql("SELECT 1;") == "1"
            except RuntimeError:
                return False

        command = [mysqld, "--no-defaults", f"--datadir={data}", "--bind-address=127.0.0.1",
                   f"--port={database_port}", f"--socket={root / 'db.sock'}", f"--pid-file={root / 'db.pid'}"]
        if not maria:
            command.append("--mysqlx=OFF")
        with running(command, root, root / "database.log") as database:
            wait_until(database_ready, database)
            sql("CREATE DATABASE metrics_test CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci;")
            key = rsa.generate_private_key(public_exponent=65537, key_size=3072)
            private, public = root / "private.pem", root / "public.pem"
            private.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                                  serialization.NoEncryption()))
            public.write_bytes(key.public_key().public_bytes(serialization.Encoding.PEM,
                                                            serialization.PublicFormat.SubjectPublicKeyInfo))
            config = root / "config.yaml"
            config.write_text(
                f"logging:\n  console: false\n  filePath: {json.dumps(str(root / 'service.log'))}\n"
                f"server:\n  host: 127.0.0.1\n  port: {http_port}\n  workerThreads: 2\n"
                f"  staticRoot: {json.dumps(str(service.parent / 'web'))}\n"
                f"auth:\n  privateKeyPath: {json.dumps(str(private))}\n  publicKeyPath: {json.dumps(str(public))}\n"
                "  allowedOrigin: '*'\n  attemptsPerMinute: 200\n"
                f"database:\n  host: 127.0.0.1\n  port: {database_port}\n  database: metrics_test\n"
                "  username: root\n  password: ''\n  poolSize: 3\nmetrics:\n  diskPaths: []\n", encoding="utf-8")

            def call(method, path, payload=None, token=None, extra=None):
                headers = {"Content-Type": "application/json", **(extra or {})}
                if token:
                    headers["Authorization"] = "Bearer " + token
                request = urllib.request.Request(f"http://127.0.0.1:{http_port}/api/v1{path}",
                    data=None if payload is None else json.dumps(payload).encode(), method=method, headers=headers)
                try:
                    response = urllib.request.urlopen(request, timeout=5)
                except urllib.error.HTTPError as error:
                    response = error
                with response:
                    raw = response.read()
                    return response.status, json.loads(raw) if raw else {}, response.headers

            with running([str(service), "--config", str(config)], root, root / "process.log") as process:
                def ready():
                    try:
                        return call("GET", "/ready")[0] == 200
                    except urllib.error.URLError:
                        return False
                try:
                    wait_until(ready, process)
                except Exception:
                    print((root / "process.log").read_text())
                    if (root / "service.log").exists():
                        print((root / "service.log").read_text())
                    raise
                assert sql("SELECT MAX(version) FROM metrics_test.schema_migrations;") == "11"
                # Only in this disposable DB: disable the bootstrap password-change gate for test login.
                sql("UPDATE metrics_test.users SET force_password_change=FALSE;")
                digest = lambda value: hashlib.sha256(value.encode()).hexdigest()

                def login(identifier, password):
                    status, body, _ = call("POST", "/auth/login", {"login_identifier": identifier, "password_hash": digest(password)})
                    assert status == 200, body
                    return body["data"]["access_token"]

                token = login("admin", "superadmin")
                for role in ["operator", "engineer", "admin"]:
                    status, body, _ = call("POST", "/admin/users", {
                        "login_identifier": "metrics_" + role, "display_name": role, "role": role,
                        "password_hash": digest("MetricsTest123")}, token)
                    assert status == 201, body
                    role_token = login("metrics_" + role, "MetricsTest123")
                    for path in ["/admin/system/metrics", "/admin/system/metrics/history"]:
                        status, body, _ = call("GET", path, token=role_token)
                        assert status == (200 if role == "admin" else 403), (role, status, body)
                for path in ["/admin/system/metrics", "/admin/system/metrics/history"]:
                    status, body, _ = call("GET", path)
                    assert status == 401 and body["code"] == 2200, body
                for query in ["window_seconds=0", "window_seconds=901", "window_seconds=-1",
                              "window_seconds=1.5", "window_seconds=", "window_seconds=abc", "unknown=1"]:
                    status, body, _ = call("GET", "/admin/system/metrics/history?" + query, token=token)
                    assert status == 422 and body["code"] == 2206, body
                status, body, _ = call("GET", "/admin/system/metrics?path=/", token=token)
                assert status == 422, body
                time.sleep(2.2)
                status, body, headers = call("GET", "/admin/system/metrics", token=token)
                assert status == 200 and headers["Cache-Control"] == "no-store", body
                current = body["data"]
                assert current["cpu"]["status"] == "ok" and 0 <= current["cpu"]["usage_percent"] <= 100
                assert current["memory"]["status"] == "ok" and current["memory"]["free_bytes"] is None
                assert len(current["disks"]) <= 32
                status, body, _ = call("GET", "/admin/system/metrics/history?window_seconds=900", token=token)
                assert status == 200 and body["data"]["instance_id"] == current["instance_id"], body
                assert 2 <= len(body["data"]["samples"]) <= 450
                status, document, _ = call("GET", "/openapi.json")
                assert status == 200
                assert "/admin/system/metrics" in document["paths"]
                print("PASS: migration 11, real metrics/history, default role RBAC, strict queries and deployed OpenAPI")
                if args.rbac:
                    from test_admin_permissions import run_permission_checks
                    run_permission_checks(call, sql, login, token)
                    from test_admin_rbac_concurrency import run_rbac_concurrency_checks
                    run_rbac_concurrency_checks(call, sql, token, service, config)


if __name__ == "__main__":
    main()
