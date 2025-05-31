# Dockerfile for Raspberry Pi Pico FreeRTOS Development Environment
# This approach builds packages directly into the image to avoid permission issues

FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04@sha256:8c08fef60259c3c48ed01daaba6d497d6525332d754a2db1dd72cd4c99b5d76a

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set up locale and install dependencies
RUN apt-get update && apt-get install -y locales && \
    locale-gen en_US.UTF-8 && \
    update-locale LANG=en_US.UTF-8 && \
    # Install required packages
    apt-get install -y \
    build-essential \
    cmake \
    curl \
    doxygen \
    gcc-arm-none-eabi \
    gdb-multiarch \
    git \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    python3 \
    python3-pip \
    python3-venv \
    screen \
    sudo \
    uncrustify \
    unzip \
    wget && \
    # Install Node.js
    curl -fsSL https://deb.nodesource.com/setup_lts.x | bash - && \
    apt-get install -y nodejs && \
    # Verify installations
    echo "=== Verifying Installations ===" && \
    arm-none-eabi-gcc --version && \
    cmake --version && \
    python3 --version && \
    uncrustify --version && \
    doxygen --version && \
    echo "=== All tools installed successfully ===" && \
    # Clean up
    rm -rf /var/lib/apt/lists/*

# Set up environment variables
ENV PICO_TOOLCHAIN_PATH=/usr/bin
ENV PATH="/usr/bin:${PATH}"

# Default command
CMD ["/bin/bash"]