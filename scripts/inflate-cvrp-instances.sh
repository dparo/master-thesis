#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit 1

echoerr() { echo "$@" 1>&2; }


instance_converter_debug="./build/Debug/src/tools/cvrp-instance-modifier"
instance_converter_release="./build/Release/src/tools/cvrp-instance-modifier"

instance_converter="$instance_converter_debug"

if [ -f "$instance_converter_release" ]; then
    instance_converter="$instance_converter_release"
fi

if [ ! -f "$instance_converter" ]; then
    echoerr "Instance converter file does not exist. Make sure to build the project first"
    exit 1
fi


for family in "A" "B" "E" "F" "M" "P" "X"; do
    find "./data/CVRP/$family" -type f -print0 |
        while IFS= read -r -d '' line; do
            for amt in "1.0" "2.0" "3.0" "4.0" "5.0" "10.0" "20.0" "100.0"; do
                out_dir="./data/CVRP-scaled-${amt}/${family}"
                mkdir -p "$out_dir"
                in="$line"
                out="$out_dir/$(basename "$in")"

                set -x
                "$instance_converter" -i "$in" -o "$out" -f "$amt"
                { set +x; } 2>/dev/null
            done
        done
done
