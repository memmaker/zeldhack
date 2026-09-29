/* NetHack 3.6  winweb.c  browser window port for ZeldHack (RVIP) */
/* C sends finished data (tile indexes, colours, text rows) to web/zeldhack.js,
 * which only draws it; rvip-wm.js places the windows.  Derived from the
 * nethack50 SDL2/web port.  Keys come from Module.nh.key(); Asyncify lets the
 * game wait for them.
 *   js_map:  ROWNO*COLNO tile indexes (-1 = nothing) and text cells
 *            (char | colour << 8), hero, level
 *   js_text: 0 prompt, 1 status lines, 2 inventory, 3 pop-up, 4 new
 *            message, 5 messages so far are old, 6 replace last message.
 *            Rows are tab-separated: tile, letter, 0/1 selected or 2 heading,
 *            colour, symbol (char code, 0 none), text. */

#include <emscripten.h>
#include <stdarg.h>
#include "hack.h"
#include "func_tab.h"
#include "dlb.h"

extern short glyph2tile[];

static winid web_create_nhwindow(int);
static void web_start_menu(winid);
static void web_add_menu(winid, int, const anything *, char, char, int,
                         const char *, boolean);
static void web_end_menu(winid, const char *);
static int web_select_menu(winid, int, menu_item **);
static void web_destroy_nhwindow(winid);
static void web_exit_nhwindows(const char *);
static void web_putstr(winid, int, const char *);
static void web_display_nhwindow(winid, boolean);

#define HIST_MAX 200
#define MAXWIN 32
#define POP_ROWS 20

struct line { char *s; int attr, clr; };
struct mitem {
    anything id;
    char ch, gch;
    int attr, clr, tile, sym;
    char *s;
    boolean sel;
};
struct nhw {
    int type;
    boolean used;
    struct line *lines;
    int nlines, cury;
    struct mitem *items;
    int nitems;
    char *prompt;
};

static struct nhw wins[MAXWIN];
static int mapt[ROWNO][COLNO];   /* tile index, -1 = nothing */
static int mapc[ROWNO][COLNO];   /* char | colour << 8 */
static char *hist[HIST_MAX];
static char hist_prev[BUFSZ];
static int hist_reps, nhist;
static int mouse_x, mouse_y, mouse_btn;
static char promptbuf[BUFSZ * 2];
static struct mitem *perm;
static int nperm;
static boolean perm_building;
static int popup = -1, pop_top, pop_cur = -1;
static boolean pop_any;
#define POP_CLICK (pop_any ? ' ' : '\n')

/* status fields, filled by web_status_update, drawn on BL_FLUSH */
static char stat_val[MAXBLSTATS][MAXCO];
static char stat_fmt[MAXBLSTATS][32];
static boolean stat_on[MAXBLSTATS];
static char statlines[2][MAXCO * 2];

static int cells[ROWNO * COLNO], chars[ROWNO * COLNO];
static char *tbuf;
static size_t tlen, tcap;

EM_JS(void, js_map, (int *c, int *t, int hx, int hy, int lev),
      { Module.nh.map(c, t, hx, hy, lev); });
EM_JS(void, js_text, (int id, const char *s),
      { Module.nh.text(id, UTF8ToString(s)); });
EM_JS(int, js_key, (int peek, int at_cmd), { return Module.nh.key(peek, at_cmd); });
EM_ASYNC_JS(void, js_end, (void), { await Module.nh.end(); });

static void tadd(const char *, ...) PRINTF_F(1, 2);

static void
tadd(const char *fmt, ...)
{
    va_list ap;
    int n;

    for (;;) {
        va_start(ap, fmt);
        n = vsnprintf(tbuf ? tbuf + tlen : 0, tcap - tlen, fmt, ap);
        va_end(ap);
        if (tlen + n < tcap) {
            tlen += n;
            return;
        }
        tcap = (tlen + n + 1) * 2;
        tbuf = (char *) realloc(tbuf, tcap);
    }
}

static int
cidx(int attr, int clr)
{
    if (clr < 0 || clr >= CLR_MAX || clr == NO_COLOR)
        return attr == ATR_BOLD ? CLR_WHITE : CLR_GRAY;
    return clr;
}

static void
redraw(void)
{
    int x, y, i, n;

    if (!iflags.window_inited)
        return;
    for (y = 0; y < ROWNO; y++)
        for (x = 0; x < COLNO; x++) {
            cells[y * COLNO + x] = mapt[y][x];
            chars[y * COLNO + x] = mapc[y][x];
        }
    js_map(cells, chars, u.ux, u.uy, u.uz.dnum * 100 + u.uz.dlevel);
    js_text(0, promptbuf);

    tlen = 0, tadd("%s", "");
    for (i = 0; i < 2; i++)
        if (statlines[i][0])
            tadd("%s\n", statlines[i]);
    js_text(1, tbuf);

    tlen = 0, tadd("%s", "");
    for (i = 0; i < nperm; i++)
        tadd("%d\t%c\t%d\t%d\t%d\t%s\n", perm[i].tile,
             perm[i].ch ? perm[i].ch : ' ', perm[i].id.a_void ? 0 : 2,
             perm[i].id.a_void ? cidx(perm[i].attr, perm[i].clr) : CLR_YELLOW,
             perm[i].sym, perm[i].s);
    js_text(2, tbuf);

    tlen = 0, tadd("%s", "");
    if (popup >= 0) { /* first row: top, cursor, prompt */
        struct nhw *w = &wins[popup];

        tadd("%d\t%d\t%s\n", pop_top, pop_cur, w->prompt ? w->prompt : "");
        n = w->nitems ? w->nitems : w->nlines;
        for (i = 0; i < n; i++)
            if (w->nitems) {
                struct mitem *m = &w->items[i];

                tadd("%d\t%c\t%d\t%d\t%d\t%s\n", m->tile, m->ch ? m->ch : ' ',
                     m->id.a_void ? m->sel : 2,
                     m->id.a_void ? cidx(m->attr, m->clr) : CLR_YELLOW,
                     m->sym, m->s);
            } else
                tadd("-1\t \t2\t%d\t0\t%s\n",
                     cidx(w->lines[i].attr, w->lines[i].clr),
                     w->lines[i].s ? w->lines[i].s : "");
    }
    js_text(3, tbuf);
}

/* returns a key, or 0 for a map click (mouse_* set).  From JS: ASCII,
 * 0x101.. arrows/Home/PgUp/End/PgDn, 0x10000|y<<8|x map click (0x8000 =
 * right button), 0x20000|row click on a pop-up row */
static int
getkey(boolean want_mouse)
{
    int k;

    redraw();
    for (;;) {
        boolean np = iflags.num_pad;

        if ((k = js_key(0, popup < 0 && !promptbuf[0])) < 0) {
            emscripten_sleep(15);
            continue;
        }
        if (k & 0x20000) {
            int i = k & 0xffff;

            if (popup >= 0 && i < wins[popup].nitems
                && wins[popup].items[i].id.a_void) {
                pop_cur = i;
                return POP_CLICK;
            }
            continue;
        }
        if (k & 0x10000) {
            if (!want_mouse || popup >= 0)
                continue;
            mouse_x = k & 0xff, mouse_y = (k >> 8) & 0x7f;
            mouse_btn = (k & 0x8000) ? CLICK_2 : CLICK_1;
            return 0;
        }
        switch (k) {
        case 0x101: return np ? '8' : 'k';
        case 0x102: return np ? '2' : 'j';
        case 0x103: return np ? '4' : 'h';
        case 0x104: return np ? '6' : 'l';
        case 0x105: return np ? '7' : 'y';
        case 0x106: return np ? '9' : 'u';
        case 0x107: return np ? '1' : 'b';
        case 0x108: return np ? '3' : 'n';
        }
        return k;
    }
}

/* ---------- window procs ---------- */

static void
web_init_nhwindows(argcp, argv)
int *argcp UNUSED;
char **argv UNUSED;
{
    int x, y;

    for (y = 0; y < ROWNO; y++)
        for (x = 0; x < COLNO; x++)
            mapt[y][x] = -1, mapc[y][x] = ' ';
    iflags.window_inited = TRUE;
    iflags.perm_invent = TRUE;
    redraw();
}

/* one menu of the choices still valid; returns the index, or -1 = quit */
static int
pick(title, n, names, ok)
const char *title;
int n;
const char *(*names)(int);
boolean (*ok)(int);
{
    winid w = web_create_nhwindow(NHW_MENU);
    menu_item *sel;
    anything any;
    int i, r = -2, cnt = 0, last = -1;

    web_start_menu(w);
    any = zeroany;
    for (i = 0; i < n; i++)
        if (ok(i)) {
            any.a_int = i + 1;
            web_add_menu(w, NO_GLYPH, &any, 0, 0, ATR_NONE, names(i), FALSE);
            cnt++, last = i;
        }
    any.a_int = -1;
    web_add_menu(w, NO_GLYPH, &any, '*', 0, ATR_NONE, "Random", FALSE);
    web_end_menu(w, title);
    if (cnt == 1) { /* only one choice: no question */
        web_destroy_nhwindow(w);
        return last;
    }
    if (web_select_menu(w, PICK_ONE, &sel) > 0) {
        r = sel[0].item.a_int;
        free((genericptr_t) sel);
        r = (r < 0) ? -3 : r - 1;
    } else
        r = -1;
    web_destroy_nhwindow(w);
    return r;
}

static const char *rolen(int i) { return roles[i].name.m; }
static const char *racen(int i) { return races[i].noun; }
static const char *gendn(int i) { return genders[i].adj; }
static const char *alignn(int i) { return aligns[i].adj; }
static boolean okrole(int i) { return ok_role(i, flags.initrace, flags.initgend, flags.initalign); }
static boolean okrace(int i) { return ok_race(flags.initrole, i, flags.initgend, flags.initalign); }
static boolean okgend(int i) { return ok_gend(flags.initrole, flags.initrace, i, flags.initalign); }
static boolean okalign(int i) { return ok_align(flags.initrole, flags.initrace, flags.initgend, i); }

static void
web_player_selection()
{
    int r, nr = 0, ng = ROLE_GENDERS, na = ROLE_ALIGNS;

    while (races[nr].noun)
        nr++;
    rigid_role_checks();
    if (flags.initrole < 0) {
        int n = 0;

        while (roles[n].name.m)
            n++;
        r = pick("Pick a role or profession", n, rolen, okrole);
        if (r == -1)
            goto quit;
        flags.initrole = (r == -3) ? pick_role(flags.initrace, flags.initgend, flags.initalign, PICK_RANDOM) : r;
    }
    rigid_role_checks();
    if (flags.initrace < 0) {
        r = pick("Pick a race or species", nr, racen, okrace);
        if (r == -1)
            goto quit;
        flags.initrace = (r == -3) ? pick_race(flags.initrole, flags.initgend, flags.initalign, PICK_RANDOM) : r;
    }
    rigid_role_checks();
    if (flags.initgend < 0) {
        r = pick("Pick a gender or sex", ng, gendn, okgend);
        if (r == -1)
            goto quit;
        flags.initgend = (r == -3) ? pick_gend(flags.initrole, flags.initrace, flags.initalign, PICK_RANDOM) : r;
    }
    rigid_role_checks();
    if (flags.initalign < 0) {
        r = pick("Pick an alignment or creed", na, alignn, okalign);
        if (r == -1)
            goto quit;
        flags.initalign = (r == -3) ? pick_align(flags.initrole, flags.initrace, flags.initgend, PICK_RANDOM) : r;
    }
    return;
 quit:
    clearlocks();
    web_exit_nhwindows((char *) 0);
    nh_terminate(EXIT_SUCCESS);
}

static void
web_display_file(fname, complain)
const char *fname;
boolean complain;
{
    dlb *f = dlb_fopen(fname, RDTMODE);
    char buf[BUFSZ];
    winid w;

    if (!f) {
        if (complain)
            pline("Cannot open \"%s\".", fname);
        return;
    }
    w = web_create_nhwindow(NHW_TEXT);
    while (dlb_fgets(buf, BUFSZ, f)) {
        char *p = index(buf, '\n');

        if (p)
            *p = 0;
        web_putstr(w, ATR_NONE, buf);
    }
    (void) dlb_fclose(f);
    web_display_nhwindow(w, TRUE);
    web_destroy_nhwindow(w);
}

static void web_getlin(const char *, char *);

static void
web_askname()
{
    char buf[BUFSZ];

    do {
        web_getlin("Who are you?", buf);
    } while (!*buf);
    if (*buf == '\033')
        nh_terminate(EXIT_SUCCESS);
    (void) strncpy(plname, buf, sizeof plname - 1);
}

static void
web_get_nh_event()
{
    static double last;

    if (emscripten_get_now() - last > 50) { /* let the page paint */
        last = emscripten_get_now();
        redraw();
        emscripten_sleep(0);
    }
}

static void
web_exit_nhwindows(str)
const char *str;
{
    if (str && *str)
        raw_print(str);
    if (iflags.window_inited)
        js_end(); /* sync IndexedDB before the runtime goes */
    iflags.window_inited = FALSE;
}

static void
web_suspend_nhwindows(str)
const char *str UNUSED;
{
}

static void
web_resume_nhwindows()
{
}

static void
freewin(w)
struct nhw *w;
{
    int i;

    for (i = 0; i < w->nlines; i++)
        free(w->lines[i].s);
    for (i = 0; i < w->nitems; i++)
        free(w->items[i].s);
    free(w->lines);
    free(w->items);
    free(w->prompt);
    w->lines = 0, w->items = 0, w->prompt = 0;
    w->nlines = w->nitems = w->cury = 0;
}

static winid
web_create_nhwindow(type)
int type;
{
    winid i;

    for (i = 0; i < MAXWIN; i++)
        if (!wins[i].used) {
            memset(&wins[i], 0, sizeof wins[i]);
            wins[i].used = TRUE;
            wins[i].type = type;
            return i;
        }
    panic("web: out of windows");
    return WIN_ERR;
}

static void
web_clear_nhwindow(w)
winid w;
{
    int x, y;

    if (w < 0 || w >= MAXWIN)
        return;
    switch (wins[w].type) {
    case NHW_MESSAGE:
        js_text(5, "");
        break;
    case NHW_MAP:
        for (y = 0; y < ROWNO; y++)
            for (x = 0; x < COLNO; x++)
                mapt[y][x] = -1, mapc[y][x] = ' ';
        break;
    case NHW_STATUS:
        break;
    default:
        freewin(&wins[w]);
    }
}

/* show a text/menu window floating; wait for dismissal */
static void
show_text(w)
winid w;
{
    int n = wins[w].nlines, rows = POP_ROWS - (wins[w].prompt ? 1 : 0), k;

    popup = w, pop_top = 0, pop_cur = -1, pop_any = FALSE;
    for (;;) {
        k = getkey(FALSE);
        if ((k == ' ' || k == '>' || k == 'j' || k == '2') && pop_top + rows < n)
            pop_top += (k == ' ' || k == '>') ? rows : 1;
        else if ((k == '<' || k == 'k' || k == '8') && pop_top > 0)
            pop_top -= (k == '<') ? min(rows, pop_top) : 1;
        else
            break;
    }
    popup = -1;
}

static void
web_display_nhwindow(w, blocking)
winid w;
boolean blocking UNUSED;
{
    if (w < 0 || w >= MAXWIN)
        return;
    if (w == WIN_INVEN)
        return;
    if ((wins[w].type == NHW_TEXT || wins[w].type == NHW_MENU)
        && wins[w].nlines)
        show_text(w);
    else
        redraw();
}

static void
web_destroy_nhwindow(w)
winid w;
{
    if (w < 0 || w >= MAXWIN)
        return;
    freewin(&wins[w]);
    wins[w].used = FALSE;
}

static void
web_curs(w, x, y)
winid w;
int x UNUSED, y;
{
    if (w >= 0 && w < MAXWIN)
        wins[w].cury = y;
}

static void
addline(w, at, attr, s)
struct nhw *w;
int at, attr;
const char *s;
{
    if (at >= w->nlines) {
        w->lines = (struct line *) realloc(w->lines,
                                           (at + 1) * sizeof *w->lines);
        memset(w->lines + w->nlines, 0,
               (at + 1 - w->nlines) * sizeof *w->lines);
        w->nlines = at + 1;
    }
    free(w->lines[at].s);
    w->lines[at].s = dupstr(s);
    w->lines[at].attr = attr;
    w->lines[at].clr = NO_COLOR;
}

static void
web_putstr(w, attr, s)
winid w;
int attr;
const char *s;
{
    struct nhw *p;

    if (w < 0 || w >= MAXWIN)
        return;
    p = &wins[w];
    switch (p->type) {
    case NHW_MESSAGE:
        /* a repeat of the newest message becomes "message (xN)" */
        if (nhist && !strcmp(s, hist_prev)) {
            char fold[BUFSZ + 16];

            Sprintf(fold, "%s (x%d)", s, ++hist_reps);
            free(hist[(nhist - 1) % HIST_MAX]);
            hist[(nhist - 1) % HIST_MAX] = dupstr(fold);
            js_text(6, fold);
            redraw();
            break;
        }
        Strcpy(hist_prev, s);
        hist_reps = 1;
        free(hist[nhist % HIST_MAX]);
        hist[nhist % HIST_MAX] = dupstr(s);
        js_text(4, s);
        nhist++;
        redraw();
        break;
    case NHW_STATUS:
        break;
    default:
        addline(p, p->nlines, attr, s);
    }
}

static void
web_start_menu(w)
winid w;
{
    if (w >= 0 && w < MAXWIN)
        freewin(&wins[w]);
}

static void
web_add_menu(w, glyph, id, ch, gch, attr, str, preselected)
winid w;
int glyph;
const anything *id;
char ch, gch;
int attr;
const char *str;
boolean preselected;
{
    struct nhw *p = &wins[w];
    struct mitem *m;
    int clr = NO_COLOR, at = attr;

    p->items = (struct mitem *) realloc(p->items,
                                        (p->nitems + 1) * sizeof *p->items);
    m = &p->items[p->nitems++];
    m->id = *id;
    m->ch = ch, m->gch = gch;
    if (iflags.use_menu_color && id->a_void)
        (void) get_menu_coloring(str, &clr, &at);
    m->attr = at, m->clr = clr;
    m->tile = -1, m->sym = 0;
    if (glyph != NO_GLYPH && glyph >= 0 && glyph < MAX_GLYPH) {
        int ch2, co;
        unsigned sp;

        m->tile = glyph2tile[glyph];
        (void) mapglyph(glyph, &ch2, &co, &sp, 0, 0, 0);
        m->sym = ch2 & 0xff;
        if (m->clr == NO_COLOR)
            m->clr = co;
    }
    m->s = dupstr(str);
    m->sel = preselected;
}

static void
web_end_menu(w, prompt)
winid w;
const char *prompt;
{
    struct nhw *p = &wins[w];
    int i;
    char next = 'a';

    free(p->prompt);
    p->prompt = prompt ? dupstr(prompt) : 0;
    for (i = 0; i < p->nitems; i++)
        if (p->items[i].gch)
            next = 0;
    for (i = 0; i < p->nitems && next; i++)
        if (p->items[i].id.a_void && !p->items[i].ch) {
            p->items[i].ch = next;
            next = next == 'z' ? 'A' : next == 'Z' ? 0 : next + 1;
        }
}

static int
web_select_menu(w, how, sel)
winid w;
int how;
menu_item **sel;
{
    struct nhw *p = &wins[w];
    int i, k, n, rows;

    *sel = 0;
    if (w == WIN_INVEN) { /* copy into the inventory pane */
        for (i = 0; i < nperm; i++)
            free(perm[i].s);
        free(perm);
        perm = (struct mitem *) malloc((p->nitems + 1) * sizeof *perm);
        nperm = p->nitems;
        for (i = 0; i < nperm; i++) {
            perm[i] = p->items[i];
            perm[i].s = dupstr(p->items[i].s);
        }
        return 0;
    }
    rows = POP_ROWS - (p->prompt ? 1 : 0);
    popup = w, pop_top = 0, pop_cur = -1, pop_any = how == PICK_ANY;
    for (i = 0; i < p->nitems; i++)
        if (p->items[i].id.a_void) {
            pop_cur = i;
            break;
        }
    for (;;) {
        if (pop_cur >= 0) {
            if (pop_cur < pop_top)
                pop_top = pop_cur;
            if (pop_cur >= pop_top + rows)
                pop_top = pop_cur - rows + 1;
        }
        k = getkey(FALSE);
        for (i = 0; i < p->nitems && p->items[i].ch != k; i++)
            ;
        if (i < p->nitems) /* a real accelerator wins over numpad keys */
            ;
        else if (k == '\r' || k == '5')
            k = '\n';
        else if (k == '0' || (k == '.' && how != PICK_ANY))
            k = '\033';
        if (k == '\033') {
            popup = -1;
            return -1;
        }
        if (how == PICK_NONE) {
            if ((k == ' ' || k == '>') && pop_top + rows < p->nitems)
                pop_top += rows;
            else if (k == '<')
                pop_top = max(0, pop_top - rows);
            else
                break;
            continue;
        }
        if (k == '\n' || (k == ' ' && how == PICK_ONE)) {
            if (how == PICK_ONE && pop_cur >= 0) {
                for (i = 0; i < p->nitems; i++)
                    p->items[i].sel = FALSE;
                p->items[pop_cur].sel = TRUE;
            }
            break;
        }
        if (k == 'j' || k == '2' || k == 'k' || k == '8') {
            int d = (k == 'j' || k == '2') ? 1 : -1;

            for (i = pop_cur + d; i >= 0 && i < p->nitems; i += d)
                if (p->items[i].id.a_void) {
                    pop_cur = i;
                    break;
                }
            continue;
        }
        if (k == ' ' && pop_cur >= 0) {
            p->items[pop_cur].sel = !p->items[pop_cur].sel;
            continue;
        }
        if (how == PICK_ANY && (k == ',' || k == '@' || k == '-')) {
            for (i = 0; i < p->nitems; i++)
                if (p->items[i].id.a_void)
                    p->items[i].sel = (k != '-');
            continue;
        }
        for (i = 0; i < p->nitems; i++)
            if (p->items[i].id.a_void
                && (p->items[i].ch == k || p->items[i].gch == k)) {
                if (how == PICK_ONE) {
                    int j;

                    for (j = 0; j < p->nitems; j++)
                        p->items[j].sel = FALSE;
                    p->items[i].sel = TRUE;
                    popup = -1;
                    goto done;
                }
                p->items[i].sel = !p->items[i].sel;
                pop_cur = i;
            }
    }
    popup = -1;
 done:
    n = 0;
    for (i = 0; i < p->nitems; i++)
        if (p->items[i].sel && p->items[i].id.a_void)
            n++;
    if (n) {
        *sel = (menu_item *) alloc(n * sizeof **sel);
        n = 0;
        for (i = 0; i < p->nitems; i++)
            if (p->items[i].sel && p->items[i].id.a_void) {
                (*sel)[n].item = p->items[i].id;
                (*sel)[n].count = -1L;
                n++;
            }
    }
    return n;
}

static void
web_update_inventory()
{
    if (!iflags.perm_invent || perm_building || WIN_INVEN == WIN_ERR)
        return;
    perm_building = TRUE;
    (void) display_inventory((char *) 0, FALSE);
    perm_building = FALSE;
    redraw();
}

static void
web_mark_synch()
{
    redraw();
}

static void
web_wait_synch()
{
    redraw();
}

static void
web_print_glyph(w, x, y, glyph, bkglyph)
winid w UNUSED;
xchar x, y;
int glyph, bkglyph UNUSED;
{
    int ch, co;
    unsigned sp;

    if (x < 0 || x >= COLNO || y < 0 || y >= ROWNO)
        return;
    (void) mapglyph(glyph, &ch, &co, &sp, x, y, 0);
    mapt[y][x] = glyph2tile[glyph];
    mapc[y][x] = (ch & 0xff) | cidx(0, co) << 8;
}

static void
web_raw_print(s)
const char *s;
{
    fprintf(stderr, "%s\n", s);
    if (iflags.window_inited && WIN_MESSAGE != WIN_ERR)
        web_putstr(WIN_MESSAGE, 0, s);
}

static int
web_nhgetch()
{
    return getkey(FALSE);
}

static int
web_nh_poskey(x, y, mod)
int *x, *y, *mod;
{
    int k = getkey(TRUE);

    if (!k)
        *x = mouse_x, *y = mouse_y, *mod = mouse_btn;
    return k;
}

static void
web_nhbell()
{
}

static int
web_doprev_message()
{
    winid w = web_create_nhwindow(NHW_TEXT);
    int i;

    for (i = max(0, nhist - HIST_MAX); i < nhist; i++)
        web_putstr(w, 0, hist[i % HIST_MAX]);
    show_text(w);
    web_destroy_nhwindow(w);
    return 0;
}

static void
add_hist(s)
const char *s;
{
    if (WIN_MESSAGE != WIN_ERR)
        web_putstr(WIN_MESSAGE, 0, s);
}

static char
web_yn_function(q, resp, def)
const char *q, *resp;
char def;
{
    char c, lo[BUFSZ];
    int k;

    if (resp)
        Sprintf(promptbuf, "%s [%s] ", q, resp);
    else
        Sprintf(promptbuf, "%s ", q);
    if (resp && def)
        Sprintf(eos(promptbuf), "(%c) ", def);
    Strcpy(lo, resp ? resp : "");
    for (;;) {
        k = getkey(FALSE);
        c = (char) k;
        if (!resp)
            break;
        if (k == '\033') {
            c = strchr(lo, 'q') ? 'q' : strchr(lo, 'n') ? 'n' : def;
            break;
        }
        if ((k == '\n' || k == '\r' || k == ' ') && def) {
            c = def;
            break;
        }
        if (strchr(lo, c))
            break;
        if (strchr(lo, lowc(c))) {
            c = lowc(c);
            break;
        }
    }
    {
        char done[BUFSZ * 2 + 8];

        Sprintf(done, "%s%c", promptbuf, (c == '\033') ? ' ' : c);
        *promptbuf = 0;
        add_hist(done);
    }
    return c;
}

static void
web_getlin(q, buf)
const char *q;
char *buf;
{
    int n = 0, k;

    buf[0] = 0;
    for (;;) {
        Sprintf(promptbuf, "%s %s_", q, buf);
        k = getkey(FALSE);
        if (k == '\033') {
            Strcpy(buf, "\033");
            break;
        }
        if (k == '\n' || k == '\r')
            break;
        if ((k == '\b' || k == 127) && n > 0)
            buf[--n] = 0;
        else if (k >= ' ' && k < 127 && n < BUFSZ - 1)
            buf[n++] = (char) k, buf[n] = 0;
    }
    *promptbuf = 0;
    if (*buf != '\033') {
        char done[BUFSZ * 2 + 2];

        Sprintf(done, "%s %s", q, buf);
        add_hist(done);
    }
}

/* extended commands: type the name, Enter (tty's autocomplete is not needed) */
static int
web_get_ext_cmd()
{
    char buf[BUFSZ];
    int i, hit = -1, n = 0;

    web_getlin("#", buf);
    if (!*buf || *buf == '\033')
        return -1;
    for (i = 0; extcmdlist[i].ef_txt; i++)
        if (!strcmpi(buf, extcmdlist[i].ef_txt))
            return i;
    for (i = 0; extcmdlist[i].ef_txt; i++)
        if (!strncmpi(buf, extcmdlist[i].ef_txt, strlen(buf)))
            hit = i, n++;
    if (n == 1)
        return hit;
    pline("%s: %s extended command.", buf, n ? "ambiguous" : "unknown");
    return -1;
}

static void
web_number_pad(state)
int state UNUSED;
{
}

static void
web_delay_output()
{
    redraw();
    emscripten_sleep(30);
}

static void
web_start_screen()
{
}

static void
web_end_screen()
{
}

static void
web_outrip(w, how, when)
winid w;
int how;
time_t when;
{
    genl_outrip(w, how, when);
}

#ifdef CLIPPING
static void
web_cliparound(x, y)
int x UNUSED, y UNUSED;
{
}
#endif

static void
web_status_init()
{
    int i;

    for (i = 0; i < MAXBLSTATS; i++)
        stat_val[i][0] = 0, stat_fmt[i][0] = 0, stat_on[i] = FALSE;
    statlines[0][0] = statlines[1][0] = 0;
}

static void
web_status_finish()
{
}

static void
web_status_enablefield(fieldidx, nm, fmt, enable)
int fieldidx;
const char *nm UNUSED, *fmt;
boolean enable;
{
    if (fieldidx < 0 || fieldidx >= MAXBLSTATS)
        return;
    stat_on[fieldidx] = enable;
    strncpy(stat_fmt[fieldidx], fmt ? fmt : "%s", sizeof stat_fmt[0] - 1);
}

/* fields in tty's order; a break after BL_SCORE starts the second line */
static const int statorder[] = {
    BL_TITLE, BL_STR, BL_DX, BL_CO, BL_IN, BL_WI, BL_CH, BL_ALIGN, BL_SCORE,
    -1,
    BL_LEVELDESC, BL_GOLD, BL_HP, BL_HPMAX, BL_ENE, BL_ENEMAX, BL_AC,
    BL_XP, BL_HD, BL_EXP, BL_TIME, BL_HUNGER, BL_CAP, BL_CONDITION
};

static void
web_status_update(fldidx, ptr, chg, percent, color, colormasks)
int fldidx, chg UNUSED, percent UNUSED, color UNUSED;
genericptr_t ptr;
unsigned long *colormasks UNUSED;
{
    int i, line = 0;
    char *out;

    if (fldidx == BL_RESET || fldidx == BL_FLUSH) {
        statlines[0][0] = statlines[1][0] = 0;
        for (i = 0; i < SIZE(statorder); i++) {
            int f = statorder[i];

            if (f < 0) {
                line = 1;
                continue;
            }
            if (!stat_on[f] || !stat_val[f][0])
                continue;
            out = statlines[line];
            if (out[0])
                Strcat(out, " ");
            Sprintf(eos(out), stat_fmt[f][0] ? stat_fmt[f] : "%s",
                    stat_val[f]);
        }
        for (i = 0; i < 2; i++) { /* \G<glyph> escapes (the $ of the gold) */
            char dec[sizeof statlines[0]];

            (void) decode_mixed(dec, statlines[i]);
            Strcpy(statlines[i], dec);
        }
        return;
    }
    if (fldidx < 0 || fldidx >= MAXBLSTATS)
        return;
    if (fldidx == BL_CONDITION) {
        unsigned long m = ptr ? *(unsigned long *) ptr : 0L;

        stat_val[fldidx][0] = 0;
        for (i = 0; i < 13; i++) { /* valid_conditions[] */
            extern const struct condmap valid_conditions[];

            if (m & valid_conditions[i].bitmask)
                Sprintf(eos(stat_val[fldidx]), "%s%s",
                        stat_val[fldidx][0] ? " " : "",
                        valid_conditions[i].id);
        }
        Strcpy(stat_fmt[fldidx], "%s");
    } else if (ptr) {
        strncpy(stat_val[fldidx], (const char *) ptr, MAXCO - 1);
        stat_val[fldidx][MAXCO - 1] = 0;
    }
}

struct window_procs web_procs = {
    "web",
    (WC_COLOR | WC_HILITE_PET | WC_INVERSE | WC_TILED_MAP | WC_PERM_INVENT
     | WC_MOUSE_SUPPORT),
    (WC2_FLUSH_STATUS),
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    web_init_nhwindows, web_player_selection, web_askname, web_get_nh_event,
    web_exit_nhwindows, web_suspend_nhwindows, web_resume_nhwindows,
    web_create_nhwindow, web_clear_nhwindow, web_display_nhwindow,
    web_destroy_nhwindow, web_curs, web_putstr, genl_putmixed,
    web_display_file, web_start_menu, web_add_menu, web_end_menu,
    web_select_menu, genl_message_menu, web_update_inventory,
    web_mark_synch, web_wait_synch,
#ifdef CLIPPING
    web_cliparound,
#endif
#ifdef POSITIONBAR
    donull,
#endif
    web_print_glyph, web_raw_print, web_raw_print, web_nhgetch,
    web_nh_poskey, web_nhbell, web_doprev_message, web_yn_function,
    web_getlin, web_get_ext_cmd, web_number_pad, web_delay_output,
#ifdef CHANGE_COLOR
    donull, donull,
#endif
    web_start_screen, web_end_screen, web_outrip, genl_preference_update,
    genl_getmsghistory, genl_putmsghistory, web_status_init,
    web_status_finish, web_status_enablefield, web_status_update,
    genl_can_suspend_no,
};

/* Run report (roguelikes-index/server/CONTRACT.md): fire-and-forget GET,
   never throws, offline just fails silently. Negative ints are omitted. */
EM_JS(void, js_beacon, (const char *g, const char *ev, const char *name, const char *killer, int depth, int score, int turns, int lvl), {
    try {
        var p = [['g', UTF8ToString(g)], ['ev', UTF8ToString(ev)], ['name', name ? UTF8ToString(name) : ''],
                 ['killer', killer ? UTF8ToString(killer) : ''], ['depth', depth], ['score', score], ['turns', turns], ['lvl', lvl]];
        var q = p.filter(function (a) { return a[1] !== '' && !(a[1] < 0); })
                 .map(function (a) { return a[0] + '=' + encodeURIComponent(a[1]); }).join('&');
        if (window.RvipWM && RvipWM.report) RvipWM.report(q); else fetch('/roguelikes/beacon?' + q, { keepalive: true, mode: 'no-cors' }).catch(function () {});
    } catch (e) {}
});
/* end.c really_done(), right before topten(): u.urexp is the final score */
void be_run_end(how)
int how;
{
    const char *ev = how == ASCENDED ? "win" : (how == QUIT || how == ESCAPED) ? "quit" : "death";
    const char *k = *ev == 'd' && killer.name[0] ? killer.name : 0;
    if (k && !strncmpi(k, "a ", 2)) k += 2;
    else if (k && !strncmpi(k, "an ", 3)) k += 3;
    else if (k && !strncmpi(k, "the ", 4)) k += 4;
    js_beacon("zeldhack", ev, plname, k, depth(&u.uz), (int) u.urexp, (int) moves, u.ulevel);
}
