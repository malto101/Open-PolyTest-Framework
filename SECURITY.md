# Security Policy

## Supported versions

Security fixes are applied to the latest main branch and the most recent release tag.

## Reporting a vulnerability

Please report security issues privately by emailing the repository owner or opening
a private GitHub security advisory on this repository.

Do not open a public issue for vulnerabilities that could affect users in production
or CI environments until a fix or mitigation is available.

## Scope

In scope:

* The PolyOnTest Core harness, CLI, protocol codecs, and first-party plugins
* Supply-chain concerns in release artifacts (`dist/polyontest.h`, `dist/polyontest.c`)

Out of scope:

* Third-party boards, toolchains, or QEMU itself
* Issues that require physical access to a DUT beyond normal HIL use
