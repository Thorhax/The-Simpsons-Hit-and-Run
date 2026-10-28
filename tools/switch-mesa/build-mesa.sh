#!/bin/sh
# Rebuilds devkitPro's switch-mesa 20.1.0 with glthread support (see glthread.patch) and faster texture
# uploads (see nouveau-texture-uploads.patch) inside the devkitpro/devkita64 docker image. The libraries end
# up in build-mesa/lib at the root of the repository, where tools/build-switch.sh picks them up.
#
# Usage: tools/switch-mesa/build-mesa.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$ROOT/build-mesa
mkdir -p "$OUT"
cp "$ROOT/tools/switch-mesa/glthread.patch" "$ROOT/tools/switch-mesa/nouveau-texture-uploads.patch" "$OUT/"

docker run --rm -v "$OUT:/work" -w /work devkitpro/devkita64:latest bash -euc '
MESA=mesa-20.1.0-rc3
PKG=https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa
apt-get update -qq >/dev/null
apt-get install -y -qq meson python3-mako bison flex pkg-config curl >/dev/null

[ -f $MESA.tar.xz ] || curl -sfL -o $MESA.tar.xz https://archive.mesa3d.org/older-versions/20.x/$MESA.tar.xz
for f in switch-mesa-20.1.0-5.patch gl_XML.py.patch glX_XML.py.patch; do
    [ -f $f ] || curl -sfL -o $f $PKG/$f
done
# Checksums from devkitPro'"'"'s PKGBUILD for switch-mesa 20.1.0-5
sha256sum -c - <<SUMS
c90b75ea34302ebde9b81b87c5642fa864c40fe9c4ad34ce0793170c1413168d  $MESA.tar.xz
950f93d3e5b6ae9c5a42c2918623fe9a80d7ee398d92d2e73abec86b62d75916  switch-mesa-20.1.0-5.patch
a9bc326195b3fe29709e079466a8b2162a2ac9409f694eaad5494490155e2dd4  gl_XML.py.patch
1475defcdf8600690eaddeee28d8f01310635c1273fb541f668e8789714d36ca  glX_XML.py.patch
SUMS

rm -rf src && mkdir src && tar xf $MESA.tar.xz -C src
cd src/$MESA
patch -s -p1 -i /work/switch-mesa-20.1.0-5.patch
patch -s -p1 -i /work/gl_XML.py.patch
patch -s -p1 -i /work/glX_XML.py.patch
patch -s -p1 -i /work/glthread.patch
patch -s -p1 -i /work/nouveau-texture-uploads.patch

export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/portlibs/switch/bin:$PATH
/opt/devkitpro/meson-toolchain.sh switch > ../crossfile.txt
# Current newlib has timespec_get, which Mesa 20.1 would otherwise define itself
sed -i "s/^c_args = \[/c_args = [\x27-DHAVE_TIMESPEC_GET\x27,/; s/^cpp_args = \[/cpp_args = [\x27-DHAVE_TIMESPEC_GET\x27,/" ../crossfile.txt
meson setup --buildtype=plain --cross-file=../crossfile.txt --default-library=static \
    --prefix=/opt/devkitpro/portlibs/switch --libdir=lib build -Db_ndebug=true >/dev/null
ninja -C build
mkdir -p /work/lib
cp build/src/egl/libEGL.a build/src/mapi/shared-glapi/libglapi.a /work/lib/
'
echo "Built $OUT/lib/libEGL.a and $OUT/lib/libglapi.a"
