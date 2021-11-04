#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit 1

set -x

docker save -o vrpsolver-dump.tar bapdock
rm -rf "$PWD/vrpsolver-dump"
mkdir  -p "$PWD/vrpsolver-dump" && pushd "$PWD/vrpsolver-dump" || exit 1
tar -xf ../vrpsolver-dump.tar
pushd 9b89c96fbb8dfbf0219c0f6e9fdd732859af7fc5f0e8a3ddaea4763ef5d357cc || exit 1
tar -xf layer.tar
cp -rf ./root/.julia "$HOME/.julia"
popd || exit 1
popd || exit 1

rm -f "$PWD/vrpsolver-dump.tar"
rm -rf "$PWD/vrpsolver-dump"

chown --recursive "$USER:$USER" "$HOME/.julia"

CPLEX_DYN_LIB="/opt/ibm/ILOG/CPLEX_Studio1210/cplex/bin/x86-64_linux/libcplex12100.so"

DEPS_FILE="$HOME/.julia/packages/CPLEX/Hxpuk/deps/deps.jl"
if [ -f "$DEPS_FILE" ]; then
	echo "const libcplex = \"${CPLEX_DYN_LIB}\"" >"$DEPS_FILE"
fi

PWD="$(pwd)"
export LD_LIBRARY_PATH="${PWD}/bin"

mkdir -p bin
ln -s "$CPLEX_DYN_LIB" bin/libcplex12x.so

rm -rf "$HOME/.julia/compiled"

julia <<END
    import Pkg
    Pkg.precompile()
    Pkg.add("Debugger")
END
