"""Two service instances against the disposable DB owned by test_admin_metrics.py.
Adds latency only inside a test-owned DB trigger, never in production code.
"""

import json
from pathlib import Path
import re
import time
from concurrent.futures import ThreadPoolExecutor
import urllib.error
import urllib.request

from test_admin_database import free_port, running, wait_until


def run_rbac_concurrency_checks(call, sql, token, service, config):
    status, body, _ = call("POST", "/admin/permissions", {"code": "report.concurrent_toggle", "name": "Concurrent toggle", "module": "reports"}, token)
    assert status == 201, body
    permission = body["data"]
    status, body, _ = call("GET", "/admin/roles", token=token)
    assert status == 200, body
    target = next(row for row in body["data"]["items"] if row["code"] == "operator")
    path = "/admin/roles/" + target["id"] + "/permissions"
    _, body, _ = call("GET", path, token=token)
    original = body["data"]["permission_ids"]
    ids = sorted(original + [permission["id"]])
    status, body, _ = call("PUT", path, {"permission_ids": ids}, token, {"If-Match": str(target["version"])})
    assert status == 200, body
    root = config.parent
    port = free_port()
    text = config.read_text()
    first_port = re.search(r"server:\n.*?\n  port: (\d+)", text, re.S).group(1)
    text = text.replace("  port: " + first_port + "\n", "  port: " + str(port) + "\n", 1)
    text = text.replace(str(root / "service.log"), str(root / "service-concurrency.log"))
    second_config = root / "concurrency-config.yaml"
    second_config.write_text(text)

    def second(method, resource, payload=None, extra=None):
        headers = {"Content-Type": "application/json", "Authorization": "Bearer " + token, **(extra or {})}
        request = urllib.request.Request(f"http://127.0.0.1:{port}/api/v1{resource}",
            data=None if payload is None else json.dumps(payload).encode(), method=method, headers=headers)
        try:
            response = urllib.request.urlopen(request, timeout=8)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            raw = response.read()
            return response.status, json.loads(raw) if raw else {}, response.headers

    with running([str(service), "--config", str(second_config)], root, root / "concurrency-process.log") as process:
        def ready():
            try:
                return second("GET", "/ready")[0] == 200
            except urllib.error.URLError:
                return False
        wait_until(ready, process)
        sql("CREATE TRIGGER metrics_test.slow_permission_update BEFORE UPDATE ON metrics_test.permissions FOR EACH ROW DO SLEEP(0.5);")
        try:
            before = int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1])
            # Keep/no-op assignment and removal both exercise the old lock inversion.
            for remove in [False, False, True]:
                _, body, _ = call("GET", "/admin/roles/" + target["id"], token=token)
                role = body["data"]
                _, body, _ = call("GET", "/admin/permissions/" + permission["id"], token=token)
                current = body["data"]
                next_status = "disabled" if current["status"] == "active" else "active"
                with ThreadPoolExecutor(max_workers=2) as executor:
                    toggle = executor.submit(call, "PATCH", "/admin/permissions/" + permission["id"] + "/status",
                        {"status": next_status}, token, {"If-Match": str(current["version"])})
                    time.sleep(0.1)
                    assign = executor.submit(second, "PUT", path, {"permission_ids": original if remove else ids}, {"If-Match": str(role["version"])})
                    results = [assign.result(), toggle.result()]
                assert results[1][0] == 200, results
                assert results[0][0] in [200, 409], results
                if results[0][0] == 409:
                    assert results[0][1]["code"] == 2209, results
            after = int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1])
            assert after == before, (before, after)
        finally:
            sql("DROP TRIGGER metrics_test.slow_permission_update;")
    print("PASS: dual-instance assignment/status/removal races: no HTTP 500, no new InnoDB deadlocks")
