#include "classifier.h"
#include "counters.h"

struct ml_comment {
	const char *start;
	const char *end;
};

struct src_spec {
	struct ml_comment ml_comment;
	const char *sl_comment[];
};

struct src_spec c_spec {
	.ml_comment = {
		.start = "/*",
		.end   = "*/"
	},
	.sl_comment = { "//", nullptr },
};

struct src_spec asm_spec {
	.ml_comment = {
		.start = "/*",
		.end   = "*/"
	},
	.sl_comment = { "#", nullptr },
};

struct src_spec python_spec {
	.ml_comment = {
		.start = "\"\"\"",
		.end   = "\"\"\""
	},
	.sl_comment = { "#", nullptr },
};

struct src_spec shell_spec {
	.ml_comment = {
		.start = nullptr,
		.end   = nullptr
	},
	.sl_comment = { "#", nullptr },
};

struct src_spec xml_spec {
	.ml_comment = {
		.start = "<!--",
		.end   = "-->"
	},
	.sl_comment = { },
};

struct src_spec latex_spec {
	.ml_comment = {
		.start = nullptr,
		.end   = nullptr
	},
	.sl_comment = { "%", nullptr },
};

struct src_spec text_spec {
	.ml_comment = {
		.start = nullptr,
		.end   = nullptr
	},
	.sl_comment = { nullptr },
};

struct src_spec asn1_spec {
	.ml_comment = {
		.start = nullptr,
		.end   = nullptr
	},
	.sl_comment = { "--", nullptr },
};

struct src_spec rust_spec {
	.ml_comment = {
		.start = nullptr,
		.end   = nullptr
	},
	.sl_comment = { "//", nullptr },
};

struct src_spec css_spec {
	.ml_comment = {
		.start = "/*",
		.end   = "*/"
	},
	.sl_comment = { nullptr },
};

struct src_spec ruby_spec {
	.ml_comment = {
		.start = "=begin",
		.end   = "=end"
	},
	.sl_comment = { "#", nullptr },
};

enum state {
	BEGIN,
	STRING,
	SLCOMMENT,
	MLCOMMENT,
};

file_result::file_result(std::string n)
	: code(0), comment(0), whitespace(0), duplicate(false),
	  type(file_type::unknown), name(n)
{
}

/*
 * Use static variants for strlen and strncmp for performance reasons.
 * Performance counting LOC for C/C++ dropped by factor 4 using the
 * variants from the C library.
 */
static bool str_eq(const char *s1, const char *s2, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (s1[i] != s2[i])
			return false;
	}

	return true;
}

static size_t str_len(const char *s)
{
	size_t len = 0;

	for (len = 0; s[len] != 0; ++len);

	return len;
}

static bool sl_comment_start(const struct src_spec &spec,
			     const char *buffer, size_t index, size_t size, size_t &l)
{
	size_t i = 0;

	while (spec.sl_comment[i] != nullptr) {
		const auto pattern = spec.sl_comment[i++];
		auto len = str_len(pattern);
		auto buf_len = size - index;

		if (len > buf_len)
			continue;

		if (str_eq(pattern, &buffer[index], len)) {
			l = len;
			return true;
		}
	}

	return false;
}

static bool ml_comment_start(const struct src_spec &spec,
			     const char *buffer, size_t index, size_t size, size_t &l)
{
	const auto str = spec.ml_comment.start;

	if (str == nullptr)
		return false;

	auto len = str_len(str);
	auto buf_len = size - index;
	l = len;

	return ((len <= buf_len) && str_eq(str, &buffer[index], len));
}

static bool ml_comment_end(const struct src_spec &spec,
			   const char *buffer, size_t index, size_t size, size_t &l)
{
	const auto str = spec.ml_comment.end;

	if (str == nullptr)
		return false;

	auto len = str_len(str);
	auto buf_len = size - index;
	l = len;

	return ((len <= buf_len) && str_eq(str, &buffer[index], len));
}

static void finish_line(struct file_result &r, bool &code, bool &comment, size_t &counter)
{
	if (counter == 0)
		return;

	if (code)
		r.code += 1;
	else if (comment)
		r.comment += 1;
	else
		r.whitespace += 1;

	counter = 0;
}

static void generic_count_source(const struct src_spec &spec,
				 struct file_result &r,
				 const char *buffer,
				 size_t size)
{
	bool code = false, comment = false;
	enum state state = BEGIN;
	size_t counter = 0, len;
	char c = 0, lc;

	if (size == 0)
		return;

	for (size_t index = 0; index < size; ++index, ++counter) {
		lc = c;
		c  = buffer[index];

		if (state == BEGIN) {
			if (ml_comment_start(spec, buffer, index, size, len)) {
				comment = true;
				state   = MLCOMMENT;
				index += len - 1;
			} else if (sl_comment_start(spec, buffer, index, size, len)) {
				comment = true;
				state   = SLCOMMENT;
				index += len - 1;
			} else if (c == '"') {
				code = true;
				state = STRING;
			} else if (c == '\n') {
				finish_line(r, code, comment, counter);
				code = comment = false;
			} else if (!isspace(c) && c != '/') {
				code = true;
			}
		} else if (state == STRING) {
			if (c == '"' && lc != '\\') {
				state = BEGIN;
			} else if (c == '\n') {
				finish_line(r, code, comment, counter);
				comment = false;
				code    = true;
			}
		} else if (state == SLCOMMENT) {
			if (c == '\n') {
				finish_line(r, code, comment, counter);
				code = comment = false;
				state = BEGIN;
			}
		} else if (state == MLCOMMENT) {
			if (ml_comment_end(spec, buffer, index, size, len)) {
				state = BEGIN;
				index += len - 1;
			} else if (c == '\n') {
				finish_line(r, code, comment, counter);
				code    = false;
				comment = true;
			}
		}
	}

	// Finish the last line
	if (c != '\n')
		finish_line(r, code, comment, counter);
}
static size_t perl_strip__END__(const char *buffer, size_t size)
{
	const char *pattern = "__END__";
	size_t pattern_len = 7;


	for (size_t ret = 0; ret < size; ++ret) {
		if (buffer[ret] != '\n')
			continue;

		auto remain = size - ret;

		if (remain < pattern_len)
			break;

		if (str_eq(&buffer[ret + 1], pattern, pattern_len))
			return ret - 1;
	}

	return size;
}

void count_c(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(c_spec, r, buffer, size);
}

void count_asm(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(asm_spec, r, buffer, size);
}

void count_python(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(python_spec, r, buffer, size);
}

void count_perl(struct file_result &r, const char *buffer, size_t size)
{
	// Perl needs some pre-processing
	size = perl_strip__END__(buffer, size);
	generic_count_source(shell_spec, r, buffer, size);
}

void count_xml(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(xml_spec, r, buffer, size);
}

void count_shell(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(shell_spec, r, buffer, size);
}

void count_latex(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(latex_spec, r, buffer, size);
}

void count_text(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(text_spec, r, buffer, size);
}

void count_asn1(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(asn1_spec, r, buffer, size);
}

void count_rust(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(rust_spec, r, buffer, size);
}

void count_css(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(css_spec, r, buffer, size);
}

void count_ruby(struct file_result &r, const char *buffer, size_t size)
{
	generic_count_source(ruby_spec, r, buffer, size);
}
