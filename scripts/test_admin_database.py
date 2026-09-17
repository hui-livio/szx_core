"""Exercise admin initialization against a disposable, loopback-only MySQL 8 instance.

Usage: python scripts/test_admin_database.py --service build/bin/szx_admin_service.exe
Requires mysql and mysqld on PATH. Never connects to an existing database server.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, contextmanager
import json
import itertools
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def wait_until(check, process, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Process exited before readiness: {process.returncode}")
        if check():
            return
        time.sleep(0.1)
    raise TimeoutError("Readiness deadline exceeded")


@contextmanager
def running(command, cwd, log):
    with log.open("wb") as output:
        process = subprocess.Popen(
            command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        try:
            yield process
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--service", type=Path, required=True)
    args = parser.parse_args()
    service = args.service.resolve(strict=True)
    mysql = shutil.which("mysql")
    mysqld = shutil.which("mysqld")
    if not mysql or not mysqld:
        parser.error("mysql and mysqld must be on PATH")

    with tempfile.TemporaryDirectory(prefix="szx-admin-db-") as temporary:
        root = Path(temporary)
        package = root / "package"
        package.mkdir()
        shutil.copy2(service, package / service.name)
        for library in service.parent.glob("*.dll"):
            shutil.copy2(library, package / library.name)
        assert not list(package.rglob("*.sql"))
        assert not (package / "migrations").exists()
        data = root / "data"
        initialized = subprocess.run(
            [mysqld, "--no-defaults", "--initialize-insecure", f"--datadir={data}"],
            capture_output=True, text=True, timeout=90,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        if initialized.returncode:
            raise RuntimeError(initialized.stderr)
        port = free_port()
        client = [mysql, "--no-defaults", "--protocol=TCP", "--host=127.0.0.1",
                  f"--port={port}", "--user=root", "--batch", "--skip-column-names", "--connect-timeout=2"]

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

        counter = itertools.count(1)

        @contextmanager
        def admin(database, expect_failure=False):
            instance = next(counter)
            http_port = free_port()
            config = root / f"config-{instance}.yaml"
            config.write_text(
                "logging:\n  console: false\n"
                f"  filePath: {json.dumps((root / f'admin-{instance}.log').as_posix())}\n"
                f"server:\n  host: 127.0.0.1\n  port: {http_port}\n  workerThreads: 2\n"
                "  staticRoot: web\n"
                f"database:\n  host: 127.0.0.1\n  port: {port}\n"
                f"  database: {database}\n  username: admin_test\n  password: disposable-test-password\n  poolSize: 2\n",
                encoding="utf-8", newline="\r\n" if instance % 2 else "\n",
            )
            with running([str(package / service.name), "--config", str(config)], package,
                         root / f"service-{instance}.log") as process:
                if expect_failure:
                    assert process.wait(timeout=30) != 0, "Invalid initialization unexpectedly succeeded"
                else:
                    def ready():
                        try:
                            with urllib.request.urlopen(f"http://127.0.0.1:{http_port}/api/v1/ready", timeout=1) as response:
                                return response.status == 200
                        except (urllib.error.URLError, TimeoutError):
                            return False
                    wait_until(ready, process)
                yield http_port

        with running([mysqld, "--no-defaults", f"--datadir={data}", f"--port={port}",
                      "--bind-address=127.0.0.1", "--mysqlx=OFF", "--skip-log-bin"],
                     root, root / "mysql.log") as database_process:
            try:
                wait_until(database_ready, database_process)
                sql("CREATE USER admin_test@localhost IDENTIFIED BY 'disposable-test-password'; GRANT ALL ON *.* TO admin_test@localhost;")
                with admin("fresh") as http_port:
                    initial = sql("SELECT id,password_hash,role,status,force_password_change FROM fresh.users;")
                    assert initial.endswith("\tsuper_admin\tactive\t1")
                    assert sql("SELECT COUNT(*) FROM fresh.users;") == "1"
                    assert sql("SELECT COUNT(*) FROM fresh.admin_bootstrap;") == "1"
                    assert sql("SELECT COUNT(*) FROM fresh.schema_migrations;") == "2"
                    for variable, value in (("time_zone", "+00:00"), ("transaction_isolation", "READ-COMMITTED"),
                                            ("innodb_lock_wait_timeout", "5")):
                        assert sql("SELECT COUNT(*) FROM performance_schema.variables_by_thread v "
                                   "JOIN performance_schema.threads t ON t.THREAD_ID=v.THREAD_ID "
                                   "WHERE t.PROCESSLIST_USER='admin_test' "
                                   f"AND v.VARIABLE_NAME='{variable}' AND v.VARIABLE_VALUE='{value}';") == "2"
                    for path in ("/migrations/001_initial_schema.sql", "/001_initial_schema.sql"):
                        try:
                            urllib.request.urlopen(f"http://127.0.0.1:{http_port}{path}", timeout=2)
                        except urllib.error.HTTPError as error:
                            assert error.code == 404
                        else:
                            raise AssertionError("SQL is exposed over HTTP")
                with admin("fresh"):
                    assert sql("SELECT id,password_hash,role,status,force_password_change FROM fresh.users;") == initial
                print("PASS: SQL-free runtime, HTTP isolation, fresh creation and repeated startup", flush=True)

                # Launch both service processes before waiting for readiness.
                with ExitStack() as services:
                    with ThreadPoolExecutor(max_workers=2) as executor:
                        contexts = [admin("concurrent"), admin("concurrent")]
                        for context in contexts:
                            services.callback(context.__exit__, None, None, None)
                        futures = [executor.submit(context.__enter__) for context in contexts]
                        for future in futures:
                            future.result()
                        assert sql("SELECT COUNT(*) FROM concurrent.users;") == "1"
                        assert sql("SELECT COUNT(*) FROM concurrent.admin_bootstrap;") == "1"
                        assert sql("SELECT COUNT(*) FROM concurrent.schema_migrations;") == "2"
                print("PASS: concurrent first startup creates one administrator and one journal per version", flush=True)

                sql("UPDATE fresh.users SET username='renamed',username_canonical='renamed',"
                    "password_hash='changed-hash',status='disabled',force_password_change=0;")
                with admin("fresh"):
                    assert sql("SELECT username,password_hash,status,force_password_change FROM fresh.users;") == "renamed\tchanged-hash\tdisabled\t0"
                sql("DELETE FROM fresh.admin_bootstrap;")
                with admin("fresh"):
                    assert sql("SELECT COUNT(*) FROM fresh.users;") == "1"
                    assert sql("SELECT user_id FROM fresh.admin_bootstrap;") == initial.split("\t")[0]
                sql("DELETE FROM fresh.users;")
                with admin("fresh"):
                    assert sql("SELECT COUNT(*) FROM fresh.users;") == "0"
                print("PASS: preserves changed credentials/status, adopts existing superadmin, never recreates after completion", flush=True)

                # Missing journal simulates interruption after MySQL auto-committed DDL.
                sql("DELETE FROM concurrent.schema_migrations;")
                with admin("concurrent"):
                    assert sql("SELECT COUNT(*) FROM concurrent.schema_migrations;") == "2"
                    assert sql("SELECT COUNT(*) FROM concurrent.users;") == "1"
                sql("UPDATE concurrent.schema_migrations SET checksum=REPEAT('0',64) WHERE version=1;")
                with admin("concurrent", expect_failure=True):
                    assert sql("SELECT checksum FROM concurrent.schema_migrations WHERE version=1;") == "0" * 64
                print("PASS: DDL replay after interruption and checksum conflict blocks startup", flush=True)

                # Fail the second write to prove account + completion marker are atomic.
                sql("DELETE FROM fresh.admin_bootstrap; CREATE TRIGGER fresh.fail_bootstrap BEFORE INSERT ON fresh.admin_bootstrap "
                    "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='injected';")
                with admin("fresh", expect_failure=True):
                    assert sql("SELECT COUNT(*) FROM fresh.users;") == "0"
                    assert sql("SELECT COUNT(*) FROM fresh.admin_bootstrap;") == "0"
                sql("DROP TRIGGER fresh.fail_bootstrap;")
                with admin("fresh"):
                    assert sql("SELECT COUNT(*) FROM fresh.users;") == "1"
                    assert sql("SELECT COUNT(*) FROM fresh.admin_bootstrap;") == "1"
                sql("DELETE FROM fresh.admin_bootstrap; UPDATE fresh.users SET role='operator';")
                with admin("fresh", expect_failure=True):
                    assert sql("SELECT role FROM fresh.users;") == "operator"
                    assert sql("SELECT COUNT(*) FROM fresh.admin_bootstrap;") == "0"
                print("PASS: bootstrap failure rolls back, retry succeeds, username conflict is not promoted", flush=True)
            except Exception:
                for log in root.glob("*.log"):
                    print(f"--- {log.name} ---\n{log.read_text(encoding='utf-8', errors='replace')[-6000:]}")
                raise
            finally:
                if database_process.poll() is None and database_ready():
                    sql("SHUTDOWN;")
                    database_process.wait(timeout=20)


if __name__ == "__main__":
    main()
