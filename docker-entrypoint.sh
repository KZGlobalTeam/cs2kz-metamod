#!/bin/bash
set -e

# Default: builds the plugin package.
# Vendor mode: rebuilds the prebuilt Linux vendor archives instead.
#   docker run --rm -e BUILD_VENDOR_DEPS=1 -v ./build:/app/build <image>
# Results land in build/vendor/{name}/linux-x86_64/*.a. Copy them over vendor/*.a and commit.

cd build

configure_args=(--enable-optimize)
if [ "${BUILD_VENDOR_DEPS:-0}" != "0" ]; then
	configure_args+=(--build-vendor-deps)
fi

python3 ../configure.py "${configure_args[@]}"
ambuild || exit 1
