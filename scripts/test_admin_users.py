"""User API regressions on the disposable MySQL instance owned by test_admin_database.py."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import time
import urllib.error
import urllib.request
import uuid


def run_user_checks(admin, sql):
    def call(port, method, path, payload=None, token=None, version=None, headers=None):
        request_headers = {"Content-Type": "application/json", "X-Request-ID": "user-check"}
        if token:
            request_headers["Authorization"] = "Bearer " + token
        if version is not None:
            request_headers["If-Match"] = str(version)
        request_headers.update(headers or {})
        request = urllib.request.Request(
            f"http://127.0.0.1:{port}/api/v1{path}", method=method, headers=request_headers,
            data=json.dumps(payload).encode() if payload is not None else None,
        )
        try:
            response = urllib.request.urlopen(request, timeout=20)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            raw = response.read()
            body = json.loads(raw) if raw else None
            return response.status, body, response.headers

    def expect(response, status, code=None):
        assert response[0] == status, response[:2]
        if code is not None:
            assert response[1]["code"] == code, response[:2]
        return response[1]["data"] if status in (200, 201, 202) else None

    def login(port, name="admin", password="superadmin"):
        return expect(call(port, "POST", "/auth/login", {
            "identifier": name, "password_hash": hashlib.sha256(password.encode()).hexdigest(),
        }), 200)

    def profile_flags(user, role):
        assert user["force_password_change"] is (role == "super_admin"), user
        assert user["password_change_recommended"] is (role != "super_admin"), user
        assert "password_hash" not in user
        assert user["username"] and "uid" not in user
        assert all(type(user[field]) is int for field in ("created_at", "updated_at"))
        assert user["last_login_at"] is None or type(user["last_login_at"]) is int

    with admin("user_api") as port:
        session = login(port)
        token = session["access_token"]
        actor_id = session["user"]["id"]
        permissions = expect(call(port, "GET", "/client/me/permissions", token=token), 200)
        assert permissions["user_id"] == actor_id and "user.create" in permissions["capabilities"]

        # Actual documented routes, HTTP methods and browser preflights must all work.
        for path in ("/admin/users", "/admin/users/" + actor_id, "/admin/users/" + actor_id + "/status",
                     "/admin/users/" + actor_id + "/unlock", "/admin/users/" + actor_id + "/role",
                     "/admin/users/" + actor_id + "/reset-password", "/admin/user-logs",
                     "/client/me/permissions", "/client/behavior-events"):
            response = call(port, "OPTIONS", path, headers={"Origin": "https://admin.example.test"})
            expect(response, 204)
            assert "PATCH" in response[2]["Access-Control-Allow-Methods"]
            assert "If-Match" in response[2]["Access-Control-Allow-Headers"]
        expect(call(port, "OPTIONS", "/admin/users", headers={
            "Origin": "https://admin.example.test", "Access-Control-Request-Method": "DELETE",
        }), 405, 2219)
        expect(call(port, "OPTIONS", "/admin/users", headers={"Origin": "https://evil.example"}), 403, 2204)
        expect(call(port, "GET", "/admin/users", token=token,
                    headers={"Origin": "https://evil.example"}), 403, 2204)

        users = {}
        for identity in ({}, {"uid": "U1", "username": "both"}, {"uid": None, "username": None}):
            expect(call(port, "POST", "/admin/users", {
                "display_name": "Invalid identity", "role": "operator", **identity,
            }, token), 422, 2207)
        expect(call(port, "POST", "/admin/users", {
            "uid": 123, "display_name": "Invalid type", "role": "operator",
        }, token), 422, 2206)
        for role in ("operator", "engineer", "admin", "super_admin"):
            data = expect(call(port, "POST", "/admin/users", {
                "username": "test_" + role, "display_name": role, "role": role,
            }, token), 201)
            profile_flags(data["user"], role)
            assert data["user"]["last_login_at"] is None
            assert type(data["temporary_credential_expires_at"]) is int
            assert abs(data["temporary_credential_expires_at"] - (time.time() + 86400) * 1000) < 10000
            users[role] = data
            read = expect(call(port, "GET", "/admin/users/" + data["user"]["id"], token=token), 200)
            profile_flags(read, role)

        for sort in ("-created_at", "created_at", "display_name", "-display_name", "last_login_at",
                     "-last_login_at", "role", "status"):
            page = expect(call(port, "GET", "/admin/users?sort=" + sort, token=token), 200)
            assert page["total"] == 5
            assert all(item["username"] and "uid" not in item for item in page["items"])
            if sort == "display_name":
                names = [item["display_name"] for item in page["items"]]
                assert names == sorted(names, key=str.casefold)
        expect(call(port, "GET", "/admin/users?sort=invalid", token=token), 422, 2206)
        expect(call(port, "GET", "/admin/users?sortt=role", token=token), 422, 2206)
        logs = expect(call(port, "GET", "/admin/user-logs?actor_user_id=" + actor_id, token=token), 200)
        assert logs["total"] >= 5 and all(row["actor_user_id"] == actor_id for row in logs["items"])
        empty = expect(call(port, "GET", "/admin/user-logs?actor_user_id=" + str(uuid.uuid4()), token=token), 200)
        assert empty["total"] == 0 and empty["items"] == []

        operator = users["operator"]
        target_id = operator["user"]["id"]
        target_path = "/admin/users/" + target_id
        upper_path = "/admin/users/" + target_id.upper()
        expect(call(port, "GET", upper_path, token=token), 200)
        operator_session = login(port, "test_operator", operator["temporary_credential"])
        low_token = operator_session["access_token"]
        expect(call(port, "POST", "/admin/users", {
            "username": "denied", "display_name": "Denied", "role": "operator",
        }, low_token), 403, 2204)
        expect(call(port, "POST", target_path + "/reset-password", token=low_token, version=1), 403, 2204)
        for invalid in (None, "invalid.token.value"):
            expect(call(port, "POST", "/admin/users", {
                "username": "unauthenticated", "display_name": "Denied", "role": "operator",
            }, invalid), 401, 2200)
            expect(call(port, "POST", target_path + "/reset-password", token=invalid, version=1), 401, 2200)
        assert sql("SELECT COUNT(*) FROM user_api.users;") == "5"

        edited = expect(call(port, "PATCH", upper_path, {"display_name": "Renamed"}, token, 1), 200)
        assert edited["version"] == 2 and edited["authz_version"] == 1
        expect(call(port, "PATCH", target_path, {"display_name": "Stale"}, token, 1), 409, 2209)
        disabled = expect(call(port, "PATCH", upper_path + "/status", {"status": "disabled"}, token, 2), 200)
        assert disabled["status"] == "disabled" and disabled["authz_version"] == 2
        expect(call(port, "GET", "/client/me/permissions", token=low_token), 401, 2200)
        assert sql(f"SELECT COUNT(*) FROM user_api.refresh_tokens WHERE user_id='{target_id}' AND revoked_at IS NULL;") == "0"
        active = expect(call(port, "PATCH", target_path + "/status", {"status": "active"}, token, 3), 200)
        assert active["version"] == 4
        sql(f"UPDATE user_api.users SET status='locked' WHERE id='{target_id}';")
        unlocked = expect(call(port, "PATCH", upper_path + "/unlock", token=token, version=4), 200)
        assert unlocked["status"] == "active" and unlocked["version"] == 5
        assigned = expect(call(port, "PUT", upper_path + "/role", {"role": "engineer"}, token, 5), 200)
        assert assigned["role"] == "engineer" and assigned["version"] == 6
        reset = expect(call(port, "POST", upper_path + "/reset-password", token=token, version=6), 200)
        profile_flags(reset["user"], "engineer")
        assert type(reset["temporary_credential_expires_at"]) is int
        assert abs(reset["temporary_credential_expires_at"] - (time.time() + 86400) * 1000) < 10000
        assert reset["temporary_credential"] != operator["temporary_credential"]
        renewed = login(port, "test_operator", reset["temporary_credential"])
        profile_flags(renewed["user"], "engineer")

        # Audit failure must roll back the credential, versions and refresh revocation together.
        before = sql(f"SELECT password_hash,version,authz_version FROM user_api.users WHERE id='{target_id}';")
        refresh_before = sql(f"SELECT COUNT(*) FROM user_api.refresh_tokens WHERE user_id='{target_id}' AND revoked_at IS NULL;")
        sql("CREATE TRIGGER user_api.fail_user_audit BEFORE INSERT ON user_api.user_logs FOR EACH ROW "
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='USER_AUDIT_SECRET';")
        expect(call(port, "POST", target_path + "/reset-password", token=token, version=7), 500, 2212)
        assert sql(f"SELECT password_hash,version,authz_version FROM user_api.users WHERE id='{target_id}';") == before
        assert sql(f"SELECT COUNT(*) FROM user_api.refresh_tokens WHERE user_id='{target_id}' AND revoked_at IS NULL;") == refresh_before
        sql("DROP TRIGGER user_api.fail_user_audit;")

        event = expect(call(port, "POST", "/client/behavior-events", {
            "event_name": "report.view", "result": "success", "details": {"source": "dashboard"},
        }, renewed["access_token"]), 202)
        assert event["event_id"]
        page = expect(call(port, "GET", "/admin/user-logs?event_name=report.view", token=token), 200)
        assert page["items"][0]["details"] == {"source": "dashboard"}
        assert type(page["items"][0]["created_at"]) is int
        event_time = sql("SET time_zone='+00:00'; SELECT CAST(FLOOR(UNIX_TIMESTAMP(created_at)*1000) AS SIGNED) "
                         "FROM user_api.user_logs WHERE event_name='report.view';")
        assert page["items"][0]["created_at"] == int(event_time)
        for index, details in enumerate(({}, {"source": "dashboard", "padding": "x" * 2000},
                                         {"unicode": "中文😀", "items": [{"count": 3}]})):
            event_name = f"report.roundtrip{index}"
            expect(call(port, "POST", "/client/behavior-events", {
                "event_name": event_name, "result": "success", "details": details,
            }, renewed["access_token"]), 202)
            page = expect(call(port, "GET", "/admin/user-logs?event_name=" + event_name, token=token), 200)
            assert page["items"][0]["details"] == details, page

        sensitive = {"source": "dashboard", "Password": "SYNTHETIC_SECRET", "private-key": "SYNTHETIC_SECRET",
                     "items": [{"access_token": "SYNTHETIC_SECRET", "count": 2}],
                     "context": {"machine_id": "SYNTHETIC_SECRET", "stack_trace": "SYNTHETIC_SECRET", "count": 1}}
        expected = {"source": "dashboard", "items": [{"count": 2}], "context": {"count": 1}}
        expect(call(port, "POST", "/client/behavior-events", {
            "event_name": "report.redaction", "result": "success", "details": sensitive,
        }, renewed["access_token"]), 202)
        stored = sql("SELECT details_json FROM user_api.user_logs WHERE event_name='report.redaction';")
        assert json.loads(stored) == expected and "SYNTHETIC_SECRET" not in stored
        # Older rows must be redacted on read too; exercise multiple rows in one blob query.
        legacy_sizes = [2000, 70000, 150000]
        for index, size in enumerate(legacy_sizes):
            sql("INSERT INTO user_api.user_logs(id,event_name,result,request_id,details_json) VALUES "
                f"(UUID(),'report.legacy','success','legacy-{index}',"
                f"JSON_OBJECT('password','SYNTHETIC_SECRET','padding',REPEAT('x',{size})));")
        page = expect(call(port, "GET", "/admin/user-logs?event_name=report.legacy", token=token), 200)
        assert len(page["items"]) == 3
        assert all(row["details"] == {"padding": "x" * legacy_sizes[int(row["request_id"].split("-")[1])]}
                   for row in page["items"])
        expect(call(port, "POST", "/client/behavior-events", {
            "event_name": "auth.login.success", "result": "success", "details": {},
        }, renewed["access_token"]), 422, 2206)
        print("PASS: user API contracts, role-derived flags, filters, lifecycle, authorization and audit rollback", flush=True)

        # A locking read must wait for concurrent revocation, then reject the stale JWT.
        # The named lock is only a barrier in this disposable database; no timing-only ordering.
        with ThreadPoolExecutor(max_workers=1) as executor:
            revoked = executor.submit(sql, "START TRANSACTION; "
                f"UPDATE user_api.users SET status='disabled',authz_version=authz_version+1 WHERE id='{actor_id}'; "
                "SELECT GET_LOCK('szx_user_revocation_barrier',0); DO SLEEP(2); COMMIT; "
                "DO RELEASE_LOCK('szx_user_revocation_barrier');")
            deadline = time.monotonic() + 5
            while sql("SELECT IS_USED_LOCK('szx_user_revocation_barrier') IS NOT NULL;") != "1":
                assert not revoked.done(), "Revocation finished before the barrier was observed"
                assert time.monotonic() < deadline, "Revocation barrier was not reached"
                time.sleep(0.02)
            expect(call(port, "POST", "/admin/users", {
                "username": "revoked_creation", "display_name": "Denied", "role": "super_admin",
            }, token), 401, 2200)
            revoked.result()
        assert sql("SELECT COUNT(*) FROM user_api.users WHERE username='revoked_creation';") == "0"
        print("PASS: concurrent actor revocation prevents creation with stale authorization", flush=True)

    with admin("user_rate", attempts_per_minute=2) as port:
        session = login(port)  # Shares the per-peer credential budget with creation/reset.
        token = session["access_token"]
        target = expect(call(port, "POST", "/admin/users", {
            "username": "limited", "display_name": "Limited", "role": "operator",
        }, token), 201)["user"]
        before = sql(f"SELECT password_hash,version FROM user_rate.users WHERE id='{target['id']}';")
        expect(call(port, "POST", "/admin/users", {
            "username": "over_budget", "display_name": "Denied", "role": "operator",
        }, token), 429, 2211)
        expect(call(port, "POST", "/admin/users/" + target["id"] + "/reset-password", token=token, version=1), 429, 2211)
        assert sql(f"SELECT password_hash,version FROM user_rate.users WHERE id='{target['id']}';") == before
        assert sql("SELECT COUNT(*) FROM user_rate.users;") == "2"
        print("PASS: credential issuance is rate-limited without partial writes", flush=True)

    with admin("user_identity") as port:
        token = login(port)["access_token"]
        for identity in ({"username": "resume"}, {"username": "résumé"}, {"uid": "U10086"}, {"uid": "u10086"}):
            issued = expect(call(port, "POST", "/admin/users", {
                **identity, "display_name": "Distinct canonical identity", "role": "operator",
            }, token), 201)
            logged_in = login(port, next(iter(identity.values())), issued["temporary_credential"])["user"]
            read = expect(call(port, "GET", "/admin/users/" + issued["user"]["id"], token=token), 200)
            for profile in (issued["user"], logged_in, read):
                assert profile["id"] == issued["user"]["id"]
                assert {key: profile[key] for key in ("uid", "username") if key in profile} == identity
        for identity in ({"username": "RESUME"}, {"username": "re\u0301sume\u0301"}, {"uid": "U10086"}):
            expect(call(port, "POST", "/admin/users", {
                **identity, "display_name": "Duplicate", "role": "operator",
            }, token), 409, 2208)
        with ThreadPoolExecutor(max_workers=2) as executor:
            responses = list(executor.map(lambda _: call(port, "POST", "/admin/users", {
                "username": "concurrent_duplicate", "display_name": "Duplicate", "role": "operator",
            }, token), range(2)))
        assert sorted(response[0] for response in responses) == [201, 409], responses
        print("PASS: binary canonical indexes preserve accents/UID case and return stable duplicate errors", flush=True)

    with admin("behavior_rate", behavior_events_per_minute=2) as port:
        token = login(port)["access_token"]
        payload = {"event_name": "report.view", "result": "success", "details": {}}
        for _ in range(2):
            expect(call(port, "POST", "/client/behavior-events", payload, token), 202)
        expect(call(port, "POST", "/client/behavior-events", payload, token), 429, 2211)
        assert sql("SELECT COUNT(*) FROM behavior_rate.user_logs WHERE event_name='report.view';") == "2"
        # Exhausting telemetry cannot lock the caller out of authentication or credential management.
        login(port)
        issued = expect(call(port, "POST", "/admin/users", {
            "username": "independent", "display_name": "Independent", "role": "operator",
        }, token), 201)
        other = login(port, "independent", issued["temporary_credential"])["access_token"]
        expect(call(port, "POST", "/client/behavior-events", payload, other), 202)
        print("PASS: telemetry limits are per-user and independent of authentication", flush=True)
