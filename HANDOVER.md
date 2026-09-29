# ZeldHack — handover

## RVIP progress
- Stage 0 (prep, Mac): repo created from NetHack 3.6.7 + ZeldHack assets
  (`zeldhack/`), brief in `CLOUD.md`.
- **Stage 1 (get + build) done in the cloud.** **Next: stage 2** (explore + stairs).
  - Base: `NetHack/NetHack` tag `NetHack-3.6.7_Released` (commit 1). Case O.
  - Window port: `win/web/winweb.c` (new, C89 style, 3.6 API), registered in
    `src/windows.c` under `WEB_GRAPHICS`; `src/end.c` calls `be_run_end()` (beacon,
    game id `zeldhack`) before `topten()`. Derived from nethack50's winsdl.c/winweb.h;
    same JS interface (`Module.nh.map/text/key/end`).
  - Page: `web/index.html`, `web/zeldhack.js` (from nethack50's nethack.js; tile
    size = sheet width / 40, so 16/32/64 sheets all work). `../rvip-*.js` shared.
  - Build: `sh web/build.sh` -> `web/dist` (runs `web/mkdata.sh` first if
    `web/b32/dat/nhdat` is missing). Needs emsdk (`/home/user/emsdk`,
    `. emsdk_env.sh`), flex not needed (pre-generated lexers from `sys/share`).
    Flags in the script: `-DWEB_GRAPHICS -DNOTTYGRAPHICS -DDLB -DSYSCF -DSECURE
    -DTEXT_TOMBSTONE`, Asyncify, HACKDIR `/nethack` on IDBFS. **No `-DNOMAIL`**.
  - `web/mkdata.sh` builds makedefs, dgn_comp, lev_comp, dlb, tilemap with emcc and
    runs them under node in the ignored copy `web/b32` (date.h, onames.h, pm.h,
    vis_tab, `src/tile.c`, `nhdat`). The native 64-bit tools write dungeon/.lev/
    quest.dat with 8-byte longs: the game then says "Dungeon description not valid".
  - Tiles: `zeldhack/tiles/zeldhack_32.bmp` (lower case!) + `ZeldHack_16/64.bmp`
    converted to `tiles*.png` by build.sh; `glyph2tile[]` from tilemap (1082 tiles;
    sheet has 1480 slots at 32px). Coverage measurement is stage 4.
  - Tested headless (Playwright, chromium): title -> name -> role/race/align menus ->
    intro -> map with ZeldHack tiles, inventory pane with icons, status, messages.
    ASan: native tty build (ASan, copy of tree) driven through a pty: start, walk,
    inventory, search, save, exit 0, no reports. No `signature mismatch` or
    `conflicting signatures` in the link output.
  - Not done here: visual look in the browser pane (CLOUD.md: Mac session).
  - Quirks: default map cell is 12 px until the WM layout (stage 5) gives the map a
    bigger window; player selection is my own menu sequence (web_player_selection),
    `Who are you?` first; `\G` gold escapes in status are decoded with `decode_mixed`.
  - Not yet baked in: `zeldhack/nethackrc` options, sounds (`SOUND=MESG` must become
    action hooks, rule 8), music, explore/stairs/Enter menu, autosave (SELF_RECOVER).
    Untested: save + reload, `S` in the browser, IDBFS round trip.
