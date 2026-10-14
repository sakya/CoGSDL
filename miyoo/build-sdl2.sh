#!/bin/sh
# Prepares SDL2 headers and libraries to build CoGSDL for Miyoo Mini (Onion),
# using steward-fu's port (https://github.com/steward-fu/sdl2).
# Run it inside the union-miyoomini-toolchain container.
#
# Usage (inside the container, from the project folder):
#   apt-get update && apt-get install -y autoconf automake libtool pkg-config
#   sh miyoo/build-sdl2.sh
#   make re PLATFORM=miyoo
#
# Output (next to this script):
#   miyoo/include/SDL2  SDL2, SDL2_image, SDL2_ttf and SDL2_mixer headers
#   miyoo/lib           libraries used for linking, to be copied to
#                       App/CoGSDL/lib on the Miyoo
#
# Optional variables:
#   BUILD  temporary work folder (default: $HOME/sdl2-build)
#   SRC    clone of steward-fu/sdl2 (default: $BUILD/sdl2-src)
#   RES    resolution of the prebuilt libSDL2: 640x480 or 320x240
#   JOBS   parallel build jobs (default: 4)
set -e

: "${CROSS_COMPILE:?CROSS_COMPILE is not set: run this script inside the toolchain container}"

MIYOO=$(cd "$(dirname "$0")" && pwd)
BUILD=${BUILD:-$HOME/sdl2-build}
SRC=${SRC:-$BUILD/sdl2-src}
PREFIX=$BUILD/install
RES=${RES:-640x480}
HOST=arm-linux-gnueabihf
JOBS=${JOBS:-4}

export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export AR=${CROSS_COMPILE}ar
export RANLIB=${CROSS_COMPILE}ranlib
export STRIP=${CROSS_COMPILE}strip
export CFLAGS="-O2 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
# pkg-config must only see the libraries built here
export PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig
export PKG_CONFIG_PATH=
export PATH=$PREFIX/bin:$PATH

mkdir -p "$BUILD"
if [ ! -d "$SRC" ]; then
  git clone --depth 1 -c core.autocrlf=false https://github.com/steward-fu/sdl2 "$SRC"
fi
DEP=$SRC/sdl2/dependency
rm -rf "$PREFIX"

# 1. "Generic" SDL2 build: only needed for the headers, sdl2-config and to
#    build the other libraries. On the Miyoo the prebuilt libSDL2 from
#    steward-fu is used (same version, same ABI) with the MI_GFX/MI_AO drivers.
cd "$SRC/sdl2"
[ -x configure ] || ./autogen.sh
rm -rf "$BUILD/sdl2" && mkdir -p "$BUILD/sdl2" && cd "$BUILD/sdl2"
"$SRC/sdl2/configure" --host=$HOST --prefix="$PREFIX" \
  --disable-static \
  --disable-video-x11 --disable-video-wayland --disable-video-kmsdrm \
  --disable-video-vulkan --disable-video-opengl --disable-video-opengles \
  --disable-video-opengles2 --disable-video-mini --disable-audio-mini \
  --disable-pulseaudio --disable-pipewire --disable-alsa --disable-oss \
  --disable-jack --disable-sndio --disable-esd --disable-arts --disable-nas \
  --disable-dbus --disable-ime --disable-ibus --disable-fcitx \
  --disable-libudev --disable-hidapi --disable-joystick-virtual
make -j$JOBS
make install

build_dep() {
  name=$1; shift
  rm -rf "$BUILD/$name"
  tar xzf "$DEP/$name.tar.gz" -C "$BUILD"
  cd "$BUILD/$name"
  ./configure --host=$HOST --prefix="$PREFIX" --disable-static "$@"
  make -j$JOBS
  make install
}

# 2. SDL2_image: PNG/JPG with built-in stb_image, no dependencies
build_dep SDL2_image-2.8.1 \
  --enable-stb-image --disable-avif --disable-jxl --disable-tif --disable-webp

# 3. SDL2_ttf with built-in FreeType: no libfreetype/libpng/libbz2 needed
build_dep SDL2_ttf-2.20.2 --enable-freetype-builtin --disable-harfbuzz

# 4. SDL2_mixer: WAV, OGG (stb_vorbis), MP3 (minimp3), FLAC (dr_flac)
build_dep SDL2_mixer-2.6.3 \
  --disable-music-mod --disable-music-midi-fluidsynth --disable-music-opus \
  --disable-music-ogg-tremor --disable-music-flac-libflac \
  --disable-music-mp3-mpg123

# 5. json-c: required by the prebuilt libSDL2
rm -rf "$BUILD/json-c-0.15" "$BUILD/json-c-build"
tar xzf "$DEP/json-c-0.15.tar.gz" -C "$BUILD"
mkdir -p "$BUILD/json-c-build" && cd "$BUILD/json-c-build"
cmake "$BUILD/json-c-0.15" \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_STATIC_LIBS=OFF \
  -DBUILD_TESTING=OFF -DDISABLE_WERROR=ON
make -j$JOBS
make install

# 6. Copy into the project: headers to miyoo/include/SDL2, libraries to
#    miyoo/lib (real files, no symlinks, named as loaded at runtime)
rm -rf "$MIYOO/include/SDL2"
mkdir -p "$MIYOO/include" "$MIYOO/lib"
cp -r "$PREFIX/include/SDL2" "$MIYOO/include/SDL2"
rm -f "$MIYOO"/include/SDL2/SDL_test*.h

L=$MIYOO/lib
cp "$SRC/prebuilt/$RES/libSDL2-2.0.so.0" "$L/"
cp "$SRC/prebuilt/$RES/libEGL.so" "$SRC/prebuilt/$RES/libGLESv2.so" "$L/"
for f in libSDL2_image-2.0.so.0 libSDL2_ttf-2.0.so.0 libSDL2_mixer-2.0.so.0 libjson-c.so.5; do
  cp -L "$PREFIX/lib/$f" "$L/$f"
  $STRIP "$L/$f"
done

echo
echo "Done."
echo "  Headers:   $MIYOO/include/SDL2"
echo "  Libraries: $L"
ls -l "$L"
echo
echo "Now build with: make re PLATFORM=miyoo"
