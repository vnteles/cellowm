#include "message.h"

#include <string.h>

#include "cello.h"
#include "log.h"
#include "xcb.h"

#define comp_str(s1, s2) ( strcmp(s1, s2) == 0 )


/**
 ** @brief handle `desktop.current` message
 ** @param data the unparsed string with the desktop number
 **/
void action_desktop_current(char ** data) {
    if (!data || !data[0]) return;

    int n = strtol(data[0], NULL, 10);

    cello_goto_desktop(n);
}

/**
 ** @brief handle `window.desktop` message
 ** @param data \
    `data[0]` must have the unparsed string with the desktop number \
    `data[1]` must have the unparsed string with the window id
 **/
void action_window_desktop(char ** data) {
    if (!data || !data[0]) return;

    struct window * w;
    int32_t ds;

    ds = strtol(data[0], NULL, 10);

    if (data[1] == NULL || comp_str(data[1], "focused")) {
        w = xcb_get_focused_window();
    } else {
        xcb_drawable_t wid = strtol(data[1], NULL, 16);
        w = find_window_by_id(wid);
    }

    xcb_change_window_ds(w, ds);
}

void action_window_move(char ** data) {
    if (!data || !data[0]) return;

    struct window * w;

    if (!data[1] || comp_str(data[1], "focused")) {
        w = xcb_get_focused_window();
    } else {
        xcb_drawable_t wid = strtol(data[1], NULL, 16);
        w = find_window_by_id(wid);
    }

    if (comp_str(data[0], "center")) {
        center_window(w);
        // puts("aa");
    } else if (comp_str(data[0], "center_x")) {
        center_window_x(w);
    } else if (comp_str(data[0], "center_y")) {
        center_window_y(w);
    } else {
        char * aux = NULL;
        int16_t x = strtol(data[0], &aux, 10);
        int16_t y = x;

        if (aux[0] == ',') {
            aux++;
            y = strtol(aux, NULL, 10);
        }

        xcb_move_window(w, x, y);

    }
}

void action_window_close(char ** data) {
    xcb_drawable_t wid;

    if (!data[0] || comp_str(data[0], "focused")) {
        wid = xcb_get_focused_window()->id;
    } else {
        wid = strtol(data[1], NULL, 16);
        if (find_window_by_id(wid) == NULL) {
            return;
        }
    }

    xcb_close_window(wid, false);
}

/*--- state ---*/

enum ops {
    OP_NONE, OP_SET, OP_UNSET, OP_GET
};

enum stt_kind {
    STT_STT, STT_OPT, STT_ACT,
};

typedef struct msg_state msg_istate;
struct msg_state {
    char * name;
    uint8_t kind;

    union {
        struct {
            void * opt;
            void (* act)(char **);
            int params;
        }s;
        msg_istate * stt;
    } u;
};

#define state_head(n, k) .name = n, .kind = k
#define head_act(n) state_head(n, STT_ACT), .u.s.act =
#define head_opt(n) state_head(n, STT_OPT), .u.s.opt =
#define head_state(n) state_head(n, STT_STT), .u.stt =

struct msg_state state[] = {
    { head_state("desktop") (struct msg_state[]){
        { head_act("current") &action_desktop_current, .u.s.params = 1 },
    }},
    { head_state("window") (struct msg_state[]) {
        { head_act("desktop") &action_window_desktop, .u.s.params = 2 },
        { head_act("move") &action_window_move, .u.s.params = 2 },
        { head_act("close")  &action_window_close, .u.s.params = 1},
    }}
    // ...
};

#undef state_head
#undef head_act
#undef head_opt
#undef head_state

/* -/- state -/- */

static struct msg_state * parse_option(char * opt) {
    char * endptr, * aux;
    int olen, start, end;

    bool apply = false;

    struct msg_state * aux_state = state;

    olen = strlen(opt);
    end = 0;

    aux = opt;
    long unsigned int i;

    for (; opt; start=end, opt+=start, olen-=start-1) {
        endptr = memchr(opt, '.', olen);
        end = endptr ? endptr - opt : olen-1;


        if (end <= 0) break;

        if (!aux_state)
            return NULL;

        for ( i = 0;  aux_state && aux_state[i].name; i++) {

            if (strncmp(opt, aux_state[i].name, end)) continue;

            switch (aux_state[i].kind) {
                case STT_STT:
                    if (!aux_state[i].u.stt)
                        goto err;

                    aux_state = aux_state[i].u.stt;

                    break;
                case STT_ACT:
                    if (!aux_state[i].u.s.act)
                        goto err;

                    apply = true;
                    break;
                case STT_OPT:
                    if (!aux_state[i].u.s.opt)
                        goto err;

                    apply = true;
                    break;
                default:
                err:
                    DLOG("Invalid option: %s", aux);
                    return NULL;
            }

            break;
        }

        if (aux_state->kind == STT_STT) return NULL;
        end++;
    }

    if (!apply) {
        puts("Nothing to do");
        return NULL;
    }
    aux_state = &aux_state[i];
    VARDUMP(aux_state);
    VARDUMP(aux_state->u.s.act);
    return aux_state;
}

void parse_opts(int len, char ** msg) {
    char * action;
    uint8_t opk;

    action = msg[0];
    opk = OP_NONE;



    //--- get the operation ---
    if (comp_str(action, "set")) {
        opk = OP_SET;
        msg++; len--;
    } else if (comp_str(action, "reload")) {
        cello_reload();
        ufree(msg[0]);
        return;
    } else if (comp_str(action, "logout")) {
        ufree(msg[0]);
        exit(0);
    }


    if (len == 0) return;

    //--- get the action ---
    struct msg_state * stt = NULL;
    char * stt_data[4];
    for (int i = 0; i < len; i++) {
        if (msg[i][0] == '-') {
            puts("modifier");
        } else {
            if ((i == len)) {
                // puts("a");
                return;
            }

            // if already have a state, skip
            if (stt)
                continue;

            stt = parse_option(msg[i]);
            VARDUMP(stt);

            if (!stt)
                return;

            if (stt->kind == STT_ACT && opk == OP_NONE) {
                bool set_null = false;
                for (int j = 0; j < stt->u.s.params; j++) {
                    // prevent stack errors
                    if (set_null || msg[++i] == NULL) {
                        stt_data[j] = NULL;
                        set_null = true;
                    }


                    stt_data[j] = msg[i];
                }

                VARDUMP(stt->u.s.act)
                VARDUMP(&action_window_move)
                stt->u.s.act(stt_data);
            }

        }
        ufree(msg[i]);
    }

    //--- execute the action ---



}