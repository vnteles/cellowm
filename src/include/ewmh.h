#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include "utils.h"

extern xcb_ewmh_connection_t * ewmh;
extern xcb_window_t ewmh_w;

void ewmh_init(xcb_connection_t * conn);
void ewmh_create_ewmh_window(xcb_connection_t * conn, int scrno);

void ewmh_change_desktop_number(uint8_t scrno, uint32_t desktop_no);
void ewmh_change_to_desktop(int scrno, uint32_t current);
void ewmh_create_desktops(int scrno, uint32_t desktop_no);

bool ewmh_is_special_window(xcb_window_t win);
bool ewmh_check_strut(xcb_window_t wid);