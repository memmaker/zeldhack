# ZeldHack for the web

Upstream: **NetHack 3.6.7**, [NetHack/NetHack @ ed600d9](https://github.com/NetHack/NetHack/tree/ed600d9f0f3c37677418f0150f59363ca641f3dc) (tag `NetHack-3.6.7_Released`), imported unchanged as commit 1 (`d4d2545`).
All our changes against it: https://github.com/memmaker/zeldhack/compare/d4d2545fcee7d935912b6f2e2fb34556ad58c295...main

Play: https://ruzzoli.de/roguelikes/zeldhack/

## What ZeldHack adds
ZeldHack is vanilla NetHack 3.6.7 with a Zelda-style tile set (16/32/64 px), message
sounds, an ambience music loop and a tuned options file: the
[ZeldHack asset pack by LSpixel](https://lspixel.itch.io/zeldhack), in `zeldhack/`.
This repo ports it to the browser (Emscripten): a web window port
(`win/web/winweb.c`), auto-explore (`~`) and stair walking, an Enter command menu,
inventory item menus, sound hooks at the game's action sites, autosave, and the page in `web/`.

## Build
Needs emsdk (`. emsdk_env.sh`) and python3 with Pillow.
- `sh web/build.sh` -> `web/dist/` (runs `web/mkdata.sh` first: builds makedefs,
  dgn_comp, lev_comp, dlb and tilemap with emcc and runs them under node in the
  ignored copy `web/b32/`).
- Shared page code (`rvip-wm.js`, `rvip-app.js`, `rvip-sound.js`) comes from the
  RVIP tools (`RVIP_WEB`), served from `../` on the site.
- `web/deploy.sh` publishes `dist/` (maintainer only).

## Licence
- NetHack: NetHack General Public License (`dat/license`), as are our changes.
- ZeldHack assets (`zeldhack/`): by LSpixel; **licence not stated** on the itch.io
  page, and its sounds are "sourced from classic NES/Famicom games". Not covered by
  the NGPL; ask the author before reusing them.

The original NetHack README is `README`.
