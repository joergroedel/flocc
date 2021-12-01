// SPDX-License-Identifier: GPL-2.0+
/*
 * Fast Lines of Code Counter
 *
 * Copyright (C) 2021 SUSE
 *
 * Author: Jörg Rödel <jroedel@suse.de>
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "md4.h"

#define F(x,y,z) ((((x) & (y)) | ((~(x)) & (z))))
#define G(x,y,z) ((((x) & (y)) | ((x) & (z)) | ((y) & (z))))
#define H(x,y,z) (((x) ^ (y) ^ (z)))
#define R_L(a,s) (((a) << (s)) | ((a) >> (32-(s))))

#define ROUND1(a,b,c,d,xk,s) (a)=R_L(((a)+F((b),(c),(d))+(xk)),(s))
#define ROUND2(a,b,c,d,xk,s) (a)=R_L(((a)+G((b),(c),(d))+(xk)+0x5a827999),(s))
#define ROUND3(a,b,c,d,xk,s) (a)=R_L(((a)+H((b),(c),(d))+(xk)+0x6ed9eba1),(s))

static void do_hash_md4(void *_ctx, const char *buf);

static void __generic_hash_process(void *ctx,
		const char *data, uint32_t data_size,
		char *buf, uint32_t *buf_fill, uint32_t buf_size)
{
	uint32_t start = 0;

	if (*buf_fill > 0) {
		start = buf_size - *buf_fill;
		if (start > data_size) start = data_size;
		memcpy(buf+(*buf_fill), data, start);
		*buf_fill += start;
		if (*buf_fill == buf_size) {
			do_hash_md4(ctx, buf);
			*buf_fill = 0;
		} else return;
	}

	while ((start+buf_size) <= data_size) {
		do_hash_md4(ctx, data+start);
		start += buf_size;
	}

	if (start < data_size) {
		*buf_fill = data_size - start;
		memcpy(buf, data+start, *buf_fill);
	}
}

static void __generic_hash_finish(void *ctx, uint64_t _len,
		char *buf, uint32_t buf_fill, uint32_t buf_size,
		int pad_bytes, int endian)
{
	uint64_t *x = (uint64_t*)buf;
	uint64_t len;
	uint32_t i;

	len = _len*8;

	buf[buf_fill++] = 0x80;
	if (buf_fill > (buf_size-pad_bytes)) {
		for (i=buf_fill;i<buf_size;++i) buf[i] = 0;
		do_hash_md4(ctx, buf);
		memset(buf, 0, buf_size);
		buf_fill = buf_size-pad_bytes;
	} else
		for (i=buf_fill;i<buf_size;++i) buf[i] = 0;
	x[((buf_size/8)-1)] = len;
	do_hash_md4(ctx,buf);
}

static char __int2char(uint32_t c)
{
	if (c < 10) return '0'+c;
	return 'a'+c-10;
}

static void __string_helper32(uint32_t h, char* str)
{
	int i;
	for (i=7;i>=0;--i) str[7-i] = __int2char((h >> (4*i)) & 0x0000000f);
}

static void do_hash_md4(void *_ctx, const char *buf)
{
	struct hash *ctx = (struct hash*)_ctx;
	int i;
	uint32_t x[16];
	uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];

	for (i = 0; i < 16; ++i)
		x[i] = ((uint32_t*)buf)[i];

	/* do the calculations */

	/* Round 1 */
	ROUND1(a,b,c,d,x[0],3);
	ROUND1(d,a,b,c,x[1],7);
	ROUND1(c,d,a,b,x[2],11);
	ROUND1(b,c,d,a,x[3],19);
	ROUND1(a,b,c,d,x[4],3);
	ROUND1(d,a,b,c,x[5],7);
	ROUND1(c,d,a,b,x[6],11);
	ROUND1(b,c,d,a,x[7],19);
	ROUND1(a,b,c,d,x[8],3);
	ROUND1(d,a,b,c,x[9],7);
	ROUND1(c,d,a,b,x[10],11);
	ROUND1(b,c,d,a,x[11],19);
	ROUND1(a,b,c,d,x[12],3);
	ROUND1(d,a,b,c,x[13],7);
	ROUND1(c,d,a,b,x[14],11);
	ROUND1(b,c,d,a,x[15],19);

	/* Round 2 */
	ROUND2(a,b,c,d,x[0],3);
	ROUND2(d,a,b,c,x[4],5);
	ROUND2(c,d,a,b,x[8],9);
	ROUND2(b,c,d,a,x[12],13);
	ROUND2(a,b,c,d,x[1],3);
	ROUND2(d,a,b,c,x[5],5);
	ROUND2(c,d,a,b,x[9],9);
	ROUND2(b,c,d,a,x[13],13);
	ROUND2(a,b,c,d,x[2],3);
	ROUND2(d,a,b,c,x[6],5);
	ROUND2(c,d,a,b,x[10],9);
	ROUND2(b,c,d,a,x[14],13);
	ROUND2(a,b,c,d,x[3],3);
	ROUND2(d,a,b,c,x[7],5);
	ROUND2(c,d,a,b,x[11],9);
	ROUND2(b,c,d,a,x[15],13);

	/* Round 3 */
	ROUND3(a,b,c,d,x[0],3);
	ROUND3(d,a,b,c,x[8],9);
	ROUND3(c,d,a,b,x[4],11);
	ROUND3(b,c,d,a,x[12],15);
	ROUND3(a,b,c,d,x[2],3);
	ROUND3(d,a,b,c,x[10],9);
	ROUND3(c,d,a,b,x[6],11);
	ROUND3(b,c,d,a,x[14],15);
	ROUND3(a,b,c,d,x[1],3);
	ROUND3(d,a,b,c,x[9],9);
	ROUND3(c,d,a,b,x[5],11);
	ROUND3(b,c,d,a,x[13],15);
	ROUND3(a,b,c,d,x[3],3);
	ROUND3(d,a,b,c,x[11],9);
	ROUND3(c,d,a,b,x[7],11);
	ROUND3(b,c,d,a,x[15],15);

	ctx->h[0] += a;
	ctx->h[1] += b;
	ctx->h[2] += c;
	ctx->h[3] += d;
}

void md4_init(struct hash* ctx)
{
	ctx->h[0] = 0x67452301;
	ctx->h[1] = 0xefcdab89;
	ctx->h[2] = 0x98badcfe;
	ctx->h[3] = 0x10325476;
	ctx->len = ctx->buf_fill = 0;
}

void md4_process(struct hash *ctx, const char *data, uint32_t data_size)
{
	__generic_hash_process((void*)ctx, data, data_size, ctx->buf,
			       &(ctx->buf_fill), 64);
	ctx->len += data_size;
}

void md4_finish(struct hash *ctx)
{
	__generic_hash_finish((void*)ctx, ctx->len, ctx->buf, ctx->buf_fill,
			      64, 8,1);
}

void md4_to_string(struct hash *ctx, char *str)
{
	__string_helper32(ctx->h[0], str);
	__string_helper32(ctx->h[1], str+8);
	__string_helper32(ctx->h[2], str+16);
	__string_helper32(ctx->h[3], str+24);
	str[32] = 0;
}
