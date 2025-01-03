/*	$OpenBSD: output-bird.c,v 1.20 2025/01/02 12:29:30 job Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2020 Robert Scheck <robert@fedoraproject.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>

#include "extern.h"

int
output_bird2(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct vsp_tree *vsps, struct stats *st)
{
	extern		const char *bird_tablename;
	struct vrp	*v;
	time_t		 now = get_current_time();

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "\ndefine force_roa_table_update = %lld;\n\n"
	    "roa4 table %s4;\nroa6 table %s6;\n\n"
	    "protocol static {\n\troa4 { table %s4; };\n\n",
	    (long long)now, bird_tablename, bird_tablename,
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV4) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n\nprotocol static {\n\troa6 { table %s6; };\n\n",
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV6) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}

int
output_bird3(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct vsp_tree *vsps, struct stats *st)
{
	extern		const char *bird_tablename;
	struct vrp	*v;
	struct vap	*vap;
	time_t		 now = get_current_time();
	size_t		 i;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "\ndefine force_roa_table_update = %lld;\n\n"
	    "roa4 table %s4;\nroa6 table %s6;\n", (long long)now,
	    bird_tablename, bird_tablename) < 0)
		return -1;

	if (!excludeaspa) {
		if (fprintf(out, "aspa table ASPAS;\n") < 0)
			return -1;
	}

	if (fprintf(out, "\nprotocol static {\n\troa4 { table %s4; };\n\n",
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV4) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n\nprotocol static {\n\troa6 { table %s6; };\n\n",
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV6) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}") < 0)
		return -1;

	if (excludeaspa)
		return 0;

	if (fprintf(out, "\n\nprotocol static {\n\taspa { table ASPAS; "
	    "};\n\n") < 0)
		return -1;

	RB_FOREACH(vap, vap_tree, vaps) {
		if (vap->overflowed)
			continue;
		if (fprintf(out, "\troute aspa %d providers ", vap->custasid) <0)
			return -1;
		for (i = 0; i < vap->num_providers; i++) {
			if (fprintf(out, "%u", vap->providers[i]) < 0)
				return -1;
			if (i + 1 < vap->num_providers)
				if (fprintf(out, ", ") < 0)
					return -1;
		}
		if (fprintf(out, ";\n") < 0)
			return -1;
	}

	if (fprintf(out, "}\n") < 0)
		return -1;

	return 0;
}
