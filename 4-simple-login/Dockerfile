FROM --platform=linux/386 docker.io/library/debian:latest

RUN apt-get update \
&& apt-get install -y build-essential clang-tidy gdb strace xxd \
&& rm -rf /var/lib/apt/lists/*
