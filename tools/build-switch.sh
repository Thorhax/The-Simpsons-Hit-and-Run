#!/bin/sh
# Builds the Switch NRO in the devkitpro/devkita64 docker image.
#
# Usage: tools/build-switch.sh [NTSC|PAL] [extra cmake options...]
#
# The output is build-<REGION>/Simpsons<REGION>.nro (set BUILD_DIR to use another folder). If tools/switch-mesa/build-mesa.sh has been run, the
# NRO is linked against that Mesa (with glthread support) instead of the one in devkitPro's portlibs.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
REGION=${1:-NTSC}
[ $# -gt 0 ] && shift
BUILD=${BUILD_DIR:-build-$REGION}

MESA_MOUNTS=""
if [ -f "$ROOT/build-mesa/lib/libEGL.a" ] && [ -f "$ROOT/build-mesa/lib/libglapi.a" ]; then
    echo "Using the Mesa build with glthread support from build-mesa/lib"
    LIB=/opt/devkitpro/portlibs/switch/lib
    MESA_MOUNTS="-v $ROOT/build-mesa/lib/libEGL.a:$LIB/libEGL.a:ro -v $ROOT/build-mesa/lib/libglapi.a:$LIB/libglapi.a:ro"
fi

docker run --rm -v "$ROOT:/src" -w /src $MESA_MOUNTS devkitpro/devkita64:latest bash -ec "
    /opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-cmake -S . -B $BUILD \
        -DCMAKE_BUILD_TYPE=Release -DSRR2_REGION=$REGION $*
    # The Mesa libraries live outside the build tree, so always relink
    rm -f $BUILD/code/SRR2.elf
    cmake --build $BUILD -j\$(nproc)
"
echo "Built $BUILD/Simpsons$REGION.nro"
