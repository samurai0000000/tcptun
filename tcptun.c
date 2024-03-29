/*
 * tcptun.c
 *
 * Copyright (C) 2019, Charles Chiou
 */

#include "tcptun.h"

#define DEFAULT_INPORT		42022
#define DEFAULT_OUTPORT		42122
#define DEFAULT_OUTHOST		"127.0.0.1"

static int serv_sock = -1;
static struct pair tunpairs[MAX_TUNNELS];
static fd_set fdset;
static int daemonize = 0;
static int debug = 0;
static int inport = DEFAULT_INPORT;
static char outhost[128] = DEFAULT_OUTHOST;
static int outport = DEFAULT_OUTPORT;

static const struct option long_options[] = {
    { "help", no_argument, 0, 'h', },
    { "daemonize", no_argument, 0, 'd', },
    { "debug", no_argument, 0, 'D', },
    { "inport", required_argument, 0, 'I', },
    { "outport", required_argument, 0, 'O', },
    { "outhost", required_argument, 0, 'H', },
    { "dns", required_argument, 0, 'n', },
    { 0, 0, 0, 0, },
};

static void print_help(int argc, char **argv)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
    fprintf(stderr,
            "  --help,-h       This message\n"
            "  --daemonize,-d  run as daemon\n"
            "  --debug,-D      debug mode (no ncurses)\n"
            "  --inport,-I     incoming port\n"
            "  --outport,-O    outgoing port\n"
            "  --outhost,-H    outgoing host\n"
            "  --dns,-n        DNS server address\n");
}

static void sighandler(int signal)
{
    fprintf(stderr, "caught signal %d, exiting\n", signal);
    exit(0);
}

static void cleanup(void)
{
    close(serv_sock);
    serv_sock = -1;
    if (daemonize) {
        fprintf(stderr, "goodbye!\n");
    } else {
        if (debug == 0) {
            nc_cleanup();
        }
    }
}

int main(int argc, char **argv)
{
    struct pair pair;
    int i, rval;
    struct timeval timeout;

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hdDI:O:H:n:",
                            long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'd':
            daemonize = 1;
            break;
        case 'D':
            debug = 1;
            break;
        case 'I':
            inport = atoi(optarg);
            break;
        case 'O':
            outport = atoi(optarg);
            break;
        case 'H':
            strncpy(outhost, optarg, sizeof(outhost) - 1);
            break;
        case 'n':
            rval = tcptun_set_dns(optarg);
            if (rval != 0) {
                fprintf(stderr, "Invalid DNS server address '%s'!\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
        default:
            print_help(argc, argv);
            exit(EXIT_FAILURE);
        }
    }

    if (daemonize) {
        if (daemon(1, 1) != 0) {
            perror("daemon");
            exit(EXIT_FAILURE);
        }
    }

    signal(SIGINT, sighandler);
    signal(SIGKILL, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGPIPE, SIG_IGN);
    atexit(cleanup);

    for (i = 0; i < MAX_TUNNELS; i++) {
        tunpairs[i].in_sock = -1;
        tunpairs[i].out_sock = -1;
    }

    if (debug == 0) {
        nc_init();
    }

    serv_sock = tcptun_bind_listen(inport);
    if (serv_sock < 0) {
        exit(EXIT_FAILURE);
    }

    if (debug == 0) {
        char title[256];
        snprintf(title, sizeof(title) - 1,
                 "%s %d:%s:%d", basename(argv[0]), inport, outhost, outport);
        nc_set_title(title);
    }

    while (serv_sock >= 0) {
        if (daemonize == 0) {
            nc_refresh(tunpairs, MAX_TUNNELS);
        }

        FD_ZERO(&fdset);
        FD_SET(serv_sock, &fdset);
        for (i = 0; i < MAX_TUNNELS; i++) {
            if (tunpairs[i].in_sock >= 0)
                FD_SET(tunpairs[i].in_sock, &fdset);
            if (tunpairs[i].out_sock >= 0)
                FD_SET(tunpairs[i].out_sock, &fdset);
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;
        rval = select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
        if (rval <= 0) {
            continue;
        }

        if (FD_ISSET(serv_sock, &fdset)) {
            if (tcptun_accept(serv_sock, &pair,
                              outhost, outport) == 0) {
                i = tcptun_find_free_pair(tunpairs, MAX_TUNNELS);
                if (i < 0 || i >= MAX_TUNNELS) {
                    nc_log("no free tunnel left!\n");
                    close(pair.in_sock);
                    close(pair.out_sock);
                    continue;
                } else {
                    memcpy(&tunpairs[i], &pair, sizeof(pair));
                    FD_SET(pair.in_sock, &fdset);
                    FD_SET(pair.out_sock, &fdset);
                    continue;
                }
            }
        }

        for (i = 0; i < MAX_TUNNELS; i++) {
            if (tunpairs[i].in_sock >= 0 &&
                FD_ISSET(tunpairs[i].in_sock, &fdset)) {
                tcptun_incoming_process(&tunpairs[i]);
            }
            if (tunpairs[i].out_sock >= 0 &&
                FD_ISSET(tunpairs[i].out_sock, &fdset)) {
                tcptun_outgoing_process(&tunpairs[i]);
            }
        }
    }

    return 0;
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
