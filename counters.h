// SPDX-License-Identifier: GPL-2.0+
/*
 * Fast Lines of Code Counter
 *
 * Copyright (C) 2021 SUSE
 *
 * Author: Jörg Rödel <jroedel@suse.de>
 */
#ifndef __COUNTERS_H
#define __COUNTERS_H

#include <string>

#include "classifier.h"

struct file_result {
	uint32_t code;
	uint32_t comment;
	uint32_t whitespace;
	bool duplicate;
	file_type type;
	std::string name;

	file_result(std::string n);
};

void count_c(struct file_result &r, const char *buffer, size_t size);
void count_asm(struct file_result &r, const char *buffer, size_t size);
void count_python(struct file_result &r, const char *buffer, size_t size);
void count_perl(struct file_result &r, const char *buffer, size_t size);
void count_xml(struct file_result &r, const char *buffer, size_t size);
void count_shell(struct file_result &r, const char *buffer, size_t size);
void count_latex(struct file_result &r, const char *buffer, size_t size);
void count_text(struct file_result &r, const char *buffer, size_t size);
void count_asn1(struct file_result &r, const char *buffer, size_t size);
void count_rust(struct file_result &r, const char *buffer, size_t size);
void count_css(struct file_result &r, const char *buffer, size_t size);
void count_ruby(struct file_result &r, const char *buffer, size_t size);

#endif
