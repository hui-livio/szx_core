"""Sample admin HTTP latency without printing tokens, query strings or response bodies.

Use a full URL, --count 100 and optionally --token-env SZX_ADMIN_PROFILE_TOKEN.
The environment variable must contain an existing access token; this tool does
not log in, refresh sessions or modify application data.
"""

import argparse
import collections
import http.client
import json
import math
import os
import statistics
import time
import urllib.parse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url', help='Full HTTP/HTTPS URL, including the desired query')
    parser.add_argument('--count', type=int, default=100)
    parser.add_argument('--interval', type=float, default=0.1, help='Seconds between samples')
    parser.add_argument('--timeout', type=float, default=10)
    parser.add_argument('--slow-ms', type=float, default=1000)
    parser.add_argument('--new-connection', action='store_true')
    parser.add_argument('--token-env', help='Name of an environment variable containing an access token')
    args = parser.parse_args()
    url = urllib.parse.urlsplit(args.url)
    if url.scheme not in ('http', 'https') or not url.hostname or url.username or url.password or url.fragment:
        parser.error('Use an HTTP/HTTPS URL without credentials or fragments')
    if args.count <= 0 or args.interval < 0 or args.timeout <= 0 or args.slow_ms <= 0:
        parser.error('Count, timeout and slow threshold must be positive; interval must be nonnegative')
    token = os.environ.get(args.token_env, '') if args.token_env else ''
    if args.token_env and not token:
        parser.error('The specified access-token environment variable is empty')
    headers = {'Connection': 'close' if args.new_connection else 'keep-alive'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    target = urllib.parse.urlunsplit(('', '', url.path or '/', url.query, ''))
    connection_type = http.client.HTTPSConnection if url.scheme == 'https' else http.client.HTTPConnection
    connection = None
    records = []
    try:
        for index in range(args.count):
            if connection is None:
                connection = connection_type(url.hostname, url.port, timeout=args.timeout)
            started = time.perf_counter()
            try:
                connection.request('GET', target, headers=headers)
                response = connection.getresponse()
                first_byte_ms = (time.perf_counter() - started) * 1000
                # Drain the response to permit reuse. Never print the data it contains.
                response.read()
                record = {
                    'sample': index + 1,
                    'status': response.status,
                    'ttfb_ms': round(first_byte_ms, 3),
                    'total_ms': round((time.perf_counter() - started) * 1000, 3),
                    'request_id': response.getheader('X-Request-ID', ''),
                    'server_timing': response.getheader('Server-Timing', ''),
                }
                records.append(record)
                if record['total_ms'] >= args.slow_ms or response.status >= 400:
                    print(json.dumps(record), flush=True)
                if args.new_connection or response.will_close:
                    connection.close()
                    connection = None
            except (OSError, http.client.HTTPException) as failure:
                records.append({'sample': index + 1, 'error_type': type(failure).__name__})
                print(json.dumps(records[-1]), flush=True)
                connection.close()
                connection = None
            if index + 1 < args.count:
                time.sleep(args.interval)
    finally:
        if connection is not None:
            connection.close()
    timings = sorted(row['total_ms'] for row in records if 'total_ms' in row)
    print(json.dumps({
        'host': url.hostname,
        'path': url.path,
        'connection_mode': 'new' if args.new_connection else 'keepalive',
        'attempts': len(records),
        'responses': len(timings),
        'statuses': dict(collections.Counter(row['status'] for row in records if 'status' in row)),
        'p50_ms': statistics.median(timings) if timings else None,
        'p95_ms': timings[max(0, math.ceil(len(timings) * .95) - 1)] if timings else None,
        'max_ms': max(timings) if timings else None,
        'slow_responses': sum(value >= args.slow_ms for value in timings),
        'responses_with_server_timing': sum(bool(row.get('server_timing')) for row in records),
    }), flush=True)
    return 0 if len(timings) == args.count and all(row['status'] == 200 for row in records) else 1


if __name__ == '__main__':
    raise SystemExit(main())
