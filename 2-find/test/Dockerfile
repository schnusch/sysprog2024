FROM docker.io/library/debian:latest

RUN apt-get update \
&& apt-get install -y build-essential sudo \
&& rm -rf /var/lib/apt/lists/*

RUN echo '%sudo ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers
RUN useradd --gid users --groups sudo --no-create-home user

COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
