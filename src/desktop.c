
#include <xcb/xcb.h>

#include "cello.h"
#include "utils.h"
#include "types.h"

#include "desktop.h"

struct window_list *dslist[MAX_DESKTOPS];

void unmap_desktop(uint32_t ds) {
    struct window_list *aux;
    for (aux = dslist[ds]; aux; aux = aux->next) {
        xcb_unmap_window(conn, aux->window->id);
    }
}