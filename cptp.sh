#!/usr/bin/env bash
# Copyright (c) 2021 Davide Paro
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# -*- coding: utf-8 -*-

# CPTP solver wrapper script

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
timeout_timelimit="$(echo "$timelimit * 1.05 + 2.0" | bc)"

if [ "$timelimit_found" -eq 1 ]; then
	set -x
	exec timeout -k 5s "${timeout_timelimit}s" "${CPTP_EXECUTABLE}" "$@"
else
	set -x
	exec timeout -k 5s "${timeout_timelimit}s" "${CPTP_EXECUTABLE}" --timelimit "$timelimit" "$@"
fi
