"""Login integration scenarios, invoked by test_admin_database.py in its disposable MySQL instance.

Requires the Python cryptography package for independent PS256 verification.
"""

import base64
from concurrent.futures import ThreadPoolExecutor
import hashlib
from http.cookies import SimpleCookie
import json
import time
import urllib.error
import urllib.request

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa

from test_admin_sessions import run_session_checks


def run_login_checks(admin, sql, public_key, root):
    with admin("bad_login_key", expect_failure=True, private_key_path=root / "missing-key.pem"):
        pass
    mismatched_key = root / "mismatched-key.pem"
    mismatched_key.write_bytes(rsa.generate_private_key(public_exponent=65537, key_size=3072).private_bytes(
        serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    with admin("bad_login_key", expect_failure=True, private_key_path=mismatched_key):
        pass

    def post(port, payload, request_id="login-check", extra_headers=None, endpoint="login"):
        headers = {"Content-Type": "application/json", "X-Request-ID": request_id}
        headers.update(extra_headers or {})
        raw = payload if isinstance(payload, bytes) else json.dumps(payload, ensure_ascii=False).encode("utf-8")
        request = urllib.request.Request(f"http://127.0.0.1:{port}/api/v1/auth/{endpoint}", data=raw, headers=headers)
        try:
            response = urllib.request.urlopen(request, timeout=20)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            if response.status == 204:
                assert response.read() == b""
                body = None
            else:
                body = json.load(response)
                assert body["request_id"] == request_id
            assert response.headers["Cache-Control"] == "no-store"
            assert response.headers["X-Request-ID"] == request_id
            if response.status not in (200, 204):
                assert "Set-Cookie" not in response.headers
                assert body["details"] == {}
            return response.status, body, response.headers

    def login(port, identifier="admin", password="superadmin", **kwargs):
        password_hash = hashlib.sha256(password.encode("utf-8")).hexdigest()
        return post(port, {"identifier": identifier, "password_hash": password_hash}, **kwargs)

    def rejected(port, identifier, password, code=2203, status=401):
        actual, body, headers = login(port, identifier, password)
        assert actual == status, (actual, body)
        assert body["code"] == code
        if status == 401:
            assert headers["WWW-Authenticate"] == "Bearer"
        return body

    def verify_token(token):
        encoded_header, encoded_payload, signature = token.split(".")
        decode = lambda value: base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))
        header = json.loads(decode(encoded_header))
        claims = json.loads(decode(encoded_payload))
        expected_kid = hashlib.sha256(public_key.public_bytes(serialization.Encoding.DER,
                                                             serialization.PublicFormat.SubjectPublicKeyInfo)).hexdigest()
        assert header["alg"] == "PS256" and header["kid"] == expected_kid
        public_key.verify(decode(signature), f"{encoded_header}.{encoded_payload}".encode(),
                          padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=32), hashes.SHA256())
        assert claims["iss"] == "szx-admin" and claims["aud"] == "szx-client"
        assert claims["exp"] - claims["iat"] == 900
        assert claims["exp"] > time.time() and claims["jti"]
        return claims

    with admin("login") as port:
        for payload in (b"{", [], {}, {"identifier": "admin", "password_hash": 1},
                        {"identifier": "admin", "password": "superadmin"},
                        {"identifier": "admin", "password_hash": "superadmin"},
                        {"identifier": "admin", "password_hash": "A" * 64},
                        {"identifier": "admin", "password_hash": "g" * 64},
                        {"identifier": "admin", "password_hash": "a" * 63},
                        {"identifier": "admin", "password_hash": "a" * 65},
                        {"identifier": "admin", "password_hash": "a" * 64, "role": "super_admin"}):
            status, body, _ = post(port, payload)
            assert status == 422 and body["code"] == 2206
        assert login(port, extra_headers={"Origin": "https://evil.example"})[0] == 403
        assert post(port, {"identifier": "admin", "password": "superadmin"},
                    extra_headers={"Content-Type": "text/plain"})[0] == 422
        rejected(port, "admin' OR 1=1 --", "wrong")
        unknown = rejected(port, "no-such-user", "wrong")
        wrong = rejected(port, "admin", "wrong")
        assert unknown == wrong
        assert sql("SELECT failed_login_count FROM login.users;") == "1"
        status, body, headers = login(port, " ADMIN ", request_id="login-success",
                                     extra_headers={"Origin": "https://admin.example.test"})
        assert status == 200, body
        assert headers["Access-Control-Allow-Origin"] == "https://admin.example.test"
        assert headers["Access-Control-Allow-Credentials"] == "true"
        data = body["data"]
        user = data["user"]
        assert user["username"] == "admin" and "uid" not in user
        assert user["role"] == "super_admin" and user["force_password_change"] is True
        assert "password_hash" not in user and "refresh_token" not in data
        stored_password = sql("SELECT password_hash FROM login.users;")
        assert stored_password.startswith("sha256-v1:$argon2id$")
        assert stored_password != hashlib.sha256(b"superadmin").hexdigest()
        claims = verify_token(data["access_token"])
        assert claims["sub"] == user["id"] and claims["authz_version"] == user["authz_version"]
        assert type(data["expires_at"]) is int and data["expires_at"] == claims["exp"] * 1000
        assert data["expires_in"] == 900
        for field in ("created_at", "updated_at", "last_login_at"):
            assert type(user[field]) is int and user[field] > 1_000_000_000_000
        expected_times = sql("SET time_zone='+00:00'; SELECT "
            "CAST(FLOOR(UNIX_TIMESTAMP(created_at)*1000) AS SIGNED),"
            "CAST(FLOOR(UNIX_TIMESTAMP(updated_at)*1000) AS SIGNED),"
            "CAST(FLOOR(UNIX_TIMESTAMP(last_login_at)*1000) AS SIGNED) FROM login.users;")
        expected_created, expected_updated, expected_login = map(int, expected_times.split("\t"))
        assert user["created_at"] == expected_created
        assert user["last_login_at"] == expected_login
        # Login reports its application update time at second precision; MySQL's
        # automatic updated_at includes the fractional part of the write time.
        assert abs(user["updated_at"] - expected_updated) < 2000, (user["updated_at"], expected_updated)
        cookie = SimpleCookie(headers["Set-Cookie"])["szx_refresh"]
        assert cookie["httponly"] and cookie["secure"] and cookie["samesite"] == "None"
        assert cookie["path"] == "/api/v1/auth" and 0 < int(cookie["max-age"]) <= 1800
        assert sql("SELECT token_hash FROM login.refresh_tokens;") == hashlib.sha256(cookie.value.encode()).hexdigest()
        assert data["csrf_token"] == hashlib.sha256(("csrf:" + cookie.value).encode()).hexdigest()
        assert sql("SELECT failed_login_count,last_login_at IS NOT NULL FROM login.users;") == "0\t1"
        assert sql("SELECT COUNT(*) FROM login.security_audit_logs WHERE event_name='auth.login.success' AND request_id='login-success';") == "1"

        # Audit failure must undo last_login, counters and the refresh row.
        sql("UPDATE login.users SET failed_login_count=2,last_login_at=NULL;")
        sql("CREATE TRIGGER login.fail_audit BEFORE INSERT ON login.security_audit_logs FOR EACH ROW "
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='AUDIT_SECRET';")
        rejected(port, "admin", "superadmin", 2212, 500)
        rejected(port, "admin", "wrong", 2212, 500)
        assert sql("SELECT COUNT(*) FROM login.refresh_tokens;") == "1"
        assert sql("SELECT failed_login_count,last_login_at IS NULL FROM login.users;") == "2\t1"
        sql("DROP TRIGGER login.fail_audit;")
        sql("UPDATE login.users SET failed_login_count=0;")
        sql("CREATE TRIGGER login.fail_refresh BEFORE INSERT ON login.refresh_tokens FOR EACH ROW "
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='REFRESH_SECRET';")
        rejected(port, "admin", "superadmin", 2221, 500)
        assert sql("SELECT last_login_at IS NULL FROM login.users;") == "1"
        sql("DROP TRIGGER login.fail_refresh;")

        # Disabled and explicitly locked accounts remain indistinguishable from bad credentials.
        for state in ("disabled", "locked"):
            sql(f"UPDATE login.users SET status='{state}',locked_until='2000-01-01 00:00:00';")
            assert rejected(port, "admin", "superadmin") == wrong
        sql("UPDATE login.users SET status='active',locked_until=NULL,failed_login_count=0;")
        with ThreadPoolExecutor(max_workers=5) as executor:
            responses = list(executor.map(lambda _: login(port, password="concurrent-wrong"), range(5)))
        assert all(response[0] == 401 for response in responses)
        assert sql("SELECT status,failed_login_count,version,authz_version FROM login.users;") == "locked\t5\t2\t2"
        assert sql("SELECT COUNT(*) FROM login.security_audit_logs WHERE event_name='auth.account.locked';") == "1"
        rejected(port, "admin", "superadmin")
        sql("UPDATE login.users SET status='active',locked_until=NULL,failed_login_count=0;")

        # Temporary credentials expire and can establish only one session.
        sql("UPDATE login.users SET temporary_credential_expires_at='2000-01-01 00:00:00';")
        rejected(port, "admin", "superadmin")
        sql("UPDATE login.users SET temporary_credential_expires_at=UTC_TIMESTAMP()+INTERVAL 1 HOUR;")
        assert login(port)[0] == 200
        rejected(port, "admin", "superadmin")
        sql("UPDATE login.users SET temporary_credential_expires_at=NULL;")

        def clone(identity_columns, identity_values):
            sql(f"INSERT INTO login.users(id,{identity_columns},display_name,password_hash,role,status,force_password_change) "
                f"SELECT UUID(),{identity_values},'Test',password_hash,'operator','active',0 FROM login.users WHERE username='admin';")

        clone("uid,uid_canonical", "'U10086','U10086'")
        assert login(port, "U10086")[0] == 200
        rejected(port, "u10086", "superadmin")
        clone("username,username_canonical", "CONVERT(0xc3896e67696e656572 USING utf8mb4),CONVERT(0xc3a96e67696e656572 USING utf8mb4)")
        assert login(port, " E\u0301NGINEER ")[0] == 200
        rejected(port, "engineer", "superadmin")
        clone("username,username_canonical", "'u10086','u10086'")
        rejected(port, "U10086", "superadmin")  # Ambiguous cross-column identity fails closed.
        old_token = data["access_token"]
        secret_values = ("AUDIT_SECRET", "REFRESH_SECRET", old_token, cookie.value, "concurrent-wrong",
                         hashlib.sha256(b"superadmin").hexdigest(), stored_password)
        assert not any(secret in log.read_text(encoding="utf-8") for log in root.glob("admin-*.log") for secret in secret_values)

    with admin("login") as port:
        verify_token(old_token)
        restarted = login(port)
        assert restarted[0] == 200
        verify_token(restarted[1]["data"]["access_token"])
    # Legacy fixtures were independently generated with argon2-cffi, m=65536/t=3/p=1.
    legacy_default = "$argon2id$v=19$m=65536,t=3,p=1$Ud0Q3VeNthnLYKzg1R4aAQ$l9vlqNhIQekZr7Bvf2LdrXaNRXOFcP0btrNnOrL5oko"
    legacy_changed = "$argon2id$v=19$m=65536,t=3,p=1$B5xHs1LatLJ0f6A4hukN/Q$88tMO0gs61HPMX7/XuOwK0qU1Gi42iySjCdz5dyx8e8"
    with admin("legacy_login") as port:
        sql(f"UPDATE legacy_login.users SET password_hash='{legacy_default}';")
        rejected(port, "admin", "wrong")
        assert sql("SELECT password_hash FROM legacy_login.users;") == legacy_default
        sql("CREATE TRIGGER legacy_login.fail_audit BEFORE INSERT ON legacy_login.security_audit_logs FOR EACH ROW "
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='AUDIT_SECRET';")
        rejected(port, "admin", "superadmin", 2212, 500)
        assert sql("SELECT password_hash FROM legacy_login.users;") == legacy_default
        assert sql("SELECT COUNT(*) FROM legacy_login.refresh_tokens;") == "0"
        sql("DROP TRIGGER legacy_login.fail_audit;")
        assert login(port)[0] == 200
        upgraded = sql("SELECT password_hash FROM legacy_login.users;")
        assert upgraded.startswith("sha256-v1:$argon2id$")
        assert login(port)[0] == 200
        assert sql("SELECT password_hash FROM legacy_login.users;") == upgraded
        sql(f"UPDATE legacy_login.users SET password_hash='{legacy_changed}';")
        rejected(port, "admin", "superadmin")
        rejected(port, "admin", "changed-test-password")
        assert sql("SELECT password_hash FROM legacy_login.users;") == legacy_changed
    print("PASS: hash-only login rejects plaintext; legacy initial credential upgrades atomically without resetting changed passwords", flush=True)
    with admin("password_change", allowed_origin="https://business.example.test") as port:
        def change(session, old="superadmin", new="changed-password-2026", version=None, **kwargs):
            headers = {"Authorization": "Bearer " + session["access_token"],
                       "If-Match": str(session["user"]["version"] if version is None else version),
                       "Origin": "https://business.example.test"}
            headers.update(kwargs.pop("extra_headers", {}))
            return post(port, {"old_password_hash": hashlib.sha256(old.encode()).hexdigest(),
                               "new_password_hash": hashlib.sha256(new.encode()).hexdigest()},
                        extra_headers=headers, endpoint="change-password", **kwargs)

        initial = login(port)[1]["data"]
        assert initial["user"]["force_password_change"] is True
        assert initial["user"]["password_change_recommended"] is False
        assert login(port)[0] == 200  # Revoke both refresh sessions on change.
        original_hash = sql("SELECT password_hash FROM password_change.users;")
        for state in ("locked", "disabled"):
            sql(f"UPDATE password_change.users SET status='{state}';")
            assert change(initial)[0] == 401
        sql("UPDATE password_change.users SET status='active';")
        assert change(initial, version=2)[0] == 409
        assert change(initial, old="wrong")[0] == 401
        assert change(initial, new="superadmin")[0] == 422
        assert change(initial, extra_headers={"Authorization": "Bearer invalid"})[0] == 401
        assert change(initial, extra_headers={"If-Match": "*"})[0] == 422
        payload = {"old_password_hash": hashlib.sha256(b"superadmin").hexdigest(),
                   "new_password_hash": hashlib.sha256(b"changed-password-2026").hexdigest()}
        headers = {"Authorization": "Bearer " + initial["access_token"], "If-Match": "1"}
        assert post(port, payload, endpoint="change-password")[0] == 401
        assert post(port, payload, endpoint="change-password",
                    extra_headers={"Authorization": headers["Authorization"]})[0] == 422
        for bad in ({"old_password": "superadmin", "new_password": "changed"},
                    {**payload, "user_id": initial["user"]["id"]},
                    {**payload, "new_password_hash": "A" * 64},
                    {**payload, "new_password_hash": "plaintext"}):
            assert post(port, bad, endpoint="change-password", extra_headers=headers)[0] == 422
        assert sql("SELECT COUNT(*) FROM password_change.security_audit_logs WHERE event_name='auth.password.change_failed';") == "1"
        for table, operation, code in (("security_audit_logs", "INSERT", 2212), ("refresh_tokens", "UPDATE", 2221)):
            sql(f"CREATE TRIGGER password_change.fail_change BEFORE {operation} ON password_change.{table} "
                "FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='CHANGE_SECRET';")
            status, body, _ = change(initial)
            assert status == 500 and body["code"] == code, body
            assert sql("SELECT password_hash FROM password_change.users;") == original_hash
            assert sql("SELECT version,authz_version,force_password_change,password_changed_at IS NULL FROM password_change.users;") == "1\t1\t1\t1"
            assert sql("SELECT COUNT(*) FROM password_change.refresh_tokens WHERE revoked_at IS NULL;") == "2"
            sql("DROP TRIGGER password_change.fail_change;")
        with ThreadPoolExecutor(max_workers=2) as executor:
            results = list(executor.map(lambda _: change(initial), range(2)))
        assert sorted(result[0] for result in results) == [200, 401], results
        changed = next(result for result in results if result[0] == 200)
        assert changed[1]["data"] == {"version": 2, "authz_version": 2, "force_password_change": False,
                                      "password_change_recommended": False, "reauthentication_required": True}
        cookie = SimpleCookie(changed[2]["Set-Cookie"])["szx_refresh"]
        assert cookie.value == "" and cookie["max-age"] == "0" and cookie["path"] == "/api/v1/auth"
        assert cookie["secure"] and cookie["httponly"] and cookie["samesite"] == "None"
        assert changed[2]["Access-Control-Allow-Origin"] == "https://business.example.test"
        assert sql("SELECT COUNT(*) FROM password_change.refresh_tokens WHERE revoked_at IS NOT NULL AND revocation_reason='password_changed';") == "2"
        assert sql("SELECT COUNT(*) FROM password_change.security_audit_logs WHERE event_name='auth.password.changed';") == "1"
        rejected(port, "admin", "superadmin")
        updated = login(port, password="changed-password-2026")[1]["data"]
        assert updated["user"]["force_password_change"] is False
        assert verify_token(updated["access_token"])["authz_version"] == 2
        assert change(initial)[0] == 401  # Old access token fails the database authorization-version check.
        assert change(updated, old="changed-password-2026", new="second-password")[0] == 200

        # A super-admin can change a temporary password consumed by the login that issued this token.
        sql("UPDATE password_change.users SET force_password_change=1,temporary_credential_expires_at=UTC_TIMESTAMP()+INTERVAL 1 HOUR;")
        temporary = login(port, password="second-password")[1]["data"]
        rejected(port, "admin", "second-password")
        assert change(temporary, old="second-password", new="third-password")[0] == 200
        assert sql("SELECT force_password_change,temporary_credential_expires_at IS NULL,password_changed_at IS NOT NULL FROM password_change.users;") == "0\t1\t1"

        # All non-super-admin roles may skip the initial prompt and log in again until credential expiry.
        for role in ("operator", "engineer", "admin"):
            sql("INSERT INTO password_change.users(id,username,username_canonical,display_name,password_hash,role,status,"
                "force_password_change,temporary_credential_expires_at) "
                f"SELECT UUID(),'{role}-user','{role}-user','Test',password_hash,'{role}','active',1,"
                "UTC_TIMESTAMP()+INTERVAL 1 HOUR FROM password_change.users WHERE username='admin';")
            optional = login(port, f"{role}-user", "third-password")[1]["data"]
            assert optional["user"]["force_password_change"] is False
            assert optional["user"]["password_change_recommended"] is True
            assert login(port, f"{role}-user", "third-password")[0] == 200
            assert change(optional, old="third-password", new="optional-password")[0] == 200
            after = login(port, f"{role}-user", "optional-password")[1]["data"]["user"]
            assert after["force_password_change"] is False and after["password_change_recommended"] is False

        request = urllib.request.Request(f"http://127.0.0.1:{port}/api/v1/auth/change-password", method="OPTIONS",
            headers={"Origin": "https://business.example.test", "Access-Control-Request-Method": "POST",
                     "Access-Control-Request-Headers": "authorization, content-type, if-match"})
        with urllib.request.urlopen(request, timeout=5) as response:
            assert response.status == 204
            assert "If-Match" in response.headers["Access-Control-Allow-Headers"]
        secrets = (original_hash, payload["old_password_hash"], payload["new_password_hash"],
                   initial["access_token"], "CHANGE_SECRET", "changed-password-2026")
        assert not any(secret in log.read_text(encoding="utf-8") for log in root.glob("admin-*.log") for secret in secrets)
    print("PASS: required super-admin change, optional non-super-admin prompt, hash-only change, revocation, concurrency and rollback", flush=True)
    with admin("rate", attempts_per_minute=2) as port:
        assert login(port)[0] == 200
        assert login(port)[0] == 200
        rejected(port, "admin", "superadmin", 2211, 429)
        assert sql("SELECT COUNT(*) FROM rate.security_audit_logs WHERE event_name='auth.login.rate_limited';") == "1"
    with admin("wildcard_origin", allowed_origin="*") as port:
        for origin in ("http://localhost:5173", "https://another.example.test"):
            preflight = urllib.request.Request(f"http://127.0.0.1:{port}/api/v1/auth/login", method="OPTIONS",
                headers={"Origin": origin, "Access-Control-Request-Method": "POST",
                         "Access-Control-Request-Headers": "content-type, x-request-id"})
            with urllib.request.urlopen(preflight, timeout=5) as response:
                assert response.status == 204
                assert response.headers["Access-Control-Allow-Origin"] == origin
                assert response.headers["Access-Control-Allow-Credentials"] == "true"
                assert response.headers["Vary"] == "Origin"
            status, body, headers = login(port, extra_headers={"Origin": origin})
            assert status == 200, body
            assert headers["Access-Control-Allow-Origin"] == origin
            assert headers["Access-Control-Allow-Credentials"] == "true"
            cookie = SimpleCookie(headers["Set-Cookie"])["szx_refresh"]
            assert cookie["samesite"] == "None" and cookie["secure"] and cookie["httponly"]
        for origin in ("null", "file://local", "https://host/path"):
            status, body, headers = login(port, extra_headers={"Origin": origin})
            assert status == 403 and body["code"] == 2204
            assert "Access-Control-Allow-Origin" not in headers
        status, body, headers = login(port, password="wrong", extra_headers={"Origin": "https://another.example.test"})
        assert status == 401 and headers["Access-Control-Allow-Origin"] == "https://another.example.test"
        assert sql("SELECT COUNT(*) FROM wildcard_origin.refresh_tokens;") == "2"
    with admin("exact_origin", allowed_origin="https://business.example.test") as port:
        for origin in ("https://evil.example", "null", "https://business.example.test.evil.test",
                       "http://business.example.test", "https://business.example.test:8443"):
            status, body, headers = login(port, extra_headers={"Origin": origin})
            assert status == 403 and body["code"] == 2204
            assert "Access-Control-Allow-Origin" not in headers
        status, body, headers = login(port, extra_headers={"Origin": "https://business.example.test"})
        assert status == 200
        assert headers["Access-Control-Allow-Origin"] == "https://business.example.test"
        assert headers["Access-Control-Allow-Credentials"] == "true"
        assert sql("SELECT COUNT(*) FROM exact_origin.refresh_tokens;") == "1"
    print("PASS: credentialed wildcard CORS, opaque origin rejection and exact origin restrictions", flush=True)
    print("PASS: login PS256/cookie, Unicode identity, uniform failures, concurrent lockout, audit rollback, temporary credentials and rate limiting", flush=True)
    run_session_checks(admin, sql, root, login, post, verify_token)
