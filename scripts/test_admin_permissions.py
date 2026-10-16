"""Custom permission lifecycle checks, called with --rbac by test_admin_metrics.py.
Uses that script's isolated database and never connects to the configured production DB.
"""


def run_permission_checks(call, sql, login, token):
    from concurrent.futures import ThreadPoolExecutor

    def get(path, credential=token):
        status, body, _ = call("GET", path, token=credential)
        assert status == 200, (status, body)
        return body["data"]

    def write(method, path, data=None, version=None, credential=token, want=200):
        headers = {"If-Match": str(version)} if version is not None else None
        status, body, _ = call(method, path, data, credential, headers)
        assert status == want, (method, path, status, body)
        return body.get("data")

    for role in ["operator", "engineer", "admin"]:
        credential = login("metrics_" + role, "MetricsTest123")
        status, body, _ = call("GET", "/admin/permissions", token=credential)
        assert status == 403 and body["code"] == 2204, body
    catalog = get("/admin/permissions")["items"]
    system = next(item for item in catalog if item["code"] == "user.read")
    write("PATCH", "/admin/permissions/" + system["id"], {"name": "changed"}, system["version"], want=409)
    write("POST", "/admin/permissions", {"code": "system.custom", "name": "Bad", "module": "test"}, want=422)
    write("POST", "/admin/permissions", {"code": "report.export", "name": "Bad", "module": "test", "enforcement": "server"}, want=422)
    created = write("POST", "/admin/permissions", {"code": "report.export", "name": "Export reports", "description": "First description", "module": "reports"}, want=201)
    assert created["version"] == 1 and created["description"] == "First description", created
    assert created["enforcement"] == "client" and created["source"] == "custom", created
    write("POST", "/admin/permissions", {"code": "report.export", "name": "Duplicate", "module": "reports"}, want=409)
    path = "/admin/permissions/" + created["id"]
    write("PATCH", path, {"name": "Bad", "description": 7}, 1, want=422)
    write("PATCH", path, {"name": "Bad", "module": "changed"}, 1, want=422)
    write("PATCH", path + "/status", {"status": "archived"}, 1, want=422)
    edited = write("PATCH", path, {"name": "Export reports v2", "description": "Edited description"}, 1)
    assert edited["version"] == 2 and edited["description"] == "Edited description", edited
    write("PATCH", path, {"name": "Stale"}, 1, want=409)
    roles = get("/admin/roles")["items"]
    target = next(item for item in roles if item["code"] == "operator")
    rp = "/admin/roles/" + target["id"] + "/permissions"
    original = get(rp)["permission_ids"]
    with_permission = sorted(original + [created["id"]])
    before = login("metrics_operator", "MetricsTest123")
    target = write("PUT", rp, {"permission_ids": with_permission}, target["version"])
    status, body, _ = call("GET", "/client/me/permissions", token=before)
    assert status == 401, body
    after = login("metrics_operator", "MetricsTest123")
    assert "report.export" in get("/client/me/permissions", after)["capabilities"]
    write("DELETE", path, version=edited["version"], want=409)  # must unbind first
    disabled = write("PATCH", path + "/status", {"status": "disabled"}, edited["version"])
    status, body, _ = call("GET", "/client/me/permissions", token=after)
    assert status == 401, body
    current = login("metrics_operator", "MetricsTest123")
    assert "report.export" not in get("/client/me/permissions", current)["capabilities"]
    engineer_role = next(item for item in roles if item["code"] == "engineer")
    engineer_path = "/admin/roles/" + engineer_role["id"] + "/permissions"
    engineer_ids = get(engineer_path)["permission_ids"]
    write("PUT", engineer_path, {"permission_ids": sorted(engineer_ids + [created["id"].upper()])}, engineer_role["version"], want=404)
    # Editing other assignments must preserve a pre-existing disabled custom binding.
    target = get("/admin/roles/" + target["id"])
    write("PUT", rp, {"permission_ids": [value.upper() for value in with_permission]}, target["version"])
    target = get("/admin/roles/" + target["id"])
    target = write("PUT", rp, {"permission_ids": original}, target["version"])
    assert get(path)["status"] == "disabled"  # no transient regrant needed to archive
    write("DELETE", path, version=disabled["version"], want=204)
    assert get(path)["status"] == "archived"
    assert not any(item["id"] == created["id"] for item in get("/admin/permissions")["items"])
    assert any(item["id"] == created["id"] for item in get("/admin/permissions?include_archived=true")["items"])
    write("PATCH", path + "/status", {"status": "active"}, disabled["version"] + 1, want=409)
    write("POST", "/admin/permissions", {"code": "report.export", "name": "Reused", "module": "reports"}, want=409)
    super_role = next(item for item in roles if item["code"] == "super_admin")
    write("PUT", "/admin/roles/" + super_role["id"] + "/permissions", {"permission_ids": []}, super_role["version"], want=409)
    with ThreadPoolExecutor(max_workers=2) as executor:
        responses = list(executor.map(lambda _: call("POST", "/admin/permissions", {"code": "report.concurrent", "name": "Concurrent", "module": "reports"}, token), range(2)))
    assert sorted(item[0] for item in responses) == [201, 409], responses
    # Business write must roll back when its required security audit cannot be persisted.
    sql("CREATE TRIGGER metrics_test.reject_permission_audit BEFORE INSERT ON metrics_test.security_audit_logs FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test-only audit rejection';")
    write("POST", "/admin/permissions", {"code": "report.rollback", "name": "Rollback", "module": "reports"}, want=500)
    sql("DROP TRIGGER metrics_test.reject_permission_audit;")
    assert sql("SELECT COUNT(*) FROM metrics_test.permissions WHERE code='report.rollback';") == "0"
    assert int(sql("SELECT COUNT(*) FROM metrics_test.security_audit_logs WHERE event_name='rbac.permission.changed';")) >= 5
    from urllib.parse import urlencode
    for code, chinese in [("rbac.permission.changed", "权限变更"), ("rbac.role.changed", "角色授权变更")]:
        by_code = get("/admin/audit-logs?" + urlencode({"event_search": code}))
        by_label = get("/admin/audit-logs?" + urlencode({"event_search": chinese}))
        assert by_code["total"] > 0 and by_label["total"] == by_code["total"], (code, by_code, by_label)
    print("PASS: Chinese permission/role audit search and disabled unbind/archive without regrant")
    print("PASS: custom permission CRUD, description/version, protected system permissions, binding guard, role assignment and token invalidation")
