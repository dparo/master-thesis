#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit

set -x

INPUT_IMG="bapdock.img.backup"
OUTPUT_IMG="bapdock.img"

if [ ! -f "${INPUT_IMG}" ]; then
	cp "${OUTPUT_IMG}" "${INPUT_IMG}"
fi

docker import -c 'ENTRYPOINT ["/julia"]' bapdock.img.backup bapdock-init
docker run -it -d --name bapdock-cont bapdock-init
docker cp /opt/ibm/ILOG/CPLEX_Studio1210/cplex bapdock-cont:/cplex
docker cp /opt/ibm/ILOG/CPLEX_Studio1210/cplex/bin/x86-64_linux/libcplex12100.so bapdock-cont:/cplex/bin/x86-64_linux/libcplex12x.so
container_id="$(docker ps -aqf "name=bapdock")"
docker commit "$container_id" "${OUTPUT_IMG}"
docker stop "${container_id}"
docker rm "${container_id}"
