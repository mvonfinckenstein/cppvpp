FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git curl ca-certificates gnupg \
    libgtest-dev doxygen gdb golang-go && rm -rf /var/lib/apt/lists/*
# VPP packages (includes the SDK: vpp-dev, vpp-plugin-core, vpp-api-*).
RUN curl -fsSL https://packagecloud.io/install/repositories/fdio/release/script.deb.sh | bash \
    && apt-get update && apt-get install -y --no-install-recommends \
       vpp vpp-plugin-core vpp-dev libvppinfra-dev \
    && rm -rf /var/lib/apt/lists/*
