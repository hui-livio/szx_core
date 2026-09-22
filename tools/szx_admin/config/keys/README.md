# Local development keys

This directory contains test-only JWT and TLS keys for local development and debugging.
They are public repository fixtures and must never be used in production.

Before deployment, replace all PEM files with environment-specific keys from a controlled
secret source. Do not package or distribute these development private keys.
