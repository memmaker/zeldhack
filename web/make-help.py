#!/usr/bin/env python3
"""Writes the in-page game guide (dist/help.html) for the ZeldHack web build.

Self-contained (cloud run, no ~/Desktop/Games/Roguelikes/Docs): the complete
key list is parsed from the game's own dat/cmdhelp with the web defaults
(number_pad:2 from zeldhack/nethackrc, no debug/shell/suspend); the rest is
written here.  On the Mac this can become a Docs GAMES/GUIDES entry.
Usage: python3 web/make-help.py > web/dist/help.html"""
import html, os, re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
esc = html.escape
OPTS = {'number_pad': '2', 'debug': False, 'shell': False, 'suspend': False,
        'rest_on_space': False}


def cond_true(c):
    c = c.split('#')[0].strip()
    if not c:
        return None                     # plain else
    m = re.match(r'(\w+)\s*=\s*([-\d,\s]+)$', c)
    if m:
        return OPTS[m.group(1)] in [v.strip() for v in m.group(2).split(',')]
    neg = c.startswith('!')
    return bool(OPTS[c.lstrip('!')]) != neg


def cmdhelp():
    rows, stack = [], []                # stack of [parent_on, this_on, taken]
    on = lambda: all(s[1] for s in stack)
    for line in open(os.path.join(ROOT, 'dat/cmdhelp'), encoding='latin-1'):
        line = line.rstrip('\n')
        if line.startswith('&?'):
            t = cond_true(line[2:])
            stack.append([on(), bool(t), bool(t)])
        elif line.startswith('&:'):
            t = cond_true(line[2:])
            s = stack[-1]
            s[1] = (not s[2]) and (t is None or t)
            s[2] = s[2] or s[1]
        elif line.startswith('&.'):
            stack.pop()
        elif line.startswith('&'):
            continue
        elif on() and '\t' in line:
            k, d = line.split('\t', 1)
            if not d.startswith('unavailable'):
                rows.append((k, d.strip()))
    rows.append(('Enter', 'Menu of all commands (this web version)'))
    return rows


def kbd(k):
    return '<kbd>' + esc(k) + '</kbd>'


def dl(items):
    return '<dl>' + ''.join(f'<dt>{kbd(k)}</dt><dd>{esc(d)}</dd>' for k, d in items) + '</dl>'


def section(anchor, title, body):
    return f'<h2 id="h-{anchor}">{esc(title)}</h2>{body}'


KEY_HINTS = [
    ('?', 'In-game help menu (command list, options, history)'),
    ('~', 'Auto-explore: walk the level until something happens'),
    ('Enter', 'Menu of all commands, with their keys'),
    ('i', 'Inventory with a cursor: Enter = everything you can do with the item'),
    ('>', 'Go down; off the stairs it first walks to the nearest known staircase'),
    ('<', 'Go up (same walking)'),
    ('S', 'Save and stop; reload the page to continue'),
]

ESSENTIALS = [
    ('Moving', [('Arrows / numpad', 'Move (numpad 7 9 1 3 or Home PgUp End PgDn = diagonals)'),
                ('G + direction', 'Rush in a direction'), ('_', 'Travel to a spot you pick on the map'),
                ('s', 'Search for hidden doors and traps'), ('.', 'Rest one turn'),
                ('~', 'Auto-explore')]),
    ('Items', [('i', 'Inventory'), (',', 'Pick up'), ('d', 'Drop'), ('e', 'Eat'),
               ('q', 'Quaff (drink)'), ('r', 'Read'), ('w', 'Wield a weapon'),
               ('W / T', 'Wear / take off armour'), ('P / R', 'Put on / remove a ring or amulet'),
               ('a', 'Apply a tool'), ('z', 'Zap a wand'), ('Z', 'Cast a spell')]),
    ('Fighting and more', [('F + direction', 'Fight in a direction'), ('t / f', 'Throw / fire'),
                           ('k', 'Kick (number pad mode)'), ('o / c', 'Open / close a door'),
                           ('#pray', 'Pray (Enter menu or #)'), ('Ctrl+X', 'Your attributes'),
                           (';', 'What is that on the map?'), ('Ctrl+P', 'Previous messages')]),
]

SAVING = '''<ul>
<li><strong>Saving is automatic.</strong> A second after each command the game writes a checkpoint, and the browser keeps it (IndexedDB). Reloading the page or closing the tab continues from there.</li>
<li><kbd>S</kbd> saves everything and stops; reload the page (or press <em>Play again</em>) to continue exactly where you were.</li>
<li>When your character dies or you quit, the save is deleted: death is final.</li>
<li>Each browser keeps <strong>one game</strong>. <em>File ▾ → New game</em> deletes it and starts over.</li>
<li><em>File ▾ → Export</em> downloads the save (or the running checkpoint) as one file; <em>Import</em> loads such a file again, also on another computer.</li>
<li>Private/incognito windows and "clear site data" delete the stored game. Export first if it matters.</li>
</ul>'''

TIPS = '''<ul>
<li>Press <kbd>~</kbd> to explore; it stops when a monster comes into view, a message appears or you press a key.</li>
<li>Press <kbd>&gt;</kbd> anywhere once you have seen the down stairs: you walk there, press it again to descend.</li>
<li>Eat corpses of freshly killed, safe monsters when "Hungry" shows; never eat cockatrices, and avoid old corpses.</li>
<li>Your pet can tell cursed items: items it steps over reluctantly ("moves only reluctantly") are cursed.</li>
<li>Pray (<kbd>#pray</kbd>) when your HP is below 1/7 of the maximum or you are Weak from hunger, but not more often than about every 1000 turns.</li>
<li>Elbereth: engrave it in the dust (<kbd>E</kbd>, <kbd>-</kbd> for fingers) and most monsters will not melee you while you stand on it.</li>
<li>Unknown wands: engrave-test them (<kbd>E</kbd>, then the wand) to learn what they do.</li>
<li>Do not fight on in bad shape: retreat up the stairs, quaff unknown potions only in an emergency.</li>
</ul>'''

GUIDE = '''<h3>The goal</h3>
<p>Descend through the Dungeons of Doom, get the Amulet of Yendor from the bottom of Gehennom and offer it on your god's altar on the Astral Plane. Most characters die long before that; getting deeper is the real measure.</p>
<h3>Your first game</h3>
<ol>
<li>Pick a name, then a role. Valkyrie or Samurai are the easiest to start with; let the game pick the race and alignment.</li>
<li>Look at the Inventory window: your weapon is wielded and your armour worn already.</li>
<li>Press <kbd>~</kbd> to explore. When a monster appears, walk into it to attack.</li>
<li>Pick up things with <kbd>,</kbd>; gold counts, most items are worth trying on later.</li>
<li>When the level is explored, press <kbd>&gt;</kbd> to walk to the down stairs and again to descend.</li>
<li>Watch HP on the Status line. Low? Rest with <kbd>s</kbd> (or <kbd>n</kbd><kbd>2</kbd><kbd>0</kbd><kbd>s</kbd> for 20 turns) away from monsters, or go back upstairs.</li>
</ol>
<h3>Reading the screen</h3>
<p>The map uses the ZeldHack tiles; Status shows your stats (St Dx Co In Wi Ch), alignment, Dlvl (depth), gold ($), HP, Pw (magic), AC (armour class, lower is better), experience level and turn count. The Visible window lists the monsters and items you can see; <kbd>;</kbd> identifies any spot on the map.</p>
<h3>Things that kill new players</h3>
<ul><li>Floating eyes: never melee one (you get paralysed); throw things at it instead.</li>
<li>Cockatrices and chickatrices: do not touch or eat them without gloves.</li>
<li>Soldier ants and dwarves with mattocks early on: flee upstairs.</li>
<li>Starvation: keep some food and eat when "Hungry" appears.</li></ul>'''

WEB = '''<ul>
<li><strong>Keys:</strong> the arrow keys or the number pad move you (Home, PgUp, End, PgDn for the diagonals); ZeldHack uses NetHack's <em>number_pad</em> mode, so <kbd>h</kbd> is help, <kbd>j</kbd> jump, <kbd>k</kbd> kick and <kbd>l</kbd> loot.</li>
<li><kbd>Enter</kbd> opens a menu of every command; <kbd>?</kbd> is the game's own help, <kbd>O</kbd> its options.</li>
<li>There is no --More--: all messages go to the Messages window.</li>
<li><em>Windows ▾</em> shows, hides and arranges the windows (drag title bars and gaps); <em>A−</em>/<em>A+</em> on a window's title bar change its text or tile size. <em>Tiles</em> switches between the ZeldHack 16, 32 and 64 px sets and text.</li>
<li><em>Audio ▾</em>: <em>Sound effects</em> (the ZeldHack sounds for hits, misses, doors, stairs, traps, hunger, level-ups…) and <em>Music</em> (the ZeldHack ambience loop). Both are off until you switch them on; the music is only downloaded then.</li>
<li>Browsers keep a few shortcuts for themselves (<kbd>Ctrl+W</kbd>, <kbd>Ctrl+T</kbd>, <kbd>Ctrl+N</kbd>, <kbd>Cmd</kbd> shortcuts on a Mac), so those never reach the game.</li>
<li>While this guide is open the game gets no keys; <kbd>Esc</kbd> closes it.</li>
</ul>'''

ABOUT = '''<p><strong>ZeldHack</strong> is NetHack 3.6.7 dressed up as an 8-bit console adventure: a tile set and sound effects in the style of the NES-era classics (The Legend of Zelda, Dragon Quest, Final Fantasy and others), with the original game underneath unchanged.</p>
<p>NetHack is the classic dungeon crawl: descend a randomly generated dungeon, fight, find and identify items, and try to retrieve the Amulet of Yendor. Every game is different, death is permanent, and almost everything interacts with everything else.</p>
<h3>Credits</h3>
<ul>
<li><strong>NetHack 3.6.7</strong> by the NetHack DevTeam (<a href="https://www.nethack.org/">nethack.org</a>), based on Hack by Jay Fenlason and Andries Brouwer; licensed under the NetHack General Public License.</li>
<li><strong>ZeldHack tiles, sounds and ambience music</strong>: the ZeldHack asset pack by LSpixel (<a href="https://lspixel.itch.io/zeldhack-for-nethack-ready-to-play">lspixel.itch.io/zeldhack-for-nethack-ready-to-play</a>); its sounds come from classic NES/Famicom games.</li>
<li><strong>Web version</strong>: our changes (auto-explore, stair walking, command menu, inventory item menus, window port, sound hooks, browser build) are local to this port and under the same licence.</li>
</ul>
<h3>About this version</h3>
<p>Based on NetHack 3.6.7 (tag <code>NetHack-3.6.7_Released</code>), <a href="https://github.com/NetHack/NetHack/tree/ed600d9f0f3c37677418f0150f59363ca641f3dc">NetHack/NetHack @ ed600d9</a>, with the ZeldHack assets by LSpixel. Web port source and all our changes: <a href="https://github.com/memmaker/zeldhack">memmaker/zeldhack</a> (<a href="https://github.com/memmaker/zeldhack/compare/d4d2545fcee7d935912b6f2e2fb34556ad58c295...main">changes against upstream</a>).</p>'''

all_keys = cmdhelp()
assert len(all_keys) > 30, len(all_keys)
assert any(k == '~' for k, _ in all_keys)
toc = [('about', 'About ZeldHack'), ('keys', 'Keyboard controls'), ('saving', 'Saving your game'),
       ('tips', 'Tips'), ('guide', "New player's guide"), ('web', 'Playing in the browser')]
parts = ['<p>NetHack 3.6.7 with ZeldHack tiles and sounds, in your browser.</p><ul class="toc">' +
         ''.join(f'<li><a href="#h-{a}">{esc(t)}</a></li>' for a, t in toc) + '</ul>']
parts.append(section('about', 'About ZeldHack', ABOUT))
ess = ''.join(f'<div class="box"><h3>{esc(c)}</h3>{dl(i)}</div>' for c, i in ESSENTIALS)
full = ''.join(f'<div>{kbd(k)}<span>{esc(d)}</span></div>' for k, d in all_keys)
parts.append(section('keys', 'Keyboard controls',
                     '<div class="box key"><h3>The keys to remember</h3>' + dl(KEY_HINTS) + '</div>'
                     '<h3>Essential keys</h3><div class="grid">' + ess + '</div>'
                     '<details><summary>Complete key list (' + str(len(all_keys)) + ' commands, from the game\'s own help)</summary>'
                     '<div class="all">' + full + '</div></details>'))
parts.append(section('saving', 'Saving your game', SAVING))
parts.append(section('tips', 'Tips', TIPS))
parts.append(section('guide', "New player's guide", GUIDE))
parts.append(section('web', 'Playing in the browser', WEB))
print('\n'.join(parts))
