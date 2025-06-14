# Use Debian 12 (Bookworm) as the base version.
# This is the current stable version of Debian.
FROM debian:12 AS build_stage

# Set environment variables for locale and non-interactivity.
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# Copy dependencies.json from project root - build will FAIL if file doesn't exist
COPY dependencies.json /tmp/dependencies.json

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
    # Show what packages will be installed
    echo "Packages to install:" && \
    jq -r 'to_entries | .[] | "  - " + .key + " (version: " + .value + ")"' /tmp/dependencies.json && \
    echo "Starting installation..." && \
    # Install packages - try with version, fallback to package name only
    jq -r 'keys[]' /tmp/dependencies.json | while read package; do \
        version=$(jq -r --arg pkg "$package" '.[$pkg]' /tmp/dependencies.json); \
        echo "Installing $package..."; \
        if [ -n "$version" ] && [ "$version" != "" ] && [ "$version" != "null" ]; then \
            if ! apt-get install -y "$package=$version" 2>/dev/null; then \
                echo "Version $version not available for $package, installing latest..."; \
                apt-get install -y "$package" || echo "Failed to install $package"; \
            fi; \
        else \
            apt-get install -y "$package" || echo "Failed to install $package"; \
        fi; \
    done && \
    echo "Package installation completed" && \
    # Clean up
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /var/tmp/* /tmp/dependencies.json

# Configuration for the development user inside the container
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