/* vorbis_patch.c -- vorbis redirection
 *
 * Copyright (C) 2022 Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <vorbis/vorbisfile.h>

#include "main.h"
#include "so_util.h"

void patch_vorbis(void) {
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis"), (uintptr_t)vorbis_analysis);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis_blockout"), (uintptr_t)vorbis_analysis_blockout);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis_buffer"), (uintptr_t)vorbis_analysis_buffer);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis_headerout"), (uintptr_t)vorbis_analysis_headerout);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis_init"), (uintptr_t)vorbis_analysis_init);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_analysis_wrote"), (uintptr_t)vorbis_analysis_wrote);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_bitrate_addblock"), (uintptr_t)vorbis_bitrate_addblock);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_bitrate_flushpacket"), (uintptr_t)vorbis_bitrate_flushpacket);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_block_clear"), (uintptr_t)vorbis_block_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_block_init"), (uintptr_t)vorbis_block_init);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_add"), (uintptr_t)vorbis_comment_add);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_add_tag"), (uintptr_t)vorbis_comment_add_tag);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_clear"), (uintptr_t)vorbis_comment_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_init"), (uintptr_t)vorbis_comment_init);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_query"), (uintptr_t)vorbis_comment_query);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_comment_query_count"), (uintptr_t)vorbis_comment_query_count);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_commentheader_out"), (uintptr_t)vorbis_commentheader_out);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_dsp_clear"), (uintptr_t)vorbis_dsp_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_info_blocksize"), (uintptr_t)vorbis_info_blocksize);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_info_clear"), (uintptr_t)vorbis_info_clear);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_info_init"), (uintptr_t)vorbis_info_init);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_packet_blocksize"), (uintptr_t)vorbis_packet_blocksize);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis"), (uintptr_t)vorbis_synthesis);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_blockin"), (uintptr_t)vorbis_synthesis_blockin);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_headerin"), (uintptr_t)vorbis_synthesis_headerin);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_init"), (uintptr_t)vorbis_synthesis_init);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_pcmout"), (uintptr_t)vorbis_synthesis_pcmout);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_read"), (uintptr_t)vorbis_synthesis_read);
	hook_addr(so_symbol(&fahrenheit_mod, "vorbis_synthesis_trackonly"), (uintptr_t)vorbis_synthesis_trackonly);
}
