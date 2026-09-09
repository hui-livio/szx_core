#!/usr/bin/env python3
"""End-to-end tests for the SZX license request, issue, and verify workflow.

The test intentionally drives the public command-line tools instead of calling
the C++ library directly.  It covers the successful client/issuer flow and
several expected failures (product mismatch, tampering, malformed requests,
and invalid CLI input).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Sequence


class TestFailure(RuntimeError):
    """Raised when an end-to-end assertion fails."""


def _executable_candidates(path: Path) -> list[Path]:
    if path.suffix.lower() == ".exe":
        return [path]
    return [path, path.with_name(path.name + ".exe")]


def _find_executable(explicit: Path | None, build_dir: Path, name: str, root: Path) -> Path:
    if explicit is not None:
        for candidate in _executable_candidates(explicit if explicit.is_absolute() else Path.cwd() / explicit):
            if candidate.is_file():
                return candidate.resolve()
        raise TestFailure(f"executable not found: {explicit}")

    build_root = build_dir if build_dir.is_absolute() else root / build_dir
    candidates: list[Path] = []
    for output_dir in (
        build_root / "bin",
        build_root / "bin" / "RelWithDebInfo",
        build_root / "bin" / "Release",
        build_root / "out" / "bin" / "RelWithDebInfo",
        build_root / "out" / "bin" / "Release",
        build_root / "out" / "bin",
    ):
        candidates.extend(output_dir / candidate for candidate in _executable_candidates(Path(name)))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    searched = ", ".join(str(candidate) for candidate in candidates)
    raise TestFailure(f"{name} not found; searched: {searched}")


def _format_command(command: Sequence[str]) -> str:
    # This is for diagnostics only; subprocess receives the original argument
    # list and therefore does not depend on shell quoting rules.
    return " ".join(f'"{arg}"' if any(char.isspace() for char in arg) else arg for arg in command)


def _run(command: Sequence[str], label: str, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    print(f"\n[{label}]\n$ {_format_command(command)}")
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, encoding="utf-8", errors="replace", check=False
        )
    except OSError as error:
        raise TestFailure(f"failed to start command: {error}") from error

    if result.stdout:
        print(result.stdout.rstrip())
    if result.stderr:
        print(result.stderr.rstrip(), file=sys.stderr)

    succeeded = result.returncode == 0
    if succeeded != expect_success:
        expectation = "success" if expect_success else "failure"
        raise TestFailure(f"{label}: expected {expectation}, got exit code {result.returncode}")
    return result


def _assert_file(path: Path, label: str) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise TestFailure(f"{label}: expected non-empty file: {path}")


def _assert_contains(value: str, expected: str, label: str) -> None:
    if expected not in value:
        raise TestFailure(f"{label}: expected {expected!r} in output")


def _assert_header(path: Path, header: bytes, label: str) -> None:
    _assert_file(path, label)
    if path.read_bytes()[: len(header)] != header:
        raise TestFailure(f"{label}: unexpected file header in {path}")


def _run_flow(user_demo: Path, issuer: Path, temp_dir: Path) -> None:
    private_key = temp_dir / "issuer-private.pem"
    public_key = temp_dir / "issuer-public.pem"
    request = temp_dir / "client-request.szxreq"
    license_file = temp_dir / "issued-license.szxlic"

    _run(
        [
            str(issuer),
            "keygen",
            "--private-key",
            str(private_key),
            "--public-key",
            str(public_key),
        ],
        "generate issuer PEM key pair",
    )
    _assert_contains(private_key.read_text(encoding="utf-8"), "BEGIN PRIVATE KEY", "private key format")
    _assert_contains(public_key.read_text(encoding="utf-8"), "BEGIN PUBLIC KEY", "public key format")

    request_result = _run(
        [
            str(user_demo),
            "request",
            "--product",
            "szx.e2e",
            "--version",
            "1.2.3",
            "--public-key",
            str(public_key),
            "--output",
            str(request),
        ],
        "generate encrypted license request",
    )
    _assert_file(request, "request generation")
    _assert_header(request, b"SZXR", "request envelope")
    _assert_contains(request_result.stdout, "request file generated", "request result")

    _run(
        [
            str(issuer),
            "issue",
            "--request",
            str(request),
            "--private-key",
            str(private_key),
            "--public-key",
            str(public_key),
            "--output",
            str(license_file),
            "--license-id",
            "e2e-license-001",
            "--issuer",
            "e2e-test",
            "--days",
            "30",
        ],
        "issue signed license",
    )
    _assert_header(license_file, b"SZXL", "license envelope")

    verify_result = _run(
        [
            str(user_demo),
            "verify",
            "--input",
            str(license_file),
            "--public-key",
            str(public_key),
            "--product",
            "szx.e2e",
        ],
        "verify signed license",
    )
    for expected in (
        "license_id=e2e-license-001",
        "product_id=szx.e2e",
        "app_version=1.2.3",
        "issuer=e2e-test",
    ):
        _assert_contains(verify_result.stdout, expected, "license verification result")


def _run_error_cases(user_demo: Path, issuer: Path, temp_dir: Path) -> None:
    private_key = temp_dir / "issuer-private.pem"
    public_key = temp_dir / "issuer-public.pem"
    request = temp_dir / "client-request.szxreq"
    license_file = temp_dir / "issued-license.szxlic"

    wrong_product = _run(
        [
            str(user_demo),
            "verify",
            "--input",
            str(license_file),
            "--public-key",
            str(public_key),
            "--product",
            "wrong.product",
        ],
        "reject wrong product",
        expect_success=False,
    )
    _assert_contains(wrong_product.stderr, "product mismatch", "wrong product error")

    tampered_license = temp_dir / "tampered-license.szxlic"
    tampered_bytes = bytearray(license_file.read_bytes())
    tampered_bytes[-1] ^= 0x01
    tampered_license.write_bytes(tampered_bytes)
    tampered = _run(
        [
            str(user_demo),
            "verify",
            "--input",
            str(tampered_license),
            "--public-key",
            str(public_key),
            "--product",
            "szx.e2e",
        ],
        "reject tampered license",
        expect_success=False,
    )
    _assert_contains(tampered.stderr, "signature", "tampered license error")

    malformed_request = temp_dir / "malformed-request.szxreq"
    malformed_bytes = bytearray(request.read_bytes())
    malformed_bytes[0] = ord("X")
    malformed_request.write_bytes(malformed_bytes)
    malformed = _run(
        [
            str(issuer),
            "issue",
            "--request",
            str(malformed_request),
            "--private-key",
            str(private_key),
            "--public-key",
            str(public_key),
            "--output",
            str(temp_dir / "malformed-license.szxlic"),
        ],
        "reject malformed request",
        expect_success=False,
    )
    _assert_contains(malformed.stderr, "invalid request envelope", "malformed request error")

    invalid_days = _run(
        [
            str(issuer),
            "issue",
            "--request",
            str(request),
            "--private-key",
            str(private_key),
            "--public-key",
            str(public_key),
            "--output",
            str(temp_dir / "invalid-days.szxlic"),
            "--days",
            "not-a-number",
        ],
        "reject invalid days option",
        expect_success=False,
    )
    _assert_contains(invalid_days.stderr, "not in range", "invalid days error")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="Build directory (default: build)")
    parser.add_argument("--user-demo", type=Path, help="Path to szx_license_user_demo(.exe)")
    parser.add_argument("--issuer", type=Path, help="Path to szx_license_issuer(.exe)")
    parser.add_argument("--keep-temp", action="store_true", help="Keep generated test files")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    try:
        user_demo = _find_executable(args.user_demo, args.build_dir, "szx_license_user_demo", root)
        issuer = _find_executable(args.issuer, args.build_dir, "szx_license_issuer", root)
        if args.keep_temp:
            temp_dir = Path(tempfile.mkdtemp(prefix="szx-license-flow-"))
            cleanup = False
        else:
            temp_context = tempfile.TemporaryDirectory(prefix="szx-license-flow-")
            temp_dir = Path(temp_context.name)
            cleanup = True

        try:
            print(f"Using user demo: {user_demo}")
            print(f"Using issuer: {issuer}")
            print(f"Test data directory: {temp_dir}")
            _run_flow(user_demo, issuer, temp_dir)
            _run_error_cases(user_demo, issuer, temp_dir)
        finally:
            if cleanup:
                temp_context.cleanup()
            else:
                print(f"Kept test data directory: {temp_dir}")
    except TestFailure as error:
        print(f"\nFAIL: {error}", file=sys.stderr)
        return 1

    print("\nPASS: license request, issue, verify, and error-case tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
