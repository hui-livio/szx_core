"""Role management checks on the disposable database owned by test_admin_metrics.py."""


def run_role_checks(call, sql, login, service, config):
    import hashlib
    from concurrent.futures import ThreadPoolExecutor

    token = login('admin', 'superadmin')

    def get(path):
        status, body, _ = call('GET', path, token=token)
        assert status == 200, (path, status, body)
        return body['data']

    def write(method, path, data=None, version=None, want=200, code=None):
        nonlocal token
        headers = {'If-Match': str(version)} if version is not None else None
        status, body, _ = call(method, path, data, token, headers)
        assert status == want, (method, path, status, body)
        if code is not None:
            assert body['code'] == code, body
        if 200 <= status < 300:
            token = login('admin', 'superadmin')
        return body.get('data')

    created = write('POST', '/admin/roles', {'code': 'custom_viewer', 'name': 'Custom viewer', 'description': 'Role description'}, want=201)
    assert created['version'] == 1 and created['authz_version'] == 1
    assert get('/admin/users')['total'] >= 4
    assert get('/admin/users?role=custom_viewer')['total'] == 0
    path = '/admin/roles/' + created['id']
    current = get(path)
    assert current['manageable'] and current['user_count'] == 0 and current['permission_count'] == 0, current
    root = next(role for role in get('/admin/roles')['items'] if role['system_key'] == 'super_admin')
    scope = get('/admin/roles/' + root['id'] + '/manage-scopes')['target_role_ids']
    assert created['id'] in scope
    write('PUT', '/admin/roles/' + root['id'] + '/manage-scopes', {'target_role_ids': []}, root['version'], want=409, code=2233)
    write('PATCH', '/admin/roles/' + root['id'] + '/status', {'status': 'disabled'}, root['version'], want=409, code=2231)
    write('DELETE', '/admin/roles/' + root['id'], version=root['version'], want=409, code=2231)
    write('POST', '/admin/roles', {'code': 'custom_viewer', 'name': 'Duplicate'}, want=409, code=2227)
    edited = write('PATCH', path, {'name': 'Custom viewer v2', 'description': 'Edited description'}, current['version'])
    assert edited['version'] == 2 and edited['description'] == 'Edited description'
    write('PATCH', path, {'name': 'Stale'}, current['version'], want=409, code=2209)
    catalog = get('/admin/permissions')['items']
    permission = next(item for item in catalog if item['code'] == 'business.read')
    assigned = write('PUT', path + '/permissions', {'permission_ids': [permission['id']]}, edited['version'])
    targets = get('/admin/roles')['items']
    operator = next(item for item in targets if item['code'] == 'operator')
    scoped = write('PUT', path + '/manage-scopes', {'target_role_ids': [operator['id']]}, assigned['version'])
    assert get(path)['permission_count'] == 1
    user = write('POST', '/admin/users', {'login_identifier': 'custom_role_user', 'display_name': 'Custom role user', 'role': created['code'], 'password_hash': hashlib.sha256(b'CustomRoleTest123').hexdigest()}, want=201)['user']
    assert get(path)['user_count'] == 1
    listed = get('/admin/users?role=custom_viewer')
    assert listed['total'] == 1 and listed['items'][0]['id'] == user['id'], listed
    assert get('/admin/users')['total'] >= 5
    write('DELETE', path, version=scoped['version'], want=409, code=2229)
    custom_token = login('custom_role_user', 'CustomRoleTest123')
    status, body, _ = call('GET', '/client/me/permissions', token=custom_token)
    assert status == 200 and body['data']['capabilities'] == ['business.read'], body
    assert body['data']['manageable_roles'] == ['operator'], body
    disabled = write('PATCH', path + '/status', {'status': 'disabled'}, scoped['version'])
    own_permissions = get('/client/me/permissions')
    assert created['code'] in own_permissions['manageable_roles']
    assert created['code'] not in own_permissions['assignable_roles']
    status, body, _ = call('GET', '/client/me/permissions', token=custom_token)
    assert status == 401, body
    status, body, _ = call('POST', '/auth/login', {'login_identifier': 'custom_role_user', 'password_hash': hashlib.sha256(b'CustomRoleTest123').hexdigest()})
    assert status == 401 and body['code'] == 2203, body
    active = write('PATCH', path + '/status', {'status': 'active'}, disabled['version'])
    write('PUT', '/admin/users/' + user['id'] + '/role', {'role': 'operator'}, user['version'])
    assert sql("SELECT role_id FROM metrics_test.users WHERE login_identifier='custom_role_user';") == operator['id']
    assert get(path)['user_count'] == 0
    assert get('/admin/users?role=custom_viewer')['total'] == 0
    assert get('/admin/users?role=operator')['total'] >= 2
    write('DELETE', path, version=active['version'], want=204)
    archived = get(path)
    assert archived['status'] == 'archived' and archived['permission_count'] == 0
    assert get(path + '/permissions')['permission_ids'] == []
    assert get(path + '/manage-scopes')['target_role_ids'] == []
    root = get('/admin/roles/' + root['id'])
    assert created['id'] not in get('/admin/roles/' + root['id'] + '/manage-scopes')['target_role_ids']
    write('POST', '/admin/roles', {'code': 'custom_viewer', 'name': 'Archived duplicate'}, want=409, code=2227)
    write('PATCH', path, {'name': 'Resurrect'}, archived['version'], want=409, code=2231)
    write('PATCH', path + '/status', {'status': 'active'}, archived['version'], want=409, code=2231)
    reader = write('POST', '/admin/roles', {'code': 'review_reader', 'name': 'Review reader'}, want=201)
    reader_path = '/admin/roles/' + reader['id']
    user_read = next(item for item in catalog if item['code'] == 'user.read')
    reader = write('PUT', reader_path + '/permissions', {'permission_ids': [user_read['id']]}, reader['version'])
    reader = write('PUT', reader_path + '/manage-scopes', {'target_role_ids': [reader['id']]}, reader['version'])
    reader_user = write('POST', '/admin/users', {'login_identifier': 'review_reader_user', 'display_name': 'Review reader user', 'role': reader['code'], 'password_hash': hashlib.sha256(b'CustomRoleTest123').hexdigest()}, want=201)['user']
    reader_token = login('review_reader_user', 'CustomRoleTest123')
    for resource in ['/admin/users', '/admin/users?role=review_reader']:
        status, body, _ = call('GET', resource, token=reader_token)
        assert status == 200 and body['data']['total'] == 1 and body['data']['items'][0]['id'] == reader_user['id'], (resource, status, body)
    status, body, _ = call('GET', '/admin/users?role=super_admin', token=reader_token)
    assert status == 403 and body['code'] == 2204, body
    for capability, allowed_path, denied_path in [
        ('behavior.read', '/admin/behavior-events', '/admin/audit-logs'),
        ('audit.read', '/admin/audit-logs', '/admin/behavior-events'),
    ]:
        selected_permission = next(item for item in catalog if item['code'] == capability)
        reader = write('PUT', reader_path + '/permissions', {'permission_ids': [selected_permission['id']]}, reader['version'])
        reader_token = login('review_reader_user', 'CustomRoleTest123')
        status, body, _ = call('GET', allowed_path, token=reader_token)
        assert status == 200, (capability, status, body)
        status, body, _ = call('GET', denied_path, token=reader_token)
        assert status == 403 and body['code'] == 2204, (capability, status, body)
    write('PUT', '/admin/users/' + reader_user['id'] + '/role', {'role': 'operator'}, reader_user['version'])
    write('DELETE', reader_path, version=reader['version'], want=204)
    print('PASS: default/custom-filtered user directories work for super-admin and dynamic managers; independent audit/behavior capabilities remain enforced')
    sql("CREATE TRIGGER metrics_test.reject_role_audit BEFORE INSERT ON metrics_test.security_audit_logs FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test-only audit rejection';")
    write('POST', '/admin/roles', {'code': 'role_rollback', 'name': 'Rollback'}, want=500)
    sql('DROP TRIGGER metrics_test.reject_role_audit;')
    assert sql("SELECT COUNT(*) FROM metrics_test.roles WHERE code='role_rollback';") == '0'
    credential = token
    with ThreadPoolExecutor(max_workers=2) as executor:
        responses = list(executor.map(lambda _: call('POST', '/admin/roles', {'code': 'role_concurrent', 'name': 'Concurrent'}, credential), range(2)))
    assert sorted(item[0] for item in responses) in ([201, 409], [201, 401]), responses
    token = login('admin', 'superadmin')
    write('POST', '/admin/roles', {'code': 'role_concurrent', 'name': 'Duplicate'}, want=409, code=2227)
    import time
    before = int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1])
    sql("CREATE TRIGGER metrics_test.slow_role_insert BEFORE INSERT ON metrics_test.roles FOR EACH ROW DO SLEEP(0.5);")
    try:
        credential = token
        with ThreadPoolExecutor(max_workers=2) as executor:
            create_role = executor.submit(call, 'POST', '/admin/roles', {'code': 'role_user_race', 'name': 'Concurrent role'}, credential)
            time.sleep(0.1)
            create_user = executor.submit(call, 'POST', '/admin/users', {'login_identifier': 'role_race_user', 'display_name': 'Concurrent user', 'role': 'super_admin', 'password_hash': hashlib.sha256(b'CustomRoleTest123').hexdigest()}, credential)
            responses = [create_role.result(), create_user.result()]
        assert all(item[0] in [201, 401] for item in responses), responses
        assert int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1]) == before
    finally:
        sql('DROP TRIGGER metrics_test.slow_role_insert;')
    token = login('admin', 'superadmin')
    first = write('POST', '/admin/roles', {'code': 'scope_race_first', 'name': 'Scope race first'}, want=201)
    second = write('POST', '/admin/roles', {'code': 'scope_race_second', 'name': 'Scope race second'}, want=201)
    target, manager = sorted([first, second], key=lambda role: role['id'])
    manager_path = '/admin/roles/' + manager['id']
    write('PUT', manager_path + '/manage-scopes', {'target_role_ids': [manager['id']]}, manager['version'])
    custom_permission = write('POST', '/admin/permissions', {'code': 'report.scope_race', 'name': 'Scope race', 'module': 'reports'}, want=201)
    for role in [target, manager]:
        current_role = get('/admin/roles/' + role['id'])
        write('PUT', '/admin/roles/' + role['id'] + '/permissions', {'permission_ids': [custom_permission['id']]}, current_role['version'])
    current_role = get(manager_path)
    write('POST', '/admin/users', {'login_identifier': 'scope_race_admin', 'display_name': 'Scope race admin', 'role': 'super_admin', 'password_hash': hashlib.sha256(b'CustomRoleTest123').hexdigest()}, want=201)
    sql("UPDATE metrics_test.users SET force_password_change=FALSE WHERE login_identifier='scope_race_admin';")
    second_credential = login('scope_race_admin', 'CustomRoleTest123')
    before = int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1])
    sql("CREATE TRIGGER metrics_test.slow_scope_delete BEFORE DELETE ON metrics_test.role_manage_scopes FOR EACH ROW DO SLEEP(IF(OLD.manager_role_id='" + manager['id'] + "',1,0));")
    try:
        import json
        import re
        import urllib.error
        import urllib.request
        from test_admin_database import free_port, running, wait_until
        race_port = free_port()
        race_config = config.read_text()
        first_port = re.search(r'server:\n.*?\n  port: (\d+)', race_config, re.S).group(1)
        race_config = race_config.replace('  port: ' + first_port + '\n', '  port: ' + str(race_port) + '\n', 1)
        race_path = config.parent / 'roles-race.yaml'
        race_path.write_text(race_config.replace(str(config.parent / 'service.log'), str(config.parent / 'service-role-race.log')))
        credential = token
        def second_call(method, resource, payload=None, headers=None):
            request = urllib.request.Request(f'http://127.0.0.1:{race_port}/api/v1{resource}', data=None if payload is None else json.dumps(payload).encode(), method=method, headers={'Content-Type': 'application/json', 'Authorization': 'Bearer ' + second_credential, **(headers or {})})
            try:
                response = urllib.request.urlopen(request, timeout=5)
            except urllib.error.HTTPError as failure:
                response = failure
            with response:
                return response.status, json.loads(response.read())
        with running([str(service), '--config', str(race_path)], config.parent, config.parent / 'role-race.log') as process:
            def race_ready():
                try: return second_call('GET', '/ready')[0] == 200
                except OSError: return False
            wait_until(race_ready, process)
            with ThreadPoolExecutor(max_workers=2) as executor:
                scope_update = executor.submit(call, 'PUT', manager_path + '/manage-scopes', {'target_role_ids': [target['id']]}, credential, {'If-Match': str(current_role['version'])})
                time.sleep(0.2)
                permission_update = executor.submit(second_call, 'PATCH', '/admin/permissions/' + custom_permission['id'] + '/status', {'status': 'disabled'}, {'If-Match': str(custom_permission['version'])})
                responses = [scope_update.result(), permission_update.result()]
        assert responses[0][0] in [200, 409] and responses[1][0] == 200, responses
        assert int(sql("SHOW GLOBAL STATUS LIKE 'Innodb_deadlocks';").split()[-1]) == before
    finally:
        sql('DROP TRIGGER metrics_test.slow_scope_delete;')
    print('PASS: dual-instance, different-actor scope replacement/permission status race: no HTTP 500, no new InnoDB deadlocks')
    import re
    from pathlib import Path
    import urllib.request
    from test_admin_database import free_port, running, wait_until

    sql("UPDATE metrics_test.users u JOIN metrics_test.roles r ON r.code='engineer' SET u.role_id=r.id WHERE u.login_identifier='custom_role_user';")
    sql("DELETE FROM metrics_test.schema_migrations WHERE version=12;")
    sql("ALTER TABLE metrics_test.users MODIFY role VARCHAR(16) NOT NULL, ADD CONSTRAINT chk_users_role CHECK (role IN ('operator','engineer','admin','super_admin'));")
    config_path = Path(config)
    port = free_port()
    upgraded_config = config_path.read_text()
    first_port = re.search(r'server:\n.*?\n  port: (\d+)', upgraded_config, re.S).group(1)
    upgraded_config = upgraded_config.replace('  port: ' + first_port + '\n', '  port: ' + str(port) + '\n', 1)
    upgraded_config = upgraded_config.replace(str(config_path.parent / 'service.log'), str(config_path.parent / 'service-upgrade.log'))
    upgraded_path = config_path.parent / 'roles-upgrade.yaml'
    upgraded_path.write_text(upgraded_config)
    with running([str(service), '--config', str(upgraded_path)], config_path.parent, config_path.parent / 'roles-upgrade.log') as process:
        def ready():
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/v1/ready', timeout=2) as response:
                    return response.status == 200
            except OSError:
                return False
        wait_until(ready, process)
        assert sql("SELECT MAX(version) FROM metrics_test.schema_migrations;") == '12'
        assert sql("SELECT role_id FROM metrics_test.users WHERE login_identifier='custom_role_user';") == operator['id']
        assert sql("SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='metrics_test' AND TABLE_NAME='users' AND COLUMN_NAME='role';") == '64'
    print('PASS: isolated v11-to-v12 startup upgrade restores dynamic role columns and repairs legacy role_id mismatches')
    import subprocess

    def restart_recovered():
        with running([str(service), '--config', str(upgraded_path)], config_path.parent, config_path.parent / 'roles-recovery.log') as process:
            wait_until(ready, process)
            assert sql("SELECT MAX(version) FROM metrics_test.schema_migrations;") == '12'
            assert sql("SELECT role_id FROM metrics_test.users WHERE login_identifier='custom_role_user';") == operator['id']
            assert sql("SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='metrics_test' AND TABLE_NAME='users' AND COLUMN_NAME='role';") == '64'

    version_before = int(sql("SELECT authz_version FROM metrics_test.users WHERE login_identifier='custom_role_user';"))
    sql("UPDATE metrics_test.users u JOIN metrics_test.roles r ON r.code='engineer' SET u.role_id=r.id WHERE u.login_identifier='custom_role_user';")
    sql("DELETE FROM metrics_test.schema_migrations WHERE version=12;")
    sql("ALTER TABLE metrics_test.users MODIFY role VARCHAR(16) NOT NULL, ADD CONSTRAINT chk_users_role CHECK (role IN ('operator','engineer','admin','super_admin'));")
    sql("CREATE TRIGGER metrics_test.reject_role_repair BEFORE UPDATE ON metrics_test.users FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test-only repair rejection';")
    try:
        failed = subprocess.run([str(service), '--config', str(upgraded_path)], cwd=config_path.parent, capture_output=True, text=True, timeout=20)
        assert failed.returncode != 0, failed.stdout
        assert sql("SELECT MAX(version) FROM metrics_test.schema_migrations;") == '11'
        assert sql("SELECT COUNT(*) FROM information_schema.TABLE_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='metrics_test' AND TABLE_NAME='users' AND CONSTRAINT_NAME='chk_users_role';") == '0'
        assert sql("SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='metrics_test' AND TABLE_NAME='users' AND COLUMN_NAME='role';") == '64'
    finally:
        sql('DROP TRIGGER metrics_test.reject_role_repair;')
    restart_recovered()
    assert int(sql("SELECT authz_version FROM metrics_test.users WHERE login_identifier='custom_role_user';")) == version_before + 1
    sql("DELETE FROM metrics_test.schema_migrations WHERE version=12;")
    sql("CREATE TRIGGER metrics_test.reject_role_journal BEFORE INSERT ON metrics_test.schema_migrations FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='test-only journal rejection';")
    try:
        failed = subprocess.run([str(service), '--config', str(upgraded_path)], cwd=config_path.parent, capture_output=True, text=True, timeout=20)
        assert failed.returncode != 0, failed.stdout
        assert sql("SELECT MAX(version) FROM metrics_test.schema_migrations;") == '11'
    finally:
        sql('DROP TRIGGER metrics_test.reject_role_journal;')
    restart_recovered()
    restart_recovered()
    assert int(sql("SELECT authz_version FROM metrics_test.users WHERE login_identifier='custom_role_user';")) == version_before + 1
    print('PASS: migration 12 resumes after committed DDL, failed data repair and failed journal persistence; repeated startup does not repeat authorization bumps')
    print('PASS: role CRUD, scopes, counts, system protection, archive references, audit rollback, code uniqueness and custom-role login')
