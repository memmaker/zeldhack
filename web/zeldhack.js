/*
 * ZeldHack (NetHack 3.6.7) in the browser: draws what win/web/winweb.c sends
 * (Module.nh): the map as tile indexes, prompt, status, inventory, pop-up
 * rows and messages. No tile logic here: C picks every tile. Windows are
 * placed by the shared rvip-wm.js. Keyboard + mouse, saves in IndexedDB
 * (IDBFS, /zeldhack). Loaded before zeldhack-core.js.
 * Derived from nethack50/web/nethack.js.
 */
(function () {
	'use strict';

	var DIR = '/zeldhack', SAVES = DIR + '/save', SEED = '/seed', COLNO = 80, ROWNO = 21;
	var PAL = ['#555', '#c82828', '#28aa28', '#aa6e28', '#3c3cdc', '#aa28aa', '#28aaaa', '#c8c8c8',
		'#646464', '#ff8c00', '#5aff5a', '#ffff50', '#6e6eff', '#ff5aff', '#5affff', '#fff'];
	/* arrows/Home/PgUp/End/PgDn: 0x101.. (winweb.h makes them hjklyubn or the number pad) */
	var KEYS = { ArrowUp: 0x101, ArrowDown: 0x102, ArrowLeft: 0x103, ArrowRight: 0x104, Home: 0x105, PageUp: 0x106,
		End: 0x107, PageDown: 0x108, Enter: 13, Escape: 27, Backspace: 8, Delete: 8, Tab: 9 };

	var events = [], lastSync = 0;
	var cells = null, chars = null, hero = { x: 0, y: 0, lev: -1 }, off = { x: 0, y: 0 };
	var cv, ctx, cell = 32, auto = true, sheet = null, sheetGen = 0, perRow = 40, TS = 32;
	/* the one tile set (ZeldHack by LS Pixel) in three sizes, then text; stored by name */
	var SETS = [['ZeldHack 16', 'tiles16.png'], ['ZeldHack 32', 'tiles.png'], ['ZeldHack 64', 'tiles64.png'], ['None', null]];
	var dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
	var log = [], prompt = '', rects = {}, wm = null, stat = [], music = null;
	var L = { cell: 0, wm: null, tiles: 'ZeldHack 32', text: false, face: '', mapFace: '', sound: false, music: false }, LAYOUT = DIR + '/web-layout.json';

	function $(id) { return document.getElementById(id); }
	function esc(t) { return t.replace(/[&<>]/g, function (c) { return '&' + (c === '&' ? 'amp' : c === '<' ? 'lt' : 'gt') + ';'; }); }

	/* ---------- map ---------- */
	function single() { return wm && wm.mode() === 'single'; }
	/* one-window mode: the game's whole 80x24 screen (prompt row, map, two
	   status rows) on this canvas, scaled to the window keeping its aspect */
	function rows() { return single() ? ROWNO + 3 : ROWNO; }
	function measure() {
		var w = COLNO * cell, h = rows() * cell;
		cv.width = w * dpr; cv.height = h * dpr;
		cv.style.width = w + 'px'; cv.style.height = h + 'px';
		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.imageSmoothingEnabled = false;     /* nearest-neighbour tiles */
	}
	function fit() {       /* biggest cell that shows the whole map in its window */
		var b = $('map'), best = 8;
		for (var c = 8; c <= 64; c++) if (COLNO * c <= b.clientWidth && ROWNO * c <= b.clientHeight) best = c;
		if (!L.text && sheet && best >= TS) return Math.floor(best / TS) * TS;   /* whole multiples of the sheet when it fits */
		return best;
	}
	/* default cell: the whole map if that is big enough, else the sheet's own
	   size (16 in text mode) while at least 12 rows fit; the camera follows
	   the hero, so a map wider than its window scrolls */
	function autoCell() {
		var f = fit(), want = Math.min(L.text || !sheet ? 16 : TS, Math.floor($('map').clientHeight / 12));
		return Math.max(f, want, 8);
	}
	/* one window: the biggest cell (whole device pixels) showing all 80x24 */
	function singleCell() {
		var b = $('map'), d = Math.floor(Math.min(b.clientWidth * dpr / COLNO, b.clientHeight * dpr / (ROWNO + 3)));
		return Math.max(1, d) / dpr;
	}
	function draw() {
		if (!cells) return;
		var one = single(), oy = one ? cell : 0;
		ctx.fillStyle = '#000'; ctx.fillRect(0, 0, COLNO * cell, rows() * cell);
		ctx.font = (L.mapFace ? '' : 'bold ') + Math.round(cell * 0.8) + 'px ' + (face('map') || 'monospace');
		ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
		if (one) {         /* the game's prompt row and status rows as text cells */
			textRow([{ c: 11, a: 0, t: prompt }], 0);
			stat.forEach(function (segs, i) { textRow(segs, ROWNO + 1 + i); });
		}
		if (L.text) {       /* text mode: the game's own characters and colours */
			for (var ty = 0; ty < ROWNO; ty++)
				for (var tx = 0; tx < COLNO; tx++) {
					var k = chars[ty * COLNO + tx];
					if ((k & 0xff) <= 32) continue;
					ctx.fillStyle = PAL[(k >> 8) & 15];
					ctx.fillText(String.fromCharCode(k & 0xff), (tx + 0.5) * cell, oy + (ty + 0.5) * cell + 1);
				}
		} else if (!sheet) return;
		else for (var y = 0; y < ROWNO; y++)
			for (var x = 0; x < COLNO; x++) {
				var t = cells[y * COLNO + x];
				if (t >= 0) ctx.drawImage(sheet, (t % perRow) * TS, Math.floor(t / perRow) * TS, TS, TS, x * cell, oy + y * cell, cell, cell);
			}
	}
	/* one text row of segments {c colour (-1 default), a HL_* bits, t} on the canvas */
	function textRow(segs, y) {
		var x = 0;
		segs.forEach(function (sg) {
			var fg = sg.c >= 0 ? PAL[sg.c] : '#d7d7d7';
			for (var i = 0; i < sg.t.length && x < COLNO; i++, x++) {
				if (sg.a & 4) { ctx.fillStyle = fg; ctx.fillRect(x * cell, y * cell, cell, cell); }
				if (sg.t[i] === ' ') continue;
				ctx.fillStyle = sg.a & 4 ? '#000' : fg;
				ctx.fillText(sg.t[i], (x + 0.5) * cell, (y + 0.5) * cell + 1);
			}
		});
	}
	/* keep the hero in the middle half of the map window; recentre when it leaves it */
	function scrollMap() {
		off = RvipWM.center(cv, (hero.x + 0.5) * cell, (hero.y + 0.5) * cell, COLNO * cell, rows() * cell);
	}
	/* a row's icon: the tile with tiles on, else the item's own map symbol (C sends both) */
	function icon(t, sym) {
		if (L.text || !sheet) return sym > 32 ? esc(String.fromCharCode(sym)) + ' ' : '';
		return tileSpan(t);
	}
	function tileSpan(t) {     /* a tile at text size, for menus and the inventory */
		if (t < 0) return '';
		return '<span class="ti" style="background-size:' + (perRow * 16) + 'px auto;background-position:-' + (t % perRow) * 16 + 'px -' + Math.floor(t / perRow) * 16 + 'px"></span>';
	}

	/* ---------- text windows ---------- */
	/* status: lines of tab-separated segments "colour:HL_* bits:text" (winweb.c, hilite_status) */
	function parseStat(t) {
		return t.split('\n').filter(function (l) { return l; }).map(function (l) {
			return l.split('\t').map(function (sg) {
				var i = sg.indexOf(':'), j = sg.indexOf(':', i + 1);
				return { c: +sg.slice(0, i), a: +sg.slice(i + 1, j), t: sg.slice(j + 1) };
			});
		});
	}
	function drawStat() {
		$('stat').innerHTML = stat.map(function (segs) {
			return segs.map(function (sg) {
				var fg = sg.c >= 0 ? PAL[sg.c] : '', cls = (sg.a & 18 ? 'b ' : '') + (sg.a & 8 ? 'u ' : '') + (sg.a & 32 ? 'd' : '');
				var st = sg.a & 4 ? 'background:' + (fg || '#d7d7d7') + ';color:#000' : fg ? 'color:' + fg : '';
				return st || cls ? '<span' + (cls ? ' class="' + cls.trim() + '"' : '') + (st ? ' style="' + st + '"' : '') + '>' + esc(sg.t) + '</span>' : esc(sg.t);
			}).join('');
		}).join('\n');
	}
	/* Visible window icons: the tile at 16 px (text mode: the glyph) */
	function visIcon(t) {
		if (L.text || !sheet || !(t >= 0)) return null;
		var d = document.createElement('span');
		d.innerHTML = tileSpan(t);
		return d.firstChild;
	}
	function drawMsgs() {
		var ml = $('msg'), body = ml.parentNode;
		ml.innerHTML = log.map(function (m) { return m.old ? '<span class="old">' + esc(m.t) + '</span>' : esc(m.t); }).join('\n') +
			(prompt ? (log.length ? '\n' : '') + '<span class="pr">' + esc(prompt) + '</span>' : '');
		body.scrollTop = body.scrollHeight;     /* the newest message stays in view */
	}
	/* rows "tile \t letter \t 0|1 selected, 2 heading \t colour \t symbol \t text" (winweb.h) */
	function rowsHtml(t, cur) {
		var ls = t.split('\n').filter(function (l, i, a) { return l || i < a.length - 1; });
		var lettered = ls.some(function (l) { var f = l.split('\t'); return f[2] !== '2' && f[1] !== ' '; });   /* no letters at all: no indent */
		return ls.map(function (l, i) {
			var f = l.split('\t'), text = f.slice(5).join('\t'), sel = +f[2];
			var h = sel === 2 ? '' : f[1] === ' ' ? (lettered ? '    ' : '') : esc(f[1]) + (sel ? ' + ' : ' - ');   /* no letter: keyed by symbol */
			return '<div class="row' + (i === cur ? ' cur' : '') + (sel === 2 ? '' : ' pick') + '" data-i="' + i + '" style="color:' + PAL[+f[3]] + '">' +
				h + icon(+f[0], +f[4]) + esc(text) + '</div>';
		}).join('');
	}
	function drawPop(t) {
		var pop = $('pop');
		if (!t) { pop.hidden = true; return; }
		var nl = t.indexOf('\n'), head = t.slice(0, nl).split('\t'), top = +head[0], cur = +head[1], p = head.slice(2).join('\t');
		pop.innerHTML = (p ? '<div class="pp">' + esc(p) + '</div>' : '') + '<div class="rows">' + rowsHtml(t.slice(nl + 1), cur) + '</div>';
		pop.hidden = false;
		RvipWM.popup(pop, { center: true });
		var rows = pop.querySelectorAll('.row'), r = rows[cur >= 0 ? cur : top];
		if (r) { if (cur >= 0) r.scrollIntoView({ block: 'nearest' }); else pop.scrollTop = r.offsetTop - pop.firstChild.offsetHeight; }
	}

	/* ---------- layout (shared rvip-wm.js, RVIP.md 5b) ---------- */
	function saveLayout() {
		try { Module.FS.writeFile(LAYOUT, JSON.stringify(L)); app.sync(); } catch (e) { console.warn('layout not saved', e); }
	}
	function fonts() {
		['msg', 'stat', 'inv', 'equip', 'vis', 'pop'].forEach(function (id) { if (id === 'pop') $(id).style.fontSize = RvipWM.fontSize('msg') + 'px'; $(id).style.fontFamily = face('txt'); });
		renderMapSel();
	}
	/* the top-bar font is for the text windows and pop-ups; the map (text mode) has its own */
	function face(p) { var f = p === 'map' ? L.mapFace : L.face; return f ? '"' + f + '", monospace' : ''; }
	function loadFace(n) {
		if (!n) { fonts(); draw(); return; }
		var ff = new FontFace(n, 'url(../fonts/' + n + '.woff)');
		ff.load().then(function () { document.fonts.add(ff); fonts(); draw(); }).catch(function () { app.status('Could not load the font ' + n + '.', true); });
	}
	/* map font chooser on the Map title bar (shown on hover), text mode only */
	var mapSel = document.createElement('select');
	mapSel.title = 'Map font (text mode)';
	mapSel.innerHTML = '<option value="">Default font</option>';
	mapSel.addEventListener('pointerdown', function (e) { e.stopPropagation(); });   /* not a window drag */
	function renderMapSel() {
		var bs = document.querySelector('#t-map .wm-btns');
		if (bs && mapSel.parentNode !== bs) bs.insertBefore(mapSel, bs.firstChild);
		mapSel.hidden = !L.text;
		mapSel.value = L.mapFace || '';
	}
	/* Audio: the game names the sounds (websound.c); this only mutes them */
	function renderAudio() { $('chk-sound').checked = L.sound; $('chk-music').checked = L.music; }
	/* music: the ZeldHack ambience loop, off by default; browsers start it only after a key or click */
	function playMusic() {
		if (L.music) {
			if (!music) { music = new Audio('music/ambience.mp3'); music.loop = true; music.volume = 0.6; }
			music.play().catch(function () { });
		} else if (music) music.pause();
	}
	function makeWM() {
		showMode(); renderAudio(); $('sel-font').value = L.face; loadFace(L.face); loadFace(L.mapFace);
		if (L.cell >= 8 && L.cell <= 64) { cell = L.cell; auto = false; }
		var H = $('game').clientHeight || 600, line = Math.ceil(RvipWM.fontSize('msg') * 1.4) + 6;
		wm = RvipWM({
			area: $('game'), menu: $('btn-layout'),
			wins: [{ id: 'map', title: 'Map' }, { id: 'msg', title: 'Messages' }, { id: 'stat', title: 'Status' },
				{ id: 'inv', title: 'Inventory' }, { id: 'vis', title: 'Visible' }, { id: 'equip', title: 'Equipment' }],
			/* default on: map, messages, status, inventory, visible; equipment via Windows (the inventory lists it too) */
			multi: { d: 'h', r: 0.72, a: { d: 'v', r: Math.min(0.3, 5 * line / H), a: 'msg', b: { d: 'v', r: 1 - (2 * line + 30) / (H - 5 * line), a: 'map', b: 'stat' } },
				b: { d: 'v', r: 0.62, a: 'inv', b: 'vis' } },
			single: 'map',
			state: L.wm,
			save: function (st) { L.wm = st; saveLayout(); },
			layout: function (r) { rects = r; fonts(); relayout(); },
			zoom: { map: function (size, d) { zoom(4 * d); }, msg: fonts },   /* A- / A+ on the map zooms the map; the rest the WM sizes */
			onReset: function () { auto = true; L.cell = 0; /* face, mapFace, sound stay */ L.wm = wm.state(); fonts(); relayout(); saveLayout(); }
		});
		wm.apply();
	}
	/* cell size for the mode: one window fits 80x24, else the auto cell unless A-/A+ chose one */
	function relayout() {
		if (single()) cell = singleCell();
		else if (auto) cell = autoCell();
		else cell = L.cell || cell;
		measure(); scrollMap(true); draw();
	}
	function showMode() { $('btn-tiles').textContent = 'Tiles: ' + L.tiles; }
	function setIndex(n) { for (var i = 0; i < SETS.length; i++) if (SETS[i][0] === n) return i; return 1; }
	/* load the chosen sheet; a late onload of an older choice (or after None) is ignored */
	function loadSheet() {
		var set = SETS[setIndex(L.tiles)], gen = ++sheetGen;
		L.tiles = set[0]; L.text = !set[1];
		sheet = null;
		if (!set[1]) return;
		var img = new Image();
		img.onload = function () {
			if (gen !== sheetGen || L.text) return;
			sheet = img; perRow = 40; TS = img.width / perRow;
			document.documentElement.style.setProperty('--ti', 'url(' + set[1] + ')');
			if (cv && !$('game').hidden) { relayout(); relist(); }
		};
		img.src = set[1];
	}
	/* Tiles button: 16 -> 32 -> 64 -> None -> 16; like a restart: sheet, cell size, lists */
	function nextSet() {
		L.tiles = SETS[(setIndex(L.tiles) + 1) % SETS.length][0];
		auto = true; L.cell = 0;
		loadSheet(); showMode(); renderMapSel(); saveLayout();
		relayout(); relist();
	}
	function readLayout() {
		try { var s = JSON.parse(Module.FS.readFile(LAYOUT, { encoding: 'utf8' })); if (s) L = { cell: s.cell | 0, wm: s.wm, tiles: s.tiles || (s.text ? 'None' : 'ZeldHack 32'), text: false, face: s.face || '', mapFace: s.mapFace || '', sound: s.sound === true, music: s.music === true }; } catch (e) { }
		if (s && L.wm && !L.wm.fs) L.wm.fs = s.fs || (s.font ? { msg: s.font, stat: s.font, inv: s.font } : undefined);   /* old layout: sizes move to the WM */
	}
	function zoom(d) {
		if (single()) return;
		auto = false;
		cell = Math.max(8, Math.min(64, Math.round(cell) + d));
		L.cell = cell; saveLayout();
		measure(); scrollMap(true); draw();
	}

	/* inventory and pop-up again from the game's last rows (tile set changed) */
	function relist() {
		$('vis')._vis = null;
		[2, 3, 7, 8].forEach(function (id) { var t = nh.last[id]; if (t != null) { nh.last[id] = null; nh.text(id, t); } });
	}

	var nh = {
		map: function (cp, tp, hx, hy, lev) {
			cells = Module.HEAP32.slice(cp >> 2, (cp >> 2) + COLNO * ROWNO);
			chars = Module.HEAP32.slice(tp >> 2, (tp >> 2) + COLNO * ROWNO);
			if ($('game').hidden) { $('game').hidden = false; app.status(''); measure(); makeWM(); }
			var moved = hx !== hero.x || hy !== hero.y, lv = lev !== hero.lev;
			hero.x = hx; hero.y = hy; hero.lev = lev;
			if (moved || lv) scrollMap(lv);
			draw();
		},
		last: [],
		text: function (id, t) {
			if (id === 4) { log.push({ t: t }); if (log.length > 300) log.shift(); drawMsgs(); return; }
			if (id === 6) { if (log.length) log[log.length - 1] = { t: t }; drawMsgs(); return; }   /* the game folded a repeat */
			if (id === 5) { log.forEach(function (m) { m.old = true; }); drawMsgs(); return; }
			if (nh.last[id] === t) return;
			nh.last[id] = t;
			if (id === 0) { prompt = t; RvipWM.prompt.text(t); drawMsgs(); if (single()) draw(); }
			else if (id === 1) { stat = parseStat(t); drawStat(); if (single()) draw(); }
			else if (id === 2) $('inv').innerHTML = rowsHtml(t, -1);
			else if (id === 3) drawPop(t);
			else if (id === 7) RvipWM.visible($('vis'), t, visIcon);
			else if (id === 8) $('equip').innerHTML = rowsHtml(t, -1);
		},
		/* peek: number of waiting keys; otherwise the next key or -1.
		 * While the game waits, the files go to IndexedDB every 2 s. */
		key: function (peek, atCmd) {
			RvipWM.prompt.wait(atCmd);
			if (peek) return events.length;
			if (!app.running) return -2;      /* New game / import / crash: no autosave any more */
			if (events.length) return events.shift();
			var now = performance.now();
			if (now - lastSync > 2000) { lastSync = now; app.sync(); }
			return -1;
		},
		end: function () {
			app.running = false;
			return new Promise(function (done) {
				app.sync(function () {
					$('overlay-msg').textContent = saveFile() ? 'Your game has been saved. Play again to continue it.' : 'The game is over.';
					$('overlay').hidden = false;
					done();
				});
			});
		}
	};

	/* ---------- input ---------- */
	function onKey(e) {
		if (!app.running || e.isComposing || e.metaKey || /^(INPUT|TEXTAREA)$/.test(e.target.tagName)) return;
		var k = e.key, c;
		if (e.code === 'NumpadEnter') c = 13;
		else if (KEYS[k] !== undefined) c = KEYS[k];
		else if (k.length === 1) {
			c = k.charCodeAt(0);
			if (e.ctrlKey && !e.altKey) {
				var u = k.toUpperCase().charCodeAt(0);
				if (u >= 65 && u <= 90) c = u & 0x1f; else return;
			} else if (e.altKey && c < 128) c |= 0x80;     /* Alt = meta, as NetHack's M- keys */
			if (c > 255) return;
		}
		else return;
		events.push(c);
		e.preventDefault();
	}
	function onMapClick(e) {
		if (!app.running) return;
		var r = cv.getBoundingClientRect(), x = Math.floor((e.clientX - r.left) / cell), y = Math.floor((e.clientY - r.top) / cell) - (single() ? 1 : 0);
		if (x > 0 && x < COLNO && y >= 0 && y < ROWNO) events.push(0x10000 | y << 8 | x | (e.button === 2 ? 0x8000 : 0));
		e.preventDefault();
	}

	/* ---------- saves: IndexedDB (IDBFS), Export / Import / New game in rvip-app.js ---------- */
	function ls(d, re) { try { return Module.FS.readdir(d).filter(function (f) { return re.test(f); }); } catch (e) { return []; } }
	/* save/<uid><name> after S; <uid><name>.0.. level files (checkpoint) while a game runs */
	function saveFile() { return ls(SAVES, /^\d+.+$/)[0] || null; }
	function charName() {
		var f = saveFile() || ls(DIR, /^\d+.+\.0$/)[0];
		return f ? f.replace(/^\d+/, '').replace(/\.0$/, '') : null;
	}
	function clearGame() {
		ls(SAVES, /^\d/).forEach(function (f) { Module.FS.unlink(SAVES + '/' + f); });
		ls(DIR, /^\d+.+\.\d+$/).forEach(function (f) { Module.FS.unlink(DIR + '/' + f); });
	}
	/* the autosave checkpoint: <uid><name>.<level> files (INSURANCE) */
	function checkpoint() { return ls(DIR, /^\d+.+\.\d+$/).map(function (f) { return DIR + '/' + f; }); }
	var app = RvipApp({
		name: 'zeldhack',
		/* Export: the S save file, else the running game's checkpoint as one bundle */
		save: function () { var f = saveFile(); if (f) return SAVES + '/' + f; var c = checkpoint(); return c.length ? c : null; },
		clear: clearGame,
		put: function (file, data) {
			if (/^\d+[^/]+\.\d+$/.test(file.name)) { Module.FS.writeFile(DIR + '/' + file.name, data); return; }   /* checkpoint bundle */
			var name = file.name.replace(/^\d+/, '').replace(/\.gz$/, '').replace(/[^\w-]/g, '');
			if (!name) return 'A NetHack save file is named like 501Name (user number, then the character name).';
			Module.FS.writeFile(SAVES + '/0' + name, data);
		},
		noSave: 'There is no game to export yet.',
		helpText: 'Press ? in the game for its own help.'
	});

	/* ---------- startup ---------- */
	window.Module = {
		nh: nh,
		arguments: [],
		preRun: [function () {
			var FS = Module.FS;
			Module.ENV.HOME = DIR;
			Module.ENV.USER = 'player';
			Module.ENV.NETHACKOPTIONS = '@' + DIR + '/nethackrc';   /* the player's ZeldHack options (web/build.sh) */
			FS.mkdirTree(DIR);
			FS.mount(Module.IDBFS, {}, DIR);
			Module.addRunDependency('idbfs');
			FS.syncfs(true, function (err) {
				if (err) app.status('Could not read saved games from IndexedDB (' + err + '). Saving may not work in this browser mode.', true);
				FS.readdir(SEED).forEach(function (f) { if (f[0] !== '.') FS.writeFile(DIR + '/' + f, FS.readFile(SEED + '/' + f)); });
				['perm', 'record', 'logfile', 'xlogfile', 'livelog'].forEach(function (f) { try { FS.stat(DIR + '/' + f); } catch (e) { FS.writeFile(DIR + '/' + f, ''); } });
				try { FS.mkdir(SAVES); } catch (e) { }
				var n = charName();
				if (n) Module.arguments.push('-u', n);
				readLayout(); loadSheet();     /* the stored tile set, before any sheet loads */
				Module.removeRunDependency('idbfs');
			});
		}],
		onRuntimeInitialized: function () { app.running = true; },
		print: function (s) { console.log(s); },
		printErr: function (s) { console.warn(s); },
		setStatus: function (s) { if (s && !app.running) app.status(s.replace(/\(\d+\/\d+\)/, '').trim() || 'Loading…'); },
		onAbort: function (what) { app.crashed(what); }
	};
	document.addEventListener('visibilitychange', function () { if (document.hidden) app.sync(); });
	window.addEventListener('pagehide', function () { app.sync(); });
	setInterval(function () { if (app.running) app.sync(); }, 15000);
	/* a running game is autosaved, but a reload mid-turn loses the last moves */
	window.addEventListener('beforeunload', function (e) { if (app.running && cells) { app.sync(); e.preventDefault(); e.returnValue = ''; } });

	window.addEventListener('resize', function () { if (wm) wm.apply(); });
	document.addEventListener('keydown', onKey);
	document.addEventListener('DOMContentLoaded', function () {
		cv = document.querySelector('#map canvas');
		ctx = cv.getContext('2d');
		cv.addEventListener('mousedown', onMapClick);
		cv.addEventListener('contextmenu', function (e) { e.preventDefault(); });
		$('pop').addEventListener('mousedown', function (e) {
			var r = e.target.closest('.row.pick');
			if (r && app.running) { events.push(0x20000 | +r.dataset.i); e.preventDefault(); }
		});
		$('btn-tiles').onclick = nextSet;
		RvipWM.dropdown($('btn-file'), $('menu-file'));
		RvipWM.dropdown($('btn-audio'), $('menu-audio'));
		$('chk-sound').onchange = function () { L.sound = this.checked; saveLayout(); };
		$('chk-music').onchange = function () { L.music = this.checked; saveLayout(); playMusic(); };
		['keydown', 'pointerdown'].forEach(function (ev) {
			document.addEventListener(ev, function once() { document.removeEventListener(ev, once, true); if (L.music) playMusic(); }, true);
		});
		if (window.RVIPSound) {
			var play = RVIPSound.play;
			RVIPSound.play = function (n, v) { if (L.sound) play(n, v); };
		}
		/* fonts: the index page's fonts/ (web/build.sh writes fonts.json) */
		fetch('fonts.json').then(function (r) { return r.json(); }).then(function (list) {
			[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
				list.forEach(function (n) { var o = document.createElement('option'); o.value = n; o.textContent = n.replace(/^Web(Plus|437)_/, '').replace(/_/g, ' '); a[0].appendChild(o); });
				a[0].value = L[a[1]] || '';
			});
		}).catch(function () { });
		[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
			a[0].onchange = function () { L[a[1]] = this.value; saveLayout(); loadFace(this.value); this.blur(); };
		});
		$('btn-restart').onclick = function () { location.reload(); };
		document.querySelectorAll('button').forEach(function (b) {
			b.addEventListener('mousedown', function (e) { e.preventDefault(); });
		});
	});
})();
