#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit 1

argc=$#
argv=("$@")

DEFAULT_TIMELIMIT=600.0
timelimit="$DEFAULT_TIMELIMIT"

timelimit_found=0

for ((j = 0; j < argc; j++)); do
	v="${argv[j]}"
	if [ "$v" == "--timelimit" ]; then
		timelimit="${argv[j + 1]}"
		timelimit_found=1
	fi
done

if [ -z "$timelimit" ]; then
	timelimit="$DEFAULT_TIMELIMIT"
fi

if [ -x "./build/Release/src/cptp" ]; then
	CPTP_EXECUTABLE="./build/Release/src/cptp"
elif [ -x "./build/Debug/src/cptp" ]; then
	CPTP_EXECUTABLE="./build/Debug/src/cptp"
fi

# 5% more of timeout before sending SIGINT signal
timeout_timelimit="$(echo "$timelimit * 1.05" | bc)"

if [ "$timelimit_found" -eq 1 ]; then
	set -x
	exec timeout -k 10s "${timeout_timelimit}s" "${CPTP_EXECUTABLE}" "$@"
else
	set -x
	exec timeout -k 10s "${timeout_timelimit}s" "${CPTP_EXECUTABLE}" --timelimit "$timelimit" "$@"
fi
