# Usa Debian 12 (Bookworm) como versão base.
# Esta é a versão estável mais atual do Debian.
FROM debian:12 AS build_stage

# Define variáveis de ambiente para localização e não interatividade.
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# Instalação de ferramentas de cross-compilação e outras dependências de desenvolvimento.
# As versões dos pacotes são as da distribuição Debian 12 Stable (Bookworm).
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    "bash=5.2.15-2+b8" \
    "build-essential=12.9" \
    "cmake=3.25.1-1" \
    "doxygen=1.9.4-4" \
    "gcc-arm-none-eabi=15:12.2.rel1-1" \
    "gdb-multiarch=13.1-3" \
    "git=1:2.39.5-0+deb12u2" \
    "libnewlib-arm-none-eabi=3.3.0-1.3+deb12u1" \
    "libstdc++-arm-none-eabi-newlib=15:12.2.rel1-1+23" \
    "openjdk-17-jre-headless=17.0.15+6-1~deb12u1" \
    "openssh-client=1:9.2p1-2+deb12u6" \
    "python3=3.11.2-1+b1" \
    "sudo=1.9.13p3-1+deb12u1" \
    "uncrustify=0.72.0+dfsg1-2" && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /var/tmp/*

ENV PICO_SDK_PATH=/workspace/lib/pico-sdk
ENV FREERTOS_KERNEL_PATH=/workspace/lib/FreeRTOS-Kernel
ENV PICO_TOOLCHAIN_PATH=/usr/bin

# Configurações para o usuário de desenvolvimento dentro do container
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Crie o usuário 'vscode' e defina-o como o shell padrão para bash.
RUN mkdir -p /etc/sudoers.d/ && \
    addgroup --gid ${USER_GID} ${USERNAME} \
    && adduser --uid ${USER_UID} --ingroup ${USERNAME} --shell /bin/bash --disabled-password --gecos "" ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME} \
    && usermod -aG sudo ${USERNAME} \
    && chown -R ${USERNAME}:${USERNAME} /home/${USERNAME} \
    && rm -rf /tmp/*
    
USER ${USERNAME}

# Define o diretório de trabalho padrão
WORKDIR /workspace

# Define o shell padrão para o usuário do container (agora bash)
CMD ["bash"]
