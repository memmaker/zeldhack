#!/bin/sh
# Build ZeldHack (NetHack 3.6.7) for the browser (Emscripten + Asyncify) into
# web/dist.  win/web/winweb.c is the window port, web/zeldhack.js draws,
# rvip-wm.js (RVIP_WEB) places the windows.  Generated headers, tile.c and
# nhdat come from web/mkdata.sh (wasm32 tools; 64-bit native ones write
# incompatible dungeon/level files).
set -e
cd "$(dirname "$0")/.."
OUT=web/dist SEED=web/seed
# web/b32 is a copy of the tree: redo it when a tracked source is newer
if [ ! -f web/b32/dat/nhdat ] || [ -n "$(git ls-files include src dat win/share | xargs sh -c 'find "$@" -newer web/b32/dat/nhdat' sh | head -1)" ]; then
    rm -rf web/b32 && sh web/mkdata.sh
fi
B=web/b32
rm -rf "$OUT" "$SEED" && mkdir -p "$OUT" "$SEED"
cp $B/dat/nhdat $B/dat/symbols $B/dat/license "$SEED/"
grep -Ev '^(GREPPATH|GDBPATH|PANICTRACE|MAXPLAYERS|DUMPLOGFILE)' sys/unix/sysconf > "$SEED/sysconf"
SRCS=$(ls $B/src/*.c)
emcc -O2 $EMFLAGS -w -I$B/include -Iinclude \
	-DWEB_GRAPHICS -DNOTTYGRAPHICS -DDEFAULT_WINDOW_SYS='"web"' -DDLB -DHACKDIR='"/nethack"' \
	-DSYSCF -DSYSCF_FILE='"/nethack/sysconf"' -DSECURE -DGNU_LIBC -DTIMED_DELAY -DTEXT_TOMBSTONE \
	$SRCS sys/share/ioctl.c sys/share/posixregex.c sys/share/unixtty.c \
	sys/unix/unixmain.c sys/unix/unixres.c sys/unix/unixunix.c win/web/winweb.c \
	--preload-file "$SEED@/seed" -o "$OUT/zeldhack-core.js" \
	-sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=2097152 \
	-sALLOW_MEMORY_GROWTH -sEXIT_RUNTIME=1 -sINITIAL_MEMORY=64MB \
	-sEXPORTED_FUNCTIONS=_main \
	-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,HEAP32 \
	-sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web
rm -rf "$SEED"
ls -la "$OUT"
python3 -c "
from PIL import Image
for n in (16, 32, 64):
    Image.open('zeldhack/tiles/%s_%d.bmp' % ('zeldhack' if n == 32 else 'ZeldHack', n)).convert('RGB').save('$OUT/tiles%s.png' % ('' if n == 32 else n))"
cp web/index.html web/zeldhack.js "$OUT/"
echo '[]' > "$OUT/fonts.json"
