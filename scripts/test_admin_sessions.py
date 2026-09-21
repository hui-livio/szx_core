"""Refresh/logout scenarios for the disposable MySQL admin integration suite."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
from http.cookies import SimpleCookie
import urllib.request


def run_session_checks(admin, sql, root, login, post, verify_token):
    secrets = []

    def session(response):
        assert response[0] == 200, response[:2]
        data = response[1]["data"]
        cookie = SimpleCookie(response[2]["Set-Cookie"])["szx_refresh"]
        token = cookie.value
        assert 0 < int(cookie["max-age"]) <= 1800
        assert "refresh_token" not in data and "password_hash" not in data["user"]
        assert data["csrf_token"] == hashlib.sha256(("csrf:" + token).encode()).hexdigest()
        verify_token(data["access_token"])
        secrets.extend((token, data["csrf_token"], data["access_token"]))
        return token, data

    def call(port, current=None, endpoint="refresh", headers=None, payload=b""):
        request_headers = {"Origin": "https://business.example.test"}
        if current:
            request_headers.update({"Cookie": "szx_refresh=" + current[0], "X-CSRF-Token": current[1]["csrf_token"]})
        request_headers.update(headers or {})
        return post(port, payload, endpoint=endpoint, extra_headers=request_headers, request_id="session-check")

    def token_hash(current):
        return hashlib.sha256(current[0].encode()).hexdigest()

    def active(current):
        return sql("SELECT COUNT(*) FROM sessions.refresh_tokens WHERE revoked_at IS NULL AND family_id="
                   f"(SELECT family_id FROM sessions.refresh_tokens WHERE token_hash='{token_hash(current)}');")

    def cleared(response):
        assert response[0] == 204, response[:2]
        cookie = SimpleCookie(response[2]["Set-Cookie"])["szx_refresh"]
        assert cookie.value == "" and cookie["max-age"] == "0" and cookie["path"] == "/api/v1/auth"
        assert cookie["httponly"] and cookie["secure"] and cookie["samesite"] == "None"

    with admin("sessions", allowed_origin="*") as port:
        first = session(login(port))
        assert first[1]["user"]["force_password_change"] is True
        assert call(port)[0] == 401
        cleared(call(port, endpoint="logout"))
        assert call(port, headers={"Authorization": "Bearer " + first[1]["access_token"]})[0] == 401
        for endpoint in ("refresh", "logout"):
            for headers in ({"X-CSRF-Token": ""}, {"X-CSRF-Token": "0" * 64},
                            {"X-CSRF-Token": first[1]["csrf_token"].upper()}):
                assert call(port, first, endpoint, headers)[0] == 403
            for payload in ({"refresh_token": first[0]}, {"user_id": first[1]["user"]["id"]}, []):
                assert call(port, first, endpoint, payload=payload)[0] == 422
            assert call(port, first, endpoint, {"Cookie": f"szx_refresh={first[0]}; szx_refresh={first[0]}"})[0] == 422
            for invalid in ("", "x", "A" * 42, "A" * 44, "a" * 43, first[0] + "="):
                assert call(port, first, endpoint, {"Cookie": "szx_refresh=" + invalid})[0] == 401
            preflight = urllib.request.Request(f"http://127.0.0.1:{port}/api/v1/auth/{endpoint}", method="OPTIONS",
                headers={"Origin": "https://business.example.test", "Access-Control-Request-Method": "POST",
                         "Access-Control-Request-Headers": "x-csrf-token"})
            with urllib.request.urlopen(preflight, timeout=5) as response:
                assert response.status == 204 and response.read() == b""
                assert "X-CSRF-Token" in response.headers["Access-Control-Allow-Headers"]
                assert response.headers["Access-Control-Allow-Credentials"] == "true"
        assert active(first) == "1"
        unknown_token = "A" * 43
        unknown = (unknown_token, {"csrf_token": hashlib.sha256(("csrf:" + unknown_token).encode()).hexdigest()})
        assert call(port, unknown)[0] == 401
        cleared(call(port, unknown, "logout"))

        # Every rotation write must roll back, including when the new token has already been inserted.
        for table, operation, code in (("user_logs", "INSERT", 2212), ("refresh_tokens", "INSERT", 2221),
                                        ("refresh_tokens", "UPDATE", 2221)):
            before = sql("SELECT COUNT(*) FROM sessions.refresh_tokens;")
            sql(f"CREATE TRIGGER sessions.fail_session BEFORE {operation} ON sessions.{table} "
                "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='SESSION_SECRET';")
            response = call(port, first)
            assert response[0] == 500 and response[1]["code"] == code, response[:2]
            assert sql("SELECT COUNT(*) FROM sessions.refresh_tokens;") == before
            assert active(first) == "1"
            assert sql(f"SELECT replaced_by IS NULL FROM sessions.refresh_tokens WHERE token_hash='{token_hash(first)}';") == "1"
            sql("DROP TRIGGER sessions.fail_session;")

        # No access token is required to refresh; an expired or invalid Bearer header is irrelevant.
        response = call(port, first, headers={"Cookie": "other=value; szx_refresh = " + first[0] + " ; last=1",
                                             "Authorization": "Bearer expired"}, payload={})
        second = session(response)
        assert response[2]["Access-Control-Allow-Origin"] == "https://business.example.test"
        assert second[0] != first[0] and second[1]["access_token"] != first[1]["access_token"]
        assert second[1]["user"]["force_password_change"] is True
        assert second[1]["user"]["username"] == "admin" and "uid" not in second[1]["user"]
        assert type(second[1]["expires_at"]) is int
        assert second[1]["expires_at"] == verify_token(second[1]["access_token"])["exp"] * 1000
        assert all(type(second[1]["user"][field]) is int for field in ("created_at", "updated_at", "last_login_at"))
        assert active(first) == "1"
        assert sql("SELECT COUNT(DISTINCT expires_at),COUNT(DISTINCT family_id) FROM sessions.refresh_tokens;") == "1\t1"
        assert sql("SELECT a.last_used_at IS NOT NULL,a.revocation_reason,a.replaced_by=b.id "
                   "FROM sessions.refresh_tokens a JOIN sessions.refresh_tokens b ON a.replaced_by=b.id;") == "1\trotated\t1"
        assert call(port, second, headers={"X-CSRF-Token": first[1]["csrf_token"]})[0] == 403
        assert active(second) == "1"
        independent = session(login(port))

        # Replay revocation itself must roll back when its mandatory audit fails.
        sql("CREATE TRIGGER sessions.fail_session BEFORE INSERT ON sessions.user_logs FOR EACH ROW "
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='SESSION_SECRET';")
        response = call(port, first)
        assert response[0] == 500 and response[1]["code"] == 2212
        assert active(second) == "1"
        sql("DROP TRIGGER sessions.fail_session;")
        response = call(port, first)
        assert response[0] == 409 and response[1]["code"] == 2202
        assert active(second) == "0" and call(port, second)[0] == 401
        assert sql("SELECT COUNT(*) FROM sessions.user_logs WHERE event_name='auth.refresh.replayed';") == "1"
        independent = session(call(port, independent))  # A different login family survives replay.

        # Logout is atomic, idempotent and can revoke descendants using a rotated ancestor cookie.
        parent = session(login(port))
        child = session(call(port, parent))
        for table, operation, code in (("user_logs", "INSERT", 2212), ("refresh_tokens", "UPDATE", 2221)):
            sql(f"CREATE TRIGGER sessions.fail_session BEFORE {operation} ON sessions.{table} "
                "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='SESSION_SECRET';")
            response = call(port, parent, "logout")
            assert response[0] == 500 and response[1]["code"] == code
            assert active(child) == "1"
            sql("DROP TRIGGER sessions.fail_session;")
        cleared(call(port, parent, "logout"))
        cleared(call(port, parent, "logout"))
        cleared(call(port, child, "logout"))
        assert call(port, child)[0] == 401 and active(child) == "0"
        independent = session(call(port, independent))

        expired = session(login(port))
        sql(f"UPDATE sessions.refresh_tokens SET expires_at='2000-01-01' WHERE token_hash='{token_hash(expired)}';")
        response = call(port, expired)
        assert response[0] == 401 and response[1]["code"] == 2201
        cleared(call(port, expired, "logout"))

        idle = session(login(port))
        before = sql("SELECT COUNT(*) FROM sessions.refresh_tokens;")
        sql(f"UPDATE sessions.refresh_tokens SET created_at=UTC_TIMESTAMP()-INTERVAL 1801 SECOND "
            f"WHERE token_hash='{token_hash(idle)}';")
        response = call(port, idle)
        assert response[0] == 401 and response[1]["code"] == 2201
        assert sql("SELECT COUNT(*) FROM sessions.refresh_tokens;") == before
        assert sql(f"SELECT replaced_by IS NULL FROM sessions.refresh_tokens WHERE token_hash='{token_hash(idle)}';") == "1"
        cleared(call(port, idle, "logout"))
        for state in ("disabled", "locked"):
            current = session(login(port))
            sql(f"UPDATE sessions.users SET status='{state}';")
            assert call(port, current)[0] == 401
            cleared(call(port, current, "logout"))
            sql("UPDATE sessions.users SET status='active';")

        concurrent = session(login(port))
        with ThreadPoolExecutor(max_workers=2) as executor:
            results = list(executor.map(lambda _: call(port, concurrent), range(2)))
        assert sorted(result[0] for result in results) == [200, 409], [result[:2] for result in results]
        winner = session(next(result for result in results if result[0] == 200))
        assert active(winner) == "0" and call(port, winner)[0] == 401
        racing = session(login(port))
        with ThreadPoolExecutor(max_workers=2) as executor:
            refresh = executor.submit(call, port, racing)
            logout = executor.submit(call, port, racing, "logout")
            assert refresh.result()[0] in (200, 401)
            cleared(logout.result())
        assert active(racing) == "0"

        # Changing the password revokes refresh from every login family.
        response = post(port, {"old_password_hash": hashlib.sha256(b"superadmin").hexdigest(),
                               "new_password_hash": hashlib.sha256(b"session-new-password").hexdigest()},
            endpoint="change-password", extra_headers={"Authorization": "Bearer " + independent[1]["access_token"],
                "If-Match": "1", "Origin": "https://business.example.test"})
        assert response[0] == 200, response[:2]
        assert call(port, independent)[0] == 401
        surviving = session(login(port, password="session-new-password"))
        assert surviving[1]["user"]["force_password_change"] is False

    with admin("sessions", allowed_origin="*") as port:
        surviving = session(call(port, surviving))
        assert surviving[1]["user"]["force_password_change"] is False
        cleared(call(port, surviving, "logout"))

    with admin("session_origin") as port:
        current = session(login(port))
        for endpoint in ("refresh", "logout"):
            assert call(port, current, endpoint)[0] == 403
        rotated = session(call(port, current, headers={"Origin": "https://admin.example.test"}))
        response = call(port, rotated, "logout", {"Origin": "https://admin.example.test"})
        assert response[0] == 204
        assert SimpleCookie(response[2]["Set-Cookie"])["szx_refresh"]["samesite"] == "None"

    with admin("session_rate", allowed_origin="*", attempts_per_minute=2) as port:
        current = session(login(port))
        current = session(call(port, current))
        for endpoint in ("refresh", "logout"):
            response = call(port, current, endpoint)
            assert response[0] == 429 and response[1]["code"] == 2211

    secrets.append("SESSION_SECRET")
    assert not any(secret in log.read_text(encoding="utf-8") for log in root.glob("admin-*.log") for secret in secrets)
    print("PASS: refresh rotation, CSRF, family replay revocation, logout idempotency/isolation, absolute/idle expiry, "
          "state checks, concurrency, rollback, restart, CORS and rate limiting", flush=True)
