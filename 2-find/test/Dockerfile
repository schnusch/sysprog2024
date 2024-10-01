FROM docker.io/library/debian:latest

RUN apt-get update \
&& apt-get install -y build-essential sudo zstd \
&& rm -rf /var/lib/apt/lists/*

RUN echo '%sudo ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
RUN useradd --gid users --groups sudo --no-create-home user

COPY find_testbed/create_testbed.sh find_testbed/xdev.tar.zstd /find_testbed/
RUN cd /find_testbed && ./create_testbed.sh
VOLUME /find_testbed/testbed/basic/MNT

COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
