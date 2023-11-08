FROM debian:stable

RUN apt-get update -y && apt-get install -y \
    build-essential \
    cmake \
    git \
    sudo

RUN useradd -G sudo -ms /bin/bash vscode