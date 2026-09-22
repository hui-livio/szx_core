"""Connection-loss and operational-log checks on the disposable MySQL harness."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import time
import urllib.error
import urllib.request


def run_recovery_checks(admin, sql, root):
    previous_logs = set(root.glob("admin-*.log"))
    with admin("pool_recovery") as port:
        log_path, = set(root.glob("admin-*.log")) - previous_logs

        def request(path, payload=None):
            probe = urllib.request.Request(
                f"http://127.0.0.1:{port}/api/v1{path}",
                headers={"Content-Type": "application/json", "X-Request-ID": "recovery-check"},
                data=json.dumps(payload).encode() if payload is not None else None,
            )
            try:
                response = urllib.request.urlopen(probe, timeout=20)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                return response.status, json.load(response)

        credentials = {"identifier": "admin", "password_hash": hashlib.sha256(b"superadmin").hexdigest()}
        sql("CREATE TRIGGER pool_recovery.slow_audit BEFORE INSERT ON pool_recovery.security_audit_logs "
            "FOR EACH ROW DO SLEEP(15);")
        for _ in range(2):
            with ThreadPoolExecutor(max_workers=1) as executor:
                pending = executor.submit(request, "/auth/login", credentials)
                deadline = time.monotonic() + 10
                while True:
                    connection_id = sql(
                        "SELECT PROCESSLIST_ID FROM performance_schema.threads "
                        "WHERE PROCESSLIST_USER='admin_test' AND PROCESSLIST_DB='pool_recovery' "
                        "AND PROCESSLIST_COMMAND <> 'Sleep';"
                    )
                    if connection_id:
                        break
                    assert not pending.done(), "Request completed before disconnection"
                    assert time.monotonic() < deadline, "Active transaction was not observed"
                    time.sleep(0.05)
                assert connection_id.isdigit(), connection_id
                sql(f"KILL CONNECTION {connection_id};")
                assert pending.result()[0] == 500
        sql("DROP TRIGGER pool_recovery.slow_audit;")
        assert sql("SELECT COUNT(*) FROM pool_recovery.refresh_tokens;") == "0"
        assert sql("SELECT COUNT(*) FROM pool_recovery.security_audit_logs;") == "0"

        # Failed reconnection must also retain capacity, and must not log each retry.
        sql("ALTER USER admin_test@localhost ACCOUNT LOCK;")
        try:
            for _ in range(3):
                assert request("/ready")[0] == 503
            log = log_path.read_text(encoding="utf-8")
            assert log.count("event=connection_unavailable") == 1
            assert "event=connection_recovered" not in log
            assert log.count("event=write_failed") == 1
        finally:
            sql("ALTER USER admin_test@localhost ACCOUNT UNLOCK;")

        for _ in range(3):
            assert request("/ready")[0] == 200
        sessions = sql("SELECT COUNT(*) FROM performance_schema.threads "
                       "WHERE PROCESSLIST_USER='admin_test' AND PROCESSLIST_DB='pool_recovery';")
        assert sessions == "2", sessions
        for variable, value in (("time_zone", "+00:00"), ("transaction_isolation", "READ-COMMITTED"),
                                ("innodb_lock_wait_timeout", "5")):
            configured = sql("SELECT COUNT(*) FROM performance_schema.variables_by_thread v "
                             "JOIN performance_schema.threads t ON t.THREAD_ID=v.THREAD_ID "
                             "WHERE t.PROCESSLIST_USER='admin_test' AND t.PROCESSLIST_DB='pool_recovery' "
                             f"AND v.VARIABLE_NAME='{variable}' AND v.VARIABLE_VALUE='{value}';")
            assert configured == "2", (variable, configured)
        status, body = request("/auth/login", credentials)
        assert status == 200
        log = log_path.read_text(encoding="utf-8")
        for event in ("authentication_initialized", "database_initialized", "connection_unavailable",
                      "connection_recovered", "write_failed", "write_recovered"):
            assert log.count("event=" + event) == 1, (event, log)
        assert 'status=503 code=2220' in log
        assert credentials["password_hash"] not in log
        assert body["data"]["access_token"] not in log
        assert "disposable-test-password" not in log
        print("PASS: lost connections and failed reconnects retain pool capacity; recovery restores sessions; logs deduplicate", flush=True)
