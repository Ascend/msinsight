#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
Windows Authenticode signing helpers for release builds.

The production signing key is expected to be backed by GlobalSign EV HSM and
selected from the Windows certificate store by thumbprint. Private keys,
passwords, and token secrets must not be stored in this repository.
"""

import hashlib
import logging
import os
import subprocess
from pathlib import Path


def signing_enabled():
    return os.getenv('MSINSIGHT_SIGN_ENABLE', '').lower() == 'true'


def get_signtool():
    return os.environ.get('MSINSIGHT_SIGNTOOL', 'signtool')


def get_timestamp_url():
    # GlobalSign's RFC 3161 endpoint uses HTTP; the timestamp token itself is cryptographically signed.
    return os.environ.get('MSINSIGHT_TIMESTAMP_URL', 'http://timestamp.globalsign.com/tsa/r6advanced1')


def get_cert_thumbprint():
    return os.environ.get('MSINSIGHT_SIGN_CERT_SHA1')


def run(cmd):
    logging.info('[sign] %s', ' '.join(str(item) for item in cmd))
    subprocess.run(cmd, check=True)


def build_sign_command(target):
    target = Path(target)
    if not target.is_file():
        raise FileNotFoundError(f'Sign target not found: {target}')

    thumbprint = get_cert_thumbprint()
    if not thumbprint:
        raise RuntimeError('MSINSIGHT_SIGN_CERT_SHA1 is required for GlobalSign EV HSM signing.')

    return [
        get_signtool(),
        'sign',
        '/sha1',
        thumbprint,
        '/fd',
        'SHA256',
        '/tr',
        get_timestamp_url(),
        '/td',
        'SHA256',
        '/v',
        str(target),
    ]


def sign_file(target):
    if not signing_enabled():
        return
    run(build_sign_command(target))


def verify_file(target):
    if not signing_enabled():
        return

    target = Path(target)
    if not target.is_file():
        raise FileNotFoundError(f'Verify target not found: {target}')

    run([get_signtool(), 'verify', '/pa', '/v', str(target)])


def sign_and_verify(target):
    sign_file(target)
    verify_file(target)


def sha256sum(target):
    target = Path(target)
    h = hashlib.sha256()

    with target.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            h.update(chunk)

    return h.hexdigest().upper()


def print_sha256(target):
    target = Path(target)
    if not target.is_file():
        raise FileNotFoundError(f'Hash target not found: {target}')
    logging.info('[sign] SHA256 %s: %s', target.name, sha256sum(target))


def sign_many(targets):
    if not signing_enabled():
        return

    for target in targets:
        target = Path(target)
        if target.is_file():
            sign_and_verify(target)
        else:
            logging.warning('[sign] Skip missing file: %s', target)
