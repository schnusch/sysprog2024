#!/bin/sh
: ${DOCKER:=docker}
: ${DOCKER_IMAGE:=sysprog2024/debian:latest}
# allow gdb to turn off ASLR
: ${DOCKER_ARGS:=-it --cap-add=SYS_PTRACE --security-opt=seccomp=unconfined}
: ${PWD:=$(pwd)}

here=$(cd "$(dirname "$0")" && pwd)
[ -z "$here" ] || here="$here/"

if [ -d /var/empty ]; then
	context=/var/empty
else
	context="${here}."
fi

PS4='$ '
set -eux
${DOCKER} build -t "${DOCKER_IMAGE}" -f "${here}Dockerfile" "$context"
${DOCKER} run ${DOCKER_ARGS} -v ".:/host$PWD" -w "/host$PWD" "${DOCKER_IMAGE}" "$@"
