#!/bin/sh
# Generate everything that depends on the target's word size with wasm32 tools
# (ZeldHack runs as wasm32, 4-byte long): include/date.h, onames.h, pm.h,
# vis_tab, src/tile.c, dat/nhdat (dungeon, *.lev and quest.dat are raw struct
# dumps).  Works in an ignored copy of the tree, web/b32, so the repo's own
# native build stays untouched.  Needs emcc and node.
set -e
cd "$(dirname "$0")/.."
B=web/b32
rm -rf "$B" && mkdir -p "$B/src" "$B/tools"
git ls-files -z include src util dat win/share sys/share sys/unix/sysconf | xargs -0 cp --parents -t "$B"
cp sys/share/dgn_lex.c sys/share/dgn_yacc.c sys/share/lev_lex.c sys/share/lev_yacc.c "$B/util/"
cp sys/share/dgn_comp.h sys/share/lev_comp.h "$B/include/"
cp sys/unix/sysconf "$B/dat/" 2>/dev/null || true
cp dat/Makefile "$B/dat/Makefile"   # made by sys/unix/setup.sh
# wasm traps on the 2-arg vs 3-arg fopen_datafile the native tools get away with
sed -i 's/fopen_datafile, (const char \*, const char \*))/fopen_datafile, (const char *, const char *, int))/; s/^const char \*filename, \*mode;/const char *filename, *mode;\nint prefix UNUSED;/; s/^fopen_datafile(filename, mode)/fopen_datafile(filename, mode, prefix)/' "$B/util/dlb_main.c"
cd "$B"
CF="-O1 -w -Iinclude -DDLB -DSYSCF -DSECURE -DTEXT_TOMBSTONE"
LF="-sNODERAWFS -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1048576"
tool() { out=$1; shift; emcc $CF $LF "$@" -o "util/$out.js"
	printf '#!/bin/sh\nexec node "%s" "$@"\n' "$PWD/util/$out.js" > "util/$out"; chmod +x "util/$out"; }
tool makedefs util/makedefs.c src/monst.c src/objects.c
(cd util && ./makedefs -v && ./makedefs -o && ./makedefs -p && ./makedefs -z)
tool dgn_comp util/dgn_yacc.c util/dgn_lex.c util/dgn_main.c src/alloc.c util/panic.c
tool lev_comp util/lev_yacc.c util/lev_lex.c util/lev_main.c src/alloc.c util/panic.c \
	src/drawing.c src/decl.c src/monst.c src/objects.c
tool dlb util/dlb_main.c src/dlb.c src/alloc.c util/panic.c
# ZeldHack sheets hold the grayscale statue tiles (slots 1082..): as the Windows build
tool tilemap -DSTATUES_LOOK_LIKE_MONSTERS win/share/tilemap.c
(cd util && ./tilemap)
# the dat Makefile calls ../util/{makedefs,lev_comp,dgn_comp,dlb}
(cd dat && make -f Makefile all >/dev/null \
 && LC_ALL=C ../util/dlb cf nhdat help hh cmdhelp keyhelp history opthelp wizhelp dungeon tribute \
	asmodeus.lev baalz.lev bigrm-*.lev castle.lev fakewiz?.lev juiblex.lev knox.lev medusa-?.lev \
	minend-?.lev minefill.lev minetn-?.lev oracle.lev orcus.lev sanctum.lev soko?-?.lev tower?.lev \
	valley.lev wizard?.lev astral.lev air.lev earth.lev fire.lev water.lev \
	???-goal.lev ???-fil?.lev ???-loca.lev ???-strt.lev data oracles options quest.dat rumors bogusmon engrave epitaph) || {
	echo "dat build failed; try: make -C $B/dat"; exit 1; }
ls -la dat/nhdat src/tile.c include/date.h
