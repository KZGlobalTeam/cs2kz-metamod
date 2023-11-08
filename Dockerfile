FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

# Update/Upgrade and Install Pip (for AMBuild)
RUN set -x \
        && apt-get update && apt-get install -y \
        --no-install-recommends --no-install-suggests python3-pip

# Install AMBuild
RUN git clone https://github.com/alliedmodders/ambuild && pip install ./ambuild

# Compile cs2kz
RUN git clone --recurse-submodules https://github.com/zer0k-z/cs2kz_metamod.git \
        && cd cs2kz_metamod \
        && git submodule update --init --recursive \
        && mkdir build \
        && cd build \
        && python3 ../configure.py --hl2sdk-root "../" \
        && ambuild
