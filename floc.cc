#include <experimental/filesystem>
#include <iostream>
#include <cstdint>
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

namespace fs = std::experimental::filesystem;

struct result {
	uint32_t code;
	uint32_t comment;
	uint32_t whitespace;

	result()
		: code(0), comment(0), whitespace(0)
	{
	}
};

enum state {
	BEGIN,
	STRING,
	SLCOMMENT,
	MLCOMMENT,
};

static void finish_line(struct result &r, bool &code, bool &comment, size_t &counter)
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

static void count_c(struct result &r, const char *buffer, size_t size)
{
	bool code = false, comment = false;
	enum state state = BEGIN;
	size_t counter = 0;
	char c = 0, lc;
	size_t index;

	if (size == 0)
		return;

	for (size_t index = 0; index < size; ++index, ++counter) {
		lc = c;
		c  = buffer[index];

		if (state == BEGIN) {
			if (c == '"') {
				code = true;
				state = STRING;
			} else if (c == '*' && lc == '/') {
				comment = true;
				state   = MLCOMMENT;
			} else if (c == '/' && lc == '/') {
				comment = true;
				state   = SLCOMMENT;
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
			if (c == '/' && lc == '*') {
				state = BEGIN;
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

static bool read_file_to_buffer(const char *path, char *buffer, size_t size)
{
	size_t fill = 0;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		std::cerr << "Can't open " << path << " for reading" << std::endl;
		return false;
	}

	while (size) {
		auto r = read(fd, buffer + fill, size);
		if (r < 0 && errno != EINTR) {
			std::cerr << "Error reading file " << path << std::endl;
			break;
		} else if (r == 0) {
			std::cerr << "Unexpected end of file " << path << std::endl;
			break;
		} else {
			fill += r;
			size -= r;
		}
	}

	close(fd);

	return size == 0;
}

int main(int argc, char **argv)
{
	uint32_t files = 0;
	struct result r;
	char *buffer = NULL;
	size_t buf_size = 0;

	if (argc < 2)
		return 1;

	for (auto &p : fs::recursive_directory_iterator(argv[1])) {
		const auto &path = p.path();
		auto ext = path.extension();

		if (!fs::is_regular_file(p))
			continue;

		if (!(ext == ".c" || ext == ".h" || ext == ".cc"))
			continue;

		files += 1;

		auto size = fs::file_size(path);
		if (size > buf_size) {
			delete[] buffer;
			buffer = new char[size];
		}

		if (read_file_to_buffer(path.c_str(), buffer, size))
			count_c(r, buffer, size);
	}

	std::cout << "Scanned " << files << " files" << std::endl;
	std::cout << "Code Lines       : " << r.code << std::endl;
	std::cout << "Comment Lines    : " << r.comment << std::endl;
	std::cout << "Whitespace Lines : " << r.whitespace << std::endl;

	return 0;
}
