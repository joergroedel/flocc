#ifndef __COUNTERS_H
#define __COUNTERS_H

#include <string>

enum class file_type {
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
