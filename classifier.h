// SPDX-License-Identifier: GPL-2.0+
/*
 * Fast Lines of Code Counter
 *
 * Copyright (C) 2021 SUSE
 *
 * Author: Jörg Rödel <jroedel@suse.de>
 */
#ifndef __CLASSIFIER_H
#define __CLASSIFIER_H

#include <string>

enum class file_type {
	ignore,
	directory,
	unknown,
	c,
	c_cpp_header,
	cpp,
	assembly,
	python,
	perl,
	xml,
	html,
	svg,
	xslt,
	java,
	yacc,
	dts,
	makefile,
	kconfig,
	shell,
	yaml,
	latex,
	text,
	cocci,
	asn1,
	sed,
	awk,
	rust,
	go,
	json,
	javascript,
	css,
	lex,
	ruby,
	typescript,
};

file_type classifile(std::string path);
void dump_unknown_exts(void);
const char *get_file_type_cstr(file_type t);

#endif
