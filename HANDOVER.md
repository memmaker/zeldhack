# ZeldHack — handover

## RVIP progress
- Stage 0 (prep, Mac): repo created from NetHack 3.6.7 + ZeldHack assets
  (`zeldhack/`), brief in `CLOUD.md`.
- Stage 1 (get + build) done in the cloud.
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
- Stage 2 (explore + stairs + no --More--) done in the cloud.
  - `~` = `autoexplore` (extcmd in src/cmd.c; `~` unused in 3.6.7 with number_pad
    off or on). Code at the end of `src/hack.c` (`rvip_*`), hook in `moveloop`
    (`if (!rvip_continue()) rhack(0)`), `<`/`>` in `src/do.c` doup/dodown: off the
    right stairs they walk to the nearest known one (stairs, ladders, branch
    stairs) and stop there; second press takes them.
  - Stops: key (winweb `web_get_nh_event` peeks `js_key(1)`, sets `rvip_keyhit`,
    paints each step with a 40 ms sleep), any new message (`rvip_msgs`, counted in
    winweb `web_putstr` NHW_MESSAGE; "The door opens." is the one exception), a
    hostile in view for explore ("In view: the jackal."), a step that did not move.
    BFS skips known traps, water/lava, remembered boulders, doors that proved
    locked (marked per level); explore paths around all non-tame monsters, stair
    walks only around hostiles (may flee past). Blocked: "Known traps, boulders or
    locked doors block the only way on."
  - No --More--: winweb has no --More-- at all (3.6.7 has no auto_more option);
    messages go to the log. Birth tested: no -more-; the legacy intro is a text
    pop-up closed with Enter (not a --More--).
  - Help: dat/hh, dat/help, dat/cmdhelp. Hint bar already showed `~ explore`.
  - build.sh now redoes web/b32 (copy of the tree it compiles!) when any tracked
    include/src/dat/win/share file is newer than nhdat.
  - Tested (Playwright): explore ran ~90 steps opening a door, stopped on pet in the
    way / monster in view; `<` from off-stairs walked back to the up stairs, second
    `<` asked "Still climb?"; no -more- in the log. Not tested: key interrupt
    mid-walk in the browser, locked door marking, `>` to a known down staircase
    (walk code is shared with `<`).
  - Open: pet swaps/pickups stop explore (every message stops); the dog "in the
    way" stops it often. Visual check of step painting: todo on Mac.
- **Stage 3 (Enter menu + inventory) done in the cloud.**
  - Enter = extcmd `commandmenu` bound to `'\r'` (JS sends Return as 13; Ctrl+J
    stays 10 = vi run south). `docmdmenu()` in src/cmd.c: groups as `dokeylist()`
    (General / Game / Wizard-mode), key by reverse lookup in `Cmd.commands[]`
    (follows number_pad), unbound commands as `#name`; the key is the row's group
    accelerator; shell/suspend left out. No movement rows (3.6 keeps moves out of
    `Cmd.commands`). Enter has no other meaning at the command prompt.
  - Inventory: `ddoinv`/`doprinuse` (`i`, `*`) -> `rvip_invlist()` at the end of the
    RVIP block in src/invent.c: tabs Inventory / Equipment / Floor (4/6 or arrows),
    letter = main action (`rvip_fits()`: zap, wear/take off, put on/remove, apply,
    read, eat, quaff, wield, else examine), Shift+letter drop, Ctrl+letter examine,
    numpad + - *, Enter/Space/5/click = item menu (`rvip_actmenu`, keys from
    `cmd_from_func`, 4 back / 6 confirm, Esc back to the list). Other keys close the
    list and run as commands (winweb `pushed_key`). Examine = `rvip_lookup()` in
    pager.c (doname + data.base entry).
  - Actions run the real command with a preselect: `rvip_prelet` is taken by
    `getobj()` first; floor prompts skipped for it in `floorfood()` and `dodrink()`.
    List reopens after the action via `rvip_reopen_cmd()` in moveloop (not with a
    hostile in view, `multi`, or engulfed).
  - Item prompts: `force_invmenu` on (winweb init), so every `getobj()` shows the
    list with cursor; 4/6 switch likely / worn / all; letters, `-`, `*` as before.
    winweb: `rvip_listmode` 1/2/3 tells `web_select_menu` which keys apply; arrows
    are 8/2/4/6 in any pop-up; j/k move only without number_pad; a real
    accelerator always wins. WIN_INVEN picks (lets = all) now pop up.
  - Pop-ups: WM-sized (`RvipWM.popup`); rows without letters are not indented.
  - Tested (Playwright, number_pad 0 and 2): menu, run by cursor+5 and by key,
    tabs, eat by letter and `+`, drop by Shift+letter and `-`, examine by Ctrl+letter
    and `*`, item menus, wield/apply/drop prompts with cursor and tab switch, `0`.
  - Open: floor tab in item prompts not done (floor food still via the y/n); no
    "@-tags" in NetHack 3.6. Reopen suppression with a hostile in view untested.
    Once saw an unexplained "e - a pick-axe." pickup message after closing a
    pop-up (not reproduced in 5 reruns). Visual check in the pane: todo on Mac.
- Stage 4 (tiles) done in the cloud.
  - One set: ZeldHack (LS Pixel) 16/32/64 (`tiles16.png`, `tiles.png`, `tiles64.png`,
    40 per row, 1480 slots). Tiles button cycles ZeldHack 16 -> 32 -> 64 -> None;
    stored by name as `tiles` in `/nethack/web-layout.json` (IDBFS), read in the
    syncfs callback before the first sheet request (tested: after None + reload no
    sheet is fetched; 16 after reload fetches only tiles16.png). `loadSheet()` drops
    late onloads (generation counter + None check). Switch resets auto cell size.
    Auto cell = whole multiple of the sheet size when it fits, else the fitted size.
  - Coverage: 5991 glyphs -> 1407 distinct tiles (0..1474), `total_tiles_used` 1475
    <= 1480 slots; every used slot non-blank except 850 (dark part of a room,
    black on purpose); 394 monster tiles all distinct = 100 %; objects/features
    100 % (89 identical pairs, all same-appearance: scroll labels, gems by colour,
    bag/lamp/horn kinds, door orientations; none tells a kind apart).
  - Fixed: statues were the plain statue object (845); the sheets hold grayscale
    statue tiles at 1082.. -> mkdata.sh runs tilemap with
    `-DSTATUES_LOOK_LIKE_MONSTERS`. Fixed: USE_TILES was off, so `shuffle_tiles()`
    never ran and random appearances showed the default (revealing) tile ->
    `include/global.h` defines USE_TILES for WEB_GRAPHICS (also enables sokoban/knox
    `substitute_tiles`). Verified: ring of invisibility shows coral, etc.
  - Hero = role monster tile; inventory icons are 16 px background crops of the
    current sheet (`--ti` CSS var) for every size. Build redoes web/b32 when
    web/mkdata.sh changes too.
  - Open: map cell is 12 px in the default layout (80 cols don't fit; stage 5).
    Reload mid-game asks "Destroy old game?" and Enter = n ends it (lock file;
    stage 5/5.10 autosave). Visual check in the pane: todo on Mac.
- **Stage 5 (web page and windows) done in the cloud.** **Next: stage 6** (docs and sound).
  - Windows (rvip-wm): Map, Messages, Status, Inventory, Visible on by default;
    Equipment (worn/wielded, C `equip_list()`, id 8) via Windows ▾; no Recall.
    One window = only the map canvas showing the game's 80x24 screen (prompt row,
    map, 2 status rows drawn as text cells), scaled to fit in whole device pixels,
    no scroll, no A−/A+. Default cell (`autoCell()` in zeldhack.js): whole map if
    it fits, else the sheet size (32) while 12 rows fit; camera = `RvipWM.center`.
  - winweb.c sends: 7 = Visible (`visible_list()`: `m_at`+`canspotmon`, `l_monnam`,
    tame/peaceful; objects in `cansee` squares, `xname` of a copy; CSS colour
    table `css[]` = page PAL), 8 = Equipment, 1 = status as tab-separated segments
    `colour:HL_bits:text` (hilite_status + hitpointbar, WC2_HILITE_STATUS|HITPOINTBAR).
  - Options: build.sh writes `nethackrc` into the seed from zeldhack/nethackrc
    (drops SOUND*, SAVEDIR, map_mode/tile_*/font_*/windowcolors/vary_msgcount;
    prepends OPTIONS=color); page sets `NETHACKOPTIONS=@/zeldhack/nethackrc`.
    So number_pad:2 is the default (arrows/numpad move), pets Sirius/Blacky/Jump.
  - Saves: IDBFS folder is now **/zeldhack** (HACKDIR/SYSCF_FILE too; /nethack
    collided with nethack50 on the shared origin). Autosave: winweb getkey,
    idle 1 s at the command prompt (`iflags.in_parse`, no popup) after a key ->
    `save_currentstate()` (INSURANCE); JS syncs every 2 s while idle, 15 s,
    pagehide/hidden. `-DSELF_RECOVER`: unixunix.c getlock (web) turns the lock
    files into a save (`recover_savefile`) and restores silently -> no "Destroy
    old game?" (reset `lock`/`fq_lock` after it!). No COMPRESS under emscripten
    (config.h; the fork failed). JS `key()` returns -2 when the page stopped the
    game (New game/import), so no autosave then.
  - File ▾: Export = the S save, else the running checkpoint as one JSON bundle
    (level files); Import takes either; New game clears save/ + level files.
    Game end -> overlay "Play again" (reload); beforeunload warns while running.
  - Audio ▾: Sound effects (no hooks yet, stage 6), Music = `music/ambience.mp3`
    loop via <audio> (off by default, 43 MB, stored as `music` in the layout).
    fonts.json from $ROGUELIKES/fonts (cloud: /home/user/roguelikes).
  - Tested headless (Playwright): birth, windows filled, status colours,
    visible list, equipment, one-window mode, options menu (O) in both modes,
    A+ on one window only + kept over reload, map zoom kept, reload mid-game
    continues (T kept), autosave leaves map pixels unchanged and updates mtimes,
    S -> overlay -> Play again restores, #quit -> beacon ev=quit -> overlay ->
    new game, export bundle -> import -> restored, New game; rvip smoke, resize,
    idbtest pass (only /zeldhack + rvip-outbox databases, no localStorage).
  - `web/deploy.sh` written (guard, rsync to roguelikes/zeldhack/): ready/todo on
    Mac (deploy + live test + pane look). help.html missing (Help 404s): stage 6.
  - Open: one-window text uses square tile cells (wide letter spacing); shop not
    visited in tests; non-integer tile scale when the map window is < 12*32 px
    high; Visible lists only squares in sight (dark rooms: remembered items not listed).

