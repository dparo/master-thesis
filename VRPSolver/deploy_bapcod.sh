#!/usr/bin/env bash
# -*- coding: utf-8 -*-

cd "$(dirname "$0")" || exit 1

if [ -z "$CPLEX_ROOT" ]; then
	echo >&2 "ERROR make sure that the environment variable $(CPLEX_ROOT) exists"
	echo >&2 "    Make sure to point CPLEX_ROOT to the IBM ILOG CPLEX Optimization studio"
	echo >&2 '    EXAMPLE: export CPLEX_ROOT="/opt/ibm/ILOG/CPLEX_Studio1210"'
	exit 1
fi

if [ -z "$CPLEX_BINARIES" ]; then
	export CPLEX_BINARIES="$CPLEX_ROOT/cplex/bin/x86-64_linux/"
fi

export CPLEX_STUDIO_BINARIES="$CPLEX_BINARIES"

echo "Using CPLEX_ROOT=$CPLEX_ROOT"
echo "Using LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

# Install dependencies if not already installed
for dep in make cmake g++ zlib1g-dev; do
	dpkg -l "$dep" 1>/dev/null || sudo apt-get install --yes "$dep"
done

bapcode_tar="$PWD/archives/bapcod-v0.66.tar.gz"
librcsp_tar="$PWD/archives/librcsp-0.5.11-Linux.tar.gz"

if [ ! -f "$bapcode_tar" ]; then
	echo >&2 "ERROR: make sure that ${bapcode_tar} file exists"
	exit 1
fi

if [ ! -f "$librcsp_tar" ]; then
	echo >&2 "ERROR: make sure that ${librcsp_tar} file exists"
	exit 1
fi

set -x

if [ ! -d "BapcodFramework" ]; then
	rm -rf BapcodFramework
	mkdir -p BapcodFramework
	tar -xf "$bapcode_tar" -C BapcodFramework
fi

if [ ! -f "archives/boost_1_76_0.tar.gz" ]; then
	wget -P ./archives https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz
fi

if [ ! -f "archives/lemon-1.3.1.tar.gz" ]; then
	wget -P ./archives http://lemon.cs.elte.hu/pub/sources/lemon-1.3.1.tar.gz
fi

cp -rf "archives/boost_1_76_0.tar.gz" BapcodFramework/Tools
cp -rf "archives/lemon-1.3.1.tar.gz" BapcodFramework/Tools

pushd BapcodFramework || exit 1
{
	bash Scripts/shell/install_bc_lemon.sh
	bash Scripts/shell/install_bc_boost.sh

	mkdir Tools/rcsp
	tar -xf "$librcsp_tar" -C Tools/rcsp

	for build_type in Debug Release; do
		if [ ! -d "build/${build_type}" ]; then
			mkdir -p "build/${build_type}"
			pushd "build/${build_type}" || exit 1
			cmake --config "$build_type" "-DCMAKE_BUILD_TYPE=${build_type}" -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ../../
			popd || exit 1
		fi
		pushd "build/${build_type}" || exit 1
		cmake --build ./ --config "${build_type}" -j 8 --target all
		popd || exit 1
	done

}
popd || exit 1 # BapcodFramework

# Download Julia language

# NOTE: https://github.com/inria-UFF/BaPCodVRPSolver.jl
# Julia versions 1.6 and later are not supported for the moment due to a JuMP issue (https://github.com/jump-dev/JuMP.jl/issues/2438).
# Support of Julia 1.6 requires a significant work and will depend on the number of inquiries for it.
# Please let the contributors of this package know if this support is critical for you.

julia_version=1.5.4
if [ ! -f "archives/julia-${julia_version}-linux-x86_64.tar.gz" ]; then
	wget -P archives "https://julialang-s3.julialang.org/bin/linux/x64/1.5/julia-${julia_version}-linux-x86_64.tar.gz"
fi

if [ ! -d "$HOME/julia-${julia_version}/bin/julia" ]; then
	tar zxf "archives/julia-${julia_version}-linux-x86_64.tar.gz" -C "$HOME"
fi

ln -sf "$HOME/julia-${julia_version}/bin/julia" ~/.local/bin/julia
rm -rf "$HOME/.julia/compiled"

julia <<END
    import Pkg
    ENV["CPLEX_STUDIO_BINARIES"] = "$CPLEX_BINARIES"
    Pkg.add("Debugger")
    Pkg.add("CPLEX")
    Pkg.build("CPLEX")
    Pkg.add("JuMP")
    Pkg.add("ArgParse")
    Pkg.add(url="https://github.com/inria-UFF/BaPCodVRPSolver.jl.git")
    Pkg.precompile()
END
