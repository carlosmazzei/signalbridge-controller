# Use Debian 13 (Trixie) as the base version.
# This is the current stable version of Debian.
FROM debian:trixie AS build_stage

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
    jq -r 'keys[] | select(. != "sonar-scanner-cli")' /tmp/dependencies.json | while read package; do \
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
    rm -rf /var/lib/apt/lists/* /var/tmp/*

# Install SonarQube scanner CLI for local, offline scans based on dependencies.json
ARG SONAR_SCANNER_VERSION
ENV SONAR_SCANNER_HOME=/opt/sonar-scanner
ENV PATH="${SONAR_SCANNER_HOME}/bin:${PATH}"

RUN set -euo pipefail && \
    DEP_FILE=/tmp/dependencies.json && \
    SCANNER_VERSION="${SONAR_SCANNER_VERSION:-}" && \
    if [ -z "${SCANNER_VERSION}" ] || [ "${SCANNER_VERSION}" = "" ]; then \
        SCANNER_VERSION=$(jq -r '."sonar-scanner-cli" // empty' "${DEP_FILE}"); \
    fi && \
    if [ -z "${SCANNER_VERSION}" ] || [ "${SCANNER_VERSION}" = "null" ]; then \
        echo "sonar-scanner-cli version not specified in dependencies.json"; \
        exit 1; \
    fi && \
    SCANNER_ZIP="/tmp/sonar-scanner.zip" && \
    curl -sSLo "${SCANNER_ZIP}" "https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-${SCANNER_VERSION}-linux-x64.zip" && \
    python3 - <<'PY'
import os
import zipfile

zip_path = os.environ["SCANNER_ZIP"]
with zipfile.ZipFile(zip_path) as zf:
    zf.extractall("/tmp")
PY
    mv "/tmp/sonar-scanner-${SCANNER_VERSION}-linux-x64" "${SONAR_SCANNER_HOME}" && \
    ln -sf "${SONAR_SCANNER_HOME}/bin/sonar-scanner" /usr/local/bin/sonar-scanner && \
    rm -f "${SCANNER_ZIP}" "${DEP_FILE}"

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