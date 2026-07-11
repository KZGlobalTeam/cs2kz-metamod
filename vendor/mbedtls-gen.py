#!/usr/bin/env python3
# Regenerates mbedTLS's build-time generated sources into vendor/mbedtls-generated/.
# Rerun after every mbedtls submodule bump:
#   python vendor/mbedtls-gen.py
# Requirements: perl and python3 on PATH, pip packages jinja2 + jsonschema.
# Invocations mirror mbedtls' own CMake rules:
# (library/CMakeLists.txt and tf-psa-crypto/{core,library}/CMakeLists.txt with GEN_FILES=ON).

import os
import posixpath
import shutil
import subprocess
import sys

# Forward slashes throughout for Perl.
VENDOR = os.path.dirname(os.path.abspath(__file__)).replace(os.sep, '/')
MBEDTLS = posixpath.join(VENDOR, 'mbedtls')
TFPSA = posixpath.join(MBEDTLS, 'tf-psa-crypto')
OUT = posixpath.join(VENDOR, 'mbedtls-generated')

EXPECTED = [
    'error.c',
    'version_features.c',
    'ssl_debug_helpers_generated.c',
    'mbedtls_config_check_before.h',
    'mbedtls_config_check_user.h',
    'mbedtls_config_check_final.h',
    'psa_crypto_driver_wrappers.h',
    'psa_crypto_driver_wrappers_no_static.c',
    'tf_psa_crypto_config_check_before.h',
    'tf_psa_crypto_config_check_user.h',
    'tf_psa_crypto_config_check_final.h',
]

def run(argv, cwd):
    print('+', ' '.join(argv))
    subprocess.run(argv, cwd = cwd, check = True)

def main():
    for tool in ('perl', sys.executable):
        if not shutil.which(tool):
            sys.exit(f'{tool} not found on PATH')

    # Not rmtree. Windows can hold the dir open and the recreate then fails.
    os.makedirs(OUT, exist_ok = True)
    for name in os.listdir(OUT):
        os.remove(posixpath.join(OUT, name))

    run(['perl', posixpath.join(MBEDTLS, 'scripts', 'generate_errors.pl'),
         posixpath.join(TFPSA, 'drivers', 'builtin', 'include', 'mbedtls'),
         posixpath.join(MBEDTLS, 'include', 'mbedtls'),
         posixpath.join(MBEDTLS, 'scripts', 'data_files'),
         posixpath.join(OUT, 'error.c')], MBEDTLS)

    run(['perl', posixpath.join(MBEDTLS, 'scripts', 'generate_features.pl'),
         posixpath.join(MBEDTLS, 'include', 'mbedtls'),
         posixpath.join(MBEDTLS, 'scripts', 'data_files'),
         posixpath.join(OUT, 'version_features.c')], MBEDTLS)

    run([sys.executable,
         posixpath.join(MBEDTLS, 'framework', 'scripts', 'generate_ssl_debug_helpers.py'),
         '--mbedtls-root', MBEDTLS, OUT], MBEDTLS)

    run([sys.executable, posixpath.join(MBEDTLS, 'scripts', 'generate_config_checks.py'),
         OUT], MBEDTLS)

    run([sys.executable, posixpath.join(TFPSA, 'scripts', 'generate_config_checks.py'),
         OUT], TFPSA)

    run([sys.executable, posixpath.join(TFPSA, 'scripts', 'generate_driver_wrappers.py'),
         OUT], TFPSA)

    missing = [f for f in EXPECTED if not os.path.isfile(posixpath.join(OUT, f))]
    extra = sorted(set(os.listdir(OUT)) - set(EXPECTED))
    if missing:
        sys.exit(f'missing outputs: {missing}')
    if extra:
        print(f'note: unexpected extra outputs: {extra}')
    print(f'OK: {len(EXPECTED)} files in {OUT}')

if __name__ == '__main__':
    main()
