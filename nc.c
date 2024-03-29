/*
 * nc.c
 *
 * Copyright (C) 2019, Charles Chiou
 */

#include "tcptun.h"

static int G_ncinit = 0;
static char G_title[256] = "*** TCPTUN ***";
static time_t G_last_sec = 0;

WINDOW *winttl = NULL;
WINDOW *winlog = NULL;
WINDOW *wincon = NULL;

void nc_init(void)
{
    int height, width, starty, startx;

    G_ncinit = 1;

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    /* Create title window */
    height = 1;
    width = COLS;
    startx = 0;
    starty = 0;
    winttl = newwin(height, width, starty, startx);
    mvwprintw(winttl, 0, 0, G_title);
    wrefresh(winttl);

    /* Create log window */
    height = LINES - 12 - 1 ;
    width = COLS;
    starty = 12 + 1;
    startx = 0;
    winlog = newwin(height, width, starty, startx);
    wsetscrreg(winlog, 1, height - 1);
    scrollok(winlog, TRUE);
    idlok(winlog, TRUE);
#if defined(__SUNPRO_C)
    wborder(winlog, '|', '|', '-', '-', '+', '+', '+', '+');
#else
    box(winlog, 0, 0);
#endif
    mvwprintw(winlog, 0, 2, "Log");
    wrefresh(winlog);

    /* Create con window */
    height = 12;
    width = COLS;
    starty = 1;
    startx = 0;
    wincon = newwin(height, width, starty, startx);
    wsetscrreg(wincon, 1, height - 1);
    scrollok(wincon, TRUE);
    idlok(wincon, TRUE);
#if defined(__SUNPRO_C)
    wborder(wincon, '|', '|', '-', '-', '+', '+', '+', '+');
#else
    box(wincon, 0, 0);
#endif
    mvwprintw(wincon, 0, 2, "Connections");
    wrefresh(wincon);
}

void nc_set_title(const char *title)
{
    if (title == NULL)
        return;

    strncpy(G_title, title, sizeof(G_title) - 1);
    if (winttl) {
        mvwprintw(winttl, 0, 0, G_title);
        wrefresh(winttl);
    }
}

void nc_log(const char *format, ...)
{
    va_list ap;
    char buf[1024];
    time_t t;
    struct tm *tm;
    size_t len = 0;

    if (format == NULL) {
        return;
    }

    t = time(NULL);
    tm = localtime(&t);
    if (tm != NULL) {
        len = strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S - ", tm);
    }

    va_start(ap, format);
    vsnprintf(buf + len, sizeof(buf) - len - 1, format, ap);
    va_end(ap);

    if (winlog == NULL) {
        fprintf(stderr, "%s", buf);
    } else {
        int maxy = getmaxy(winlog);
        int cury = getcury(winlog);

        if (cury == 0) {
            cury = 1;
        } else if (cury > (maxy - 1)) {
            cury = (maxy - 1);
        }

        if (format) {
            mvwprintw(winlog, cury, 1, buf);
        }

        cury = getcury(winlog);
#if defined(__SUNPRO_C)
        wborder(winlog, '|', '|', '-', '-', '+', '+', '+', '+');
#else
        box(winlog, 0, 0);
#endif
        mvwprintw(winlog, 0, 2, "Log");
        wmove(winlog, cury, 1);
        wrefresh(winlog);
    }
}

void nc_refresh(const struct pair pairs[], unsigned int npairs)
{
    unsigned int i, instance = 0;
    char timestr[64];
    char instr[64];
    struct timeval timeval;
    time_t conntime, secs, mins, hours, days;


    if (pairs == NULL || wincon == NULL)
        goto done;


    gettimeofday(&timeval, NULL);
    werase(wincon);

    for (i = 0; i < npairs; i++) {
        if (pairs[i].in_sock <= 0)
            continue;

        conntime = timeval.tv_sec - pairs[i].tod_sec;
        secs = conntime % 60;
        mins = conntime / 60;
        mins %= 60;
        hours = conntime / 3600;
        hours %= 24;
        days = conntime / 86400;

        if (days > 0) {
            sprintf(timestr, "%lud:%.2lu:%.2lu:%.2lu",
                    days, hours, mins, secs);
        } else {
            sprintf(timestr, "%.2lu:%.2lu:%.2lu", hours, mins, secs);
        }
        strcpy(instr, inet_ntoa(pairs[i].in_addr.sin_addr));
        mvwprintw(wincon,
                  1 + instance, 0,
                  "%2d %s %s %llu/%llu",
                  i,
                  timestr,
                  instr,
                  pairs[i].inbytes,
                  pairs[i].outbytes);
        instance++;
    }

#if defined(__SUNPRO_C)
    wborder(wincon, '|', '|', '-', '-', '+', '+', '+', '+');
#else
    box(wincon, 0, 0);
#endif
    mvwprintw(wincon, 0, 2, "Connections");

done:

    if (timeval.tv_sec != G_last_sec) {
        wrefresh(wincon);
        G_last_sec = timeval.tv_sec;
    }
}

void nc_cleanup(void)
{
    if (G_ncinit) {
        endwin();
        G_ncinit = 0;
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
