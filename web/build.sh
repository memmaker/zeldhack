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
if [ ! -f web/b32/dat/nhdat ] || [ -n "$(git ls-files include src dat win/share web/mkdata.sh | xargs sh -c 'find "$@" -newer web/b32/dat/nhdat' sh | head -1)" ]; then
    rm -rf web/b32 && sh web/mkdata.sh
fi
B=web/b32
rm -rf "$OUT" "$SEED" && mkdir -p "$OUT" "$SEED"
cp $B/dat/nhdat $B/dat/symbols $B/dat/license "$SEED/"
# the player's ZeldHack options as web defaults (RVIP stage 5): minus the
# Windows-only window/font/tile options, SAVEDIR and the SOUND=MESG table
# (sounds come from game-action hooks)
# (colour is on by default in the Windows build the options come from)
{ echo 'OPTIONS=color'
  grep -Eiv '^(SOUND|SAVEDIR)|map_mode|tile_(file|width|height)|font_|windowcolors|vary_msgcount' \
	zeldhack/nethackrc | tr -d '\r'; } > "$SEED/nethackrc"
grep -Ev '^(GREPPATH|GDBPATH|PANICTRACE|MAXPLAYERS|DUMPLOGFILE)' sys/unix/sysconf > "$SEED/sysconf"
SRCS=$(ls $B/src/*.c)
emcc -O2 $EMFLAGS -w -I$B/include -Iinclude \
	-DWEB_GRAPHICS -DNOTTYGRAPHICS -DDEFAULT_WINDOW_SYS='"web"' -DDLB -DHACKDIR='"/zeldhack"' \
	-DSYSCF -DSYSCF_FILE='"/zeldhack/sysconf"' -DSECURE -DGNU_LIBC -DTIMED_DELAY -DTEXT_TOMBSTONE -DSELF_RECOVER \
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
mkdir "$OUT/music" && cp zeldhack/music/ambience.mp3 "$OUT/music/"
# text fonts: the index page's fonts/ (served at ../fonts/ next to the games)
ROGUELIKES=${ROGUELIKES:-$HOME/Games/roguelikes-index}
[ -d "$ROGUELIKES" ] || ROGUELIKES=/home/user/roguelikes
if [ -d "$ROGUELIKES/fonts" ]; then (cd "$ROGUELIKES/fonts" && ls *.woff | sed 's/\.woff$//'); fi |
	python3 -c 'import json,sys; print(json.dumps(sys.stdin.read().split()))' > "$OUT/fonts.json"
