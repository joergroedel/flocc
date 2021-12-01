// SPDX-License-Identifier: GPL-2.0+
/*
 * Fast Lines of Code Counter
 *
 * Copyright (C) 2021 SUSE
 *
 * Author: Jörg Rödel <jroedel@suse.de>
 */
#ifndef __MD4_H
#define __MD4_H

struct hash {
	uint32_t h[10];
	char     buf[128];
	uint64_t len;
	uint32_t buf_fill;
};

extern void md4_init(struct hash* ctx);
extern void md4_process(struct hash *ctx, const char *data, uint32_t data_size);
extern void md4_finish(struct hash *ctx);
extern void md4_to_string(struct hash *ctx, char *str);

#endif /* __MD4_H */
