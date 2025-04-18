/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */
#ifndef __LINK_HWSS_HPO_DP_H__
#define __LINK_HWSS_HPO_DP_H__

#include "link_hwss.h"
#include "link.h"

void set_hpo_dp_throttled_vcp_size(struct pipe_ctx *pipe_ctx,
		struct fixed31_32 throttled_vcp_size);
void set_hpo_dp_hblank_min_symbol_width(struct pipe_ctx *pipe_ctx,
		const struct dc_link_settings *link_settings,
		struct fixed31_32 throttled_vcp_size);
void set_hpo_dp_hblank_min_symbol_width(struct pipe_ctx *pipe_ctx,
		const struct dc_link_settings *link_settings,
		struct fixed31_32 throttled_vcp_size);
void setup_hpo_dp_stream_encoder(struct pipe_ctx *pipe_ctx);
void reset_hpo_dp_stream_encoder(struct pipe_ctx *pipe_ctx);
void setup_hpo_dp_stream_attribute(struct pipe_ctx *pipe_ctx);
void enable_hpo_dp_link_output(struct dc_link *link,
		const struct link_resource *link_res,
		enum amd_signal_type signal,
		enum clock_source_id clock_source,
		const struct dc_link_settings *link_settings);
void disable_hpo_dp_link_output(struct dc_link *link,
		const struct link_resource *link_res,
		enum amd_signal_type signal);
void update_hpo_dp_stream_allocation_table(struct dc_link *link,
		const struct link_resource *link_res,
		const struct link_mst_stream_allocation_table *table);
void setup_hpo_dp_audio_output(struct pipe_ctx *pipe_ctx,
		struct audio_output *audio_output, uint32_t audio_inst);
void enable_hpo_dp_audio_packet(struct pipe_ctx *pipe_ctx);
void disable_hpo_dp_audio_packet(struct pipe_ctx *pipe_ctx);
const struct link_hwss *get_hpo_dp_link_hwss(void);
bool can_use_hpo_dp_link_hwss(const struct dc_link *link,
		const struct link_resource *link_res);


#endif /* __LINK_HWSS_HPO_DP_H__ */
