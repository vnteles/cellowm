#include "ewmh.h"
#include "cello.h"

#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include "utils.h"
#include "log.h"

xcb_ewmh_connection_t * ewmh;
xcb_window_t ewmh_w;

void ewmh_init(xcb_connection_t * conn) {
    NLOG("Initializing ewmh");
    ewmh = ucalloc(1, sizeof(xcb_ewmh_connection_t));

    xcb_intern_atom_cookie_t *cookie = xcb_ewmh_init_atoms(conn, ewmh);

    if(!xcb_ewmh_init_atoms_replies(ewmh, cookie, (void *)0)){
        CRITICAL("{!} Cannot initiate ewmh atoms");
    }
}

void ewmh_create_ewmh_window(xcb_connection_t * conn, int scrno) {
    ewmh_w = xcb_generate_id(conn);

    /*Don't know if it is necessary*/
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        ewmh_w, root_screen->root,
        -1, -1, 1, 1, 0,
        XCB_WINDOW_CLASS_INPUT_ONLY,
        XCB_COPY_FROM_PARENT,
        XCB_CW_OVERRIDE_REDIRECT,
        (uint32_t[]){1}
    );

    xcb_ewmh_set_wm_name(ewmh, ewmh_w, WMNAMELEN, WMNAME);
    xcb_ewmh_set_supporting_wm_check(ewmh, root_screen->root, ewmh_w);

    xcb_ewmh_set_wm_name(ewmh, root_screen->root, WMNAMELEN, WMNAME);
    xcb_ewmh_set_wm_pid(ewmh, root_screen->root, getpid());
    xcb_ewmh_set_supporting_wm_check(ewmh, root_screen->root, root_screen->root);


    xcb_atom_t supported[] = {
        #define xmacro(ATOM) ewmh->ATOM,
            #include "atoms.xmacro"
        #undef xmacro
    };

    xcb_ewmh_set_supported(
        ewmh, scrno,
        sizeof(supported) / sizeof(*supported),
        supported
    );


    xcb_map_window(conn, ewmh_w);
    xcb_configure_window(conn, ewmh_w, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){XCB_STACK_MODE_BELOW});

    xcb_flush(conn);
}

void ewmh_change_desktop_number(uint8_t scrno, uint32_t desktop_no){
    xcb_ewmh_set_number_of_desktops(ewmh, scrno, desktop_no);
}


void ewmh_change_to_desktop(int scrno, uint32_t desktop){
    if (desktop >= MAX_DESKTOPS) return;

    xcb_ewmh_set_current_desktop(ewmh, scrno, desktop);
}


void ewmh_create_desktops(int scrno, uint32_t desktop_no) {
    ewmh_change_desktop_number(scrno, desktop_no);
    ewmh_change_to_desktop(scrno, 0);
}


bool ewmh_is_special_window(xcb_window_t win) {
    xcb_ewmh_get_atoms_reply_t wt;
	xcb_atom_t atom;

    unsigned int i = 0;

    xcb_get_property_cookie_t cprops = xcb_ewmh_get_wm_window_type(ewmh, win);


    if ( xcb_ewmh_get_wm_window_type_reply(ewmh, cprops, &wt, NULL) == 1 ){

        #define is_special ( \
            atom == ewmh->_NET_WM_WINDOW_TYPE_DOCK    ||\
            atom == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP ||\
            atom == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR   \
        )

        for (i = 0; i < wt.atoms_len; i++){
            atom = wt.atoms[i];
            if is_special return true;
        }

        #undef is_special

        xcb_ewmh_get_atoms_reply_wipe(&wt);
    }

    return false;
}

bool ewmh_check_strut(xcb_window_t wid) {
    puts("hmmm");
    xcb_ewmh_wm_strut_partial_t struts;
    bool changed = false;
	if (xcb_ewmh_get_wm_strut_partial_reply(ewmh, xcb_ewmh_get_wm_strut_partial(ewmh, wid), &struts, NULL) == 1) {
            int32_t 
                wid = root_screen->width_in_pixels, 
                hei = root_screen->height_in_pixels;

			if (
                (int16_t)struts.left > 0 &&
                (int16_t)struts.left < (wid - 1) &&
                (int16_t)struts.left_end_y >= 0 &&
                (int16_t)struts.left_start_y < hei 
            ) {
				int dx = struts.left;
				printf("new padding left: %d\n", dx);
				changed = true;
			} if ((wid) > (int16_t)(wid - struts.right) &&
			    (int16_t)(wid - struts.right) > 0 &&
			    (int16_t)struts.right_end_y >= 0 &&
			    (int16_t)struts.right_start_y < (hei)
            ) {
                int dx = (wid) - wid + struts.right;
                printf("new padding right: %d\n", dx);
				changed = true;
			} if (0 < (int16_t) struts.top &&
			    (int16_t)struts.top < (hei - 1) &&
			    (int16_t)struts.top_end_x >= 0 &&
			    (int16_t)struts.top_start_x < (0 + wid)) {
				int dy = struts.top - 0;
				
                printf("new padding top: %d\n", dy);

				changed = true;
			}
			if ((0 + hei) > (int16_t)(hei - struts.bottom) &&
			    (int16_t) (hei - struts.bottom) > 0 &&
			    (int16_t) struts.bottom_end_x >= 0 &&
			    (int16_t) struts.bottom_start_x < (0 + wid)) {
				int dy = (0 + hei) - hei + struts.bottom;
				printf("new padding bottom: %d", dy);

				changed = true;
			}

	}
	return changed;
}