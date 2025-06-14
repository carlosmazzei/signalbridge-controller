# Uses Debian 12 (Bookworm) as the base version.
# This is the latest stable version of Debian.
FROM debian:12 AS build_stage

# Set environment variables for locale and non-interactivity.
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# Copy dependencies.json first before using it
COPY dependencies.json /tmp/dependencies.json

# Installation of cross-compilation tools and other development dependencies.
# The package versions are from Debian 12 Stable (Bookworm) distribution.
RUN apt-get update && \
    apt-get install -y --no-install-recommends jq sudo && \
    echo "Installing build dependencies..." && \
    # Check if dependencies.json exists and is valid JSON
    if [ ! -f /tmp/dependencies.json ]; then \
        echo "ERROR: dependencies.json not found"; \
        exit 1; \
    fi && \
    if ! jq empty /tmp/dependencies.json 2>/dev/null; then \
        echo "ERROR: dependencies.json is not valid JSON"; \
        exit 1; \
    fi && \
    # Install packages from dependencies.json
    jq -r 'to_entries | .[] | .key + "=" + .value' /tmp/dependencies.json | xargs apt-get install -y && \
    # Clean up
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /var/tmp/* /tmp/dependencies.json

# Settings for the development user inside the container
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the 'vscode' user and set bash as the default shell.
RUN mkdir -p /etc/sudoers.d/ && \
    addgroup --gid ${USER_GID} ${USERNAME} && \
    adduser --uid ${USER_UID} --ingroup ${USERNAME} --shell /bin/bash --disabled-password --gecos "" ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} && \
    chmod 0440 /etc/sudoers.d/${USERNAME} && \
    usermod -aG sudo ${USERNAME} && \
    chown -R ${USERNAME}:${USERNAME} /home/${USERNAME} && \
    rm -rf /tmp/*

USER ${USERNAME}

# Set the default working directory
WORKDIR /workspace

# Set the default shell for the container user (now bash)
CMD ["bash"]