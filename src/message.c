#include "message.h"

#include <string.h>

#include "cello.h"
#include "log.h"
#include "xcb.h"

// TODO: create a new message parser :/

#define comp_str(s1, s2) ( strcmp(s1, s2) == 0 )

/**
 ** @brief find the given argument in the argument list.
 ** @param arg the argument that you're looking for.
 ** @param argc the length of the argument list.
 ** @param argv the argument list.
 ** @return the position of the argument in the argument list, or -1 if unsuccessfully
 **/
int find_arg(char * arg, int argc, char ** argv) {
    for (int i = 0; i < argc; i++)
        if (argv[i] && comp_str(argv[i], arg)) return i;

    return -1;
}

/**
 ** @brief get the value of the given argument.
 ** @param arg the argument you're looking for.
 ** @param argc the length of the argument list.
 ** @param argv the argument list.
 ** @return a pointer to the value on the argument list, or null if unsuccessfully
 **/
char * get_arg_val(char * arg, int argc, char ** argv) {
    int i = find_arg(arg, argc, argv);
    return i >= 0 && i < argc-1 ?  argv[i+1] : NULL;
}

#define find_opt(opt) (find_arg(opt, opts_len, opts) > -1)


char * get_opt_value(char * opt, char ** opts, int opts_len, char ** aval) {
    char * aux = get_arg_val(opt, opts_len, opts);
    *aval = aux ? aux : *aval;

    return aux;
}

#define get_aval(v) get_opt_value(v, opts, opts_len, &aval)
#define get_val(v, o) get_opt_value(v, opts, opts_len, &o)

/**
 ** @brief parse wm option
 ** @param opts a list with the options
 ** @param opts_len the length of the list
 **/
static void parse_wm(char ** opts, int opts_len) {
    // char * aval = NULL;

    if (find_opt("logout") || find_opt("-l")){
        run = false;
        return;
    }
    if (find_opt("reload") || find_opt("-r")){
        DLOG("Reloading");
        return;
    }
    if (find_opt("hijack") || find_opt("-h")){
        DLOG("Hijacking");
        return;
    }
}

struct window * get_window_arg(bool cached, char ** opts, int opts_len) {
    static struct window * w = NULL;

    if (cached && w) return w;

    char * rwid = NULL;
    if (find_opt("--focused")) {
        w = xcb_get_focused_window();
    } else if (get_val("--id", rwid)) {
        xcb_window_t wid = strtol(rwid, NULL, 16);
        w = find_window_by_id(wid);
    } else {
        w = NULL;
    }

    return w;

}

static void parse_window(char ** opts, int opts_len) {
    char * aval = NULL;

    /*focus on struct window*/
    if (get_aval("--focus") || get_aval("-f")) {
        puts("--focus");
        struct window * w = NULL;
        if (comp_str(aval, "next")) {
            w = next_window();
        } else if (comp_str(aval, "prev")) {
            w = prev_window();
        }

        xcb_focus_window(w);
    }

    /*send window to desktop*/
    if (get_aval("--move-to")) {
        // puts("move-to");
        struct window * w = get_window_arg(false, opts, opts_len);
        if (w) {
            uint32_t ds = (uint32_t) strtol(aval, NULL, 10);

            // char * rwid = NULL;

            if (comp_str(aval, "next")) {
                ds = cello_get_current_desktop() + 1;
            } else if (comp_str(aval, "prev")) {
                ds = cello_get_current_desktop() - 1;
            } else {
                ds = strtol(aval, NULL, 10);
            }

            xcb_change_window_ds(w, ds);
        }
    }

    if (get_aval("--close")) {
        struct window * w = get_window_arg(true, opts, opts_len);
        if (w) {
            xcb_close_window(w->id, true);
        }
    }

    /*list windows*/
    if (find_opt("--list") || find_opt("-l")) {
        //temporary
        {
            for (struct window_list * wl = wilist; wl; wl=wl->next) {
                // todo: send to the client socket
                printf("0x%X ", ((struct window *) wl->window)->id);
            }
            puts("");
        }
    }

}

static void parse_desktop(char ** opts, int opts_len) {
    char * aval = NULL;

    if (get_aval("goto") || get_aval("-g")) {
        uint32_t ds = 0;
        if (comp_str(aval, "next")) {
            ds = cello_get_current_desktop() + 1;
        } else if (comp_str(aval, "prev")) {
            ds = cello_get_current_desktop() - 1;
        } else {
            ds = (uint32_t) strtol(aval, NULL, 10);
        }
        cello_goto_desktop(ds);
    }
}

#undef get_aval
#undef find_opt
void parse_opts(int argc, char ** argv) {
    if (comp_str(argv[0], "wm")) {
        parse_wm(++argv, --argc);
    }
    else if (comp_str(argv[0], "window")) {
        parse_window(++argv, --argc);
    } 
    else if (comp_str(argv[0], "desktop")) {
        parse_desktop(++argv, --argc);
    }

    else {
        DLOG("Unknown option: %s", argv[0]);
    }
}