#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit 1

set -x

# Cd into latest VRP directory
cd "$(find ./ -mindepth 1 -maxdepth 1 -type d | grep -E 'vrpsolver.*' | sort -n | head -n 1)" || exit 1

INPUT_IMG="bapdock.img.backup"
OUTPUT_IMG="bapdock"

if [ ! -f "${INPUT_IMG}" ]; then
	cp bapdock.img "${INPUT_IMG}"
fi

docker import -c 'ENTRYPOINT ["/julia"]' bapdock.img.backup bapdock-init
docker run -it -d --name bapdock-cont bapdock-init
container_id="$(docker ps -aqf "name=bapdock-cont")"
docker cp /opt/ibm/ILOG/CPLEX_Studio1210/cplex bapdock-cont:/cplex
docker cp /opt/ibm/ILOG/CPLEX_Studio1210/cplex/bin/x86-64_linux/libcplex12100.so bapdock-cont:/cplex/bin/x86-64_linux/libcplex12x.so
docker commit "$container_id" "${OUTPUT_IMG}"
docker stop "${container_id}"
docker rm "${container_id}"
