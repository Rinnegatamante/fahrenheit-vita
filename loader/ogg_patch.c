/* ogg_patch.c -- ogg redirection
 *
 * Copyright (C) 2022 Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <ogg/ogg.h>

#include "main.h"
#include "so_util.h"

void patch_ogg(void) {
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_writetrunc"), (uintptr_t)oggpack_writetrunc);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_writeinit"), (uintptr_t)oggpack_writeinit);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_writecopy"), (uintptr_t)oggpack_writecopy);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_writeclear"), (uintptr_t)oggpack_writeclear);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_writealign"), (uintptr_t)oggpack_writealign);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_write"), (uintptr_t)oggpack_write);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_reset"), (uintptr_t)oggpack_reset);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_readinit"), (uintptr_t)oggpack_readinit);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_read1"), (uintptr_t)oggpack_read1);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_read"), (uintptr_t)oggpack_read);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_look1"), (uintptr_t)oggpack_look1);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_look"), (uintptr_t)oggpack_look);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_get_buffer"), (uintptr_t)oggpack_get_buffer);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_bytes"), (uintptr_t)oggpack_bytes);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_bits"), (uintptr_t)oggpack_bits);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_adv1"), (uintptr_t)oggpack_adv1);
	hook_addr(so_symbol(&fahrenheit_mod, "oggpack_adv"), (uintptr_t)oggpack_adv);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_wrote"), (uintptr_t)ogg_sync_wrote);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_reset"), (uintptr_t)ogg_sync_reset);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_pageseek"), (uintptr_t)ogg_sync_pageseek);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_pageout"), (uintptr_t)ogg_sync_pageout);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_init"), (uintptr_t)ogg_sync_init);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_destroy"), (uintptr_t)ogg_sync_destroy);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_clear"), (uintptr_t)ogg_sync_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_sync_buffer"), (uintptr_t)ogg_sync_buffer);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_reset_serialno"), (uintptr_t)ogg_stream_reset_serialno);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_reset"), (uintptr_t)ogg_stream_reset);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_pageout"), (uintptr_t)ogg_stream_pageout);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_pagein"), (uintptr_t)ogg_stream_pagein);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_packetpeek"), (uintptr_t)ogg_stream_packetpeek);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_packetout"), (uintptr_t)ogg_stream_packetout);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_packetin"), (uintptr_t)ogg_stream_packetin);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_init"), (uintptr_t)ogg_stream_init);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_flush"), (uintptr_t)ogg_stream_flush);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_eos"), (uintptr_t)ogg_stream_eos);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_destroy"), (uintptr_t)ogg_stream_destroy);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_stream_clear"), (uintptr_t)ogg_stream_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_version"), (uintptr_t)ogg_page_version);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_serialno"), (uintptr_t)ogg_page_serialno);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_pageno"), (uintptr_t)ogg_page_pageno);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_packets"), (uintptr_t)ogg_page_packets);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_granulepos"), (uintptr_t)ogg_page_granulepos);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_eos"), (uintptr_t)ogg_page_eos);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_continued"), (uintptr_t)ogg_page_continued);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_checksum_set"), (uintptr_t)ogg_page_checksum_set);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_page_bos"), (uintptr_t)ogg_page_bos);
	hook_addr(so_symbol(&fahrenheit_mod, "ogg_packet_clear"), (uintptr_t)ogg_packet_clear);
}
