/*
 * Public include file for the UUID library
 *
 * Copyright (C) 1996, 1997, 1998 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#ifndef _UUID_UUID_H
#define _UUID_UUID_H

#include <sys/types.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <time.h>

typedef unsigned char uuid_t[16];

/* UUID Variant definitions */
#define UUID_VARIANT_NCS 	0
#define UUID_VARIANT_DCE 	1
#define UUID_VARIANT_MICROSOFT	2
#define UUID_VARIANT_OTHER	3

/* UUID Type definitions */
#define UUID_TYPE_DCE_TIME   1
#define UUID_TYPE_DCE_RANDOM 4

/* Allow UUID constants to be defined */
#ifdef __GNUC__
#define UUID_DEFINE(name,u0,u1,u2,u3,u4,u5,u6,u7,u8,u9,u10,u11,u12,u13,u14,u15) \
	static const uuid_t name __attribute__ ((unused)) = {u0,u1,u2,u3,u4,u5,u6,u7,u8,u9,u10,u11,u12,u13,u14,u15}
#else
#define UUID_DEFINE(name,u0,u1,u2,u3,u4,u5,u6,u7,u8,u9,u10,u11,u12,u13,u14,u15) \
	static const uuid_t name = {u0,u1,u2,u3,u4,u5,u6,u7,u8,u9,u10,u11,u12,u13,u14,u15}
#endif

#define UUID_FIELD uuid_t

struct st_tm_val {
  long    tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
};


#ifdef __cplusplus
extern "C" {
#endif


/* clear.c */
void uuid_clear(UUID_FIELD uu);

/* compare.c */
int uuid_compare(const UUID_FIELD uu1, const UUID_FIELD uu2);

/* copy.c */
void uuid_copy(UUID_FIELD dst, const UUID_FIELD src);

/* gen_uuid.c */
void uuid_generate(UUID_FIELD out);
void uuid_generate_random(UUID_FIELD out);
void uuid_generate_time(UUID_FIELD out);

/* isnull.c */
int uuid_is_null(const UUID_FIELD uu);

/* parse.c */
int uuid_parse(const char *in, UUID_FIELD uu);

/* unparse.c */
void uuid_unparse(const UUID_FIELD uu, char *out);
void uuid_unparse_lower(const UUID_FIELD uu, char *out);
void uuid_unparse_upper(const UUID_FIELD uu, char *out);

/* m_uuidime.c */
time_t m_uuidime(const UUID_FIELD uu, struct st_tm_val *ret_tv);
int m_uuidype(const UUID_FIELD uu);
int uuid_variant(const UUID_FIELD uu);

#ifdef __cplusplus
}
#endif

#endif /* _UUID_UUID_H */
