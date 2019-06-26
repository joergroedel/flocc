#ifndef __COUNTERS_H
#define __COUNTERS_H

#include <string>

enum class file_type {
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

#endif
