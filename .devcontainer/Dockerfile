# Use Debian 13 (Trixie) as the base version.
# This is the current stable version of Debian.
FROM debian:trixie AS build_stage

# Set environment variables for locale and non-interactivity.
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# Copy dependencies.json from project root - build will FAIL if file doesn't exist
COPY dependencies.json resolve-dependencies.jq /tmp/

# Install packages from dependencies.json
RUN apt-get update && \
    apt-get install -y --no-install-recommends jq sudo && \
    echo "Installing build dependencies from dependencies.json..." && \
    # Validate dependencies.json exists and is valid JSON
    if [ ! -f /tmp/dependencies.json ]; then \
        echo "ERROR: dependencies.json not found"; \
        exit 1; \
    fi && \
    if ! jq empty /tmp/dependencies.json 2>/dev/null; then \
        echo "ERROR: dependencies.json is not valid JSON"; \
        exit 1; \
    fi && \
    # Resolve exact pins for the architecture being built (ARM64/AMD64).
    jq -r --arg architecture "$(dpkg --print-architecture)" \
        -f /tmp/resolve-dependencies.jq /tmp/dependencies.json > /tmp/packages.txt && \
    echo "Installing pinned packages for $(dpkg --print-architecture):" && \
    cat /tmp/packages.txt && \
    # Fail closed: missing pins and package installation errors stop the build.
    xargs -r apt-get install -y --no-install-recommends < /tmp/packages.txt && \
    # Clean up
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /var/tmp/* /tmp/dependencies.json

# Configuration for the development user inside the container
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the 'vscode' user and set bash as the default shell.
RUN mkdir -p /etc/sudoers.d/ && \
    groupadd --gid ${USER_GID} ${USERNAME} && \
    useradd --uid ${USER_UID} --gid ${USER_GID} --shell /bin/bash --create-home ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} && \
    chmod 0440 /etc/sudoers.d/${USERNAME} && \
    (groupadd sudo 2>/dev/null || true) && \
    usermod -aG sudo ${USERNAME} && \
    chown -R ${USERNAME}:${USERNAME} /home/${USERNAME} && \
    rm -rf /tmp/*

USER ${USERNAME}

# Set the default working directory
WORKDIR /workspace

# Set the default shell for the container user (now bash)
CMD ["bash"]
