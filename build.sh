set -e

DEPS=enji-deps-0.0.0
DEPS_ARCH=$DEPS.zip

if [ ! -d deps ]; then
	if [ ! -d $DEPS_ARCH ]; then
	    echo "Downloading dependencies: $DEPS_ARCH"
	    curl -q -o $DEPS_ARCH -L https://github.com/aptakhin/enji/releases/download/0.0.0/$DEPS_ARCH
	fi
    unzip -q $DEPS_ARCH
	mv $DEPS deps
fi

ENJI_DIR=`pwd`

cd deps/packages/libuv-1.8.0/
python gyp_uv.py -f make
make -C out

CUR=`pwd`
echo "C $CUR"
mkdir -p ../../lib/debug ../../lib/release
cp out/Debug/libuv.a ../../lib/debug
cp out/Release/libuv.a ../../lib/release

cd $ENJI_DIR
