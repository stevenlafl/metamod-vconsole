FROM ubuntu:24.04

ARG USER_ID=1000
ARG GROUP_ID=1000

RUN dpkg --add-architecture i386

RUN apt-get update && apt-get install -y \
    gcc-multilib \
    g++-multilib \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    sudo \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -g ${GROUP_ID} builder 2>/dev/null || groupmod -n builder $(getent group ${GROUP_ID} | cut -d: -f1) && \
    useradd -u ${USER_ID} -g ${GROUP_ID} -m -s /bin/bash builder 2>/dev/null || usermod -l builder -d /home/builder -m $(getent passwd ${USER_ID} | cut -d: -f1) && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /app
RUN chown -R builder:builder /app

USER builder
