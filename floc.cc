#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cerrno>
#include <list>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <git2.h>

#include "md4.h"

namespace fs = std::experimental::filesystem;

struct type_result {
	uint32_t code = 0;
	uint32_t comment = 0;
	uint32_t whitespace = 0;
	uint32_t files = 0;
};

enum class file_type {
	unknown,
	c,
	c_cpp_header,
	cpp,
};

struct file_result {
	uint32_t code;
	uint32_t comment;
	uint32_t whitespace;
	bool duplicate;
	file_type type;
	std::string name;

	file_result(std::string n)
		: code(0), comment(0), whitespace(0), duplicate(false),
		  type(file_type::unknown), name(n)
	{
	}
};

using file_handler = std::function<void(struct file_result &r, const char *buffer, size_t size)>;
using file_list    = std::list<file_result>;

static const char *get_file_type_cstr(file_type t)
{
	switch (t) {
	case file_type::unknown:	return "Unknown";
	case file_type::c:		return "C";
	case file_type::c_cpp_header:	return "C/C++ Header";
	case file_type::cpp:		return "C++";
	}

	return nullptr;
}

enum state {
	BEGIN,
	STRING,
	SLCOMMENT,
	MLCOMMENT,
};

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

static void count_c(struct file_result &r, const char *buffer, size_t size)
{
	bool code = false, comment = false;
	enum state state = BEGIN;
	size_t counter = 0;
	char c = 0, lc;

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

static file_type classifile(std::string path)
{
	std::string ext;

	auto pos = path.find_last_of(".");
	if (pos == std::string::npos)
		return file_type::unknown;

	ext = path.substr(pos);

	if (ext == ".c")
		return file_type::c;
	if (ext == ".h" || ext == ".hh")
		return file_type::c_cpp_header;
	if (ext == ".cc" || ext == ".C" || ext == ".c++")
		return file_type::cpp;

	return file_type::unknown;
}

static file_handler get_file_handler(file_type type)
{
	file_handler fh_default = [](struct file_result &r, const char *buffer, size_t size) {};

	file_handler unknown;
	switch (type) {
		case file_type::c:
		case file_type::c_cpp_header:
		case file_type::cpp:
			return count_c;
		default:
			return fh_default;
	}
}

static std::string hash_buffer(const char *buffer, size_t size)
{
	char str[33];
	struct hash h;

	md4_init(&h);
	md4_process(&h, buffer, size);
	md4_finish(&h);
	md4_to_string(&h, str);

	return std::string(str);
}

static void fs_count_one(struct file_result &r,
			 const fs::directory_entry &p,
			 std::map<std::string, bool> &seen)
{
	const auto &path = p.path();

	if (!fs::is_regular_file(p))
		return;

	auto type    = classifile(path);
	auto handler = get_file_handler(type);
	auto size    = fs::file_size(path);

	r.type = type;

	std::unique_ptr<char[]> buffer(new char[size]);

	if (read_file_to_buffer(path.c_str(), buffer.get(), size)) {
		std::string hash = hash_buffer(buffer.get(), size);
		auto pos = seen.find(hash);

		if (pos != seen.end())
			r.duplicate = true;
		else
			seen[hash] = true;

		handler(r, buffer.get(), size);
	}
}

static void fs_counter(file_list &fl, const char *path)
{
	std::map<std::string, bool> seen;
	fs::path input = path;

	if (fs::is_regular_file(input)) {
		fs::directory_entry entry(input);
		file_result fr(input.string());
		fs_count_one(fr, entry, seen);
		fl.emplace_back(std::move(fr));
	} else if (fs::is_directory(input)) {
		for (auto &p : fs::recursive_directory_iterator(path)) {
			file_result fr(p.path());
			fs_count_one(fr, p, seen);
			fl.emplace_back(std::move(fr));
		}
	} else {
		throw fs::filesystem_error("File type not supported", input, std::error_code());
	}
}

struct git_walk_cb_data {
	git_repository *repo;
	file_list *fl;
	std::map<std::string, bool> seen;

	git_walk_cb_data()
		: repo(nullptr), fl(nullptr)
	{ }
};

static int git_tree_walker(const char *root, const git_tree_entry *entry, void *payload)
{
	struct git_walk_cb_data *cb_data = static_cast<struct git_walk_cb_data *>(payload);
	std::string fname = git_tree_entry_name(entry);
	const git_oid *oid = git_tree_entry_id(entry);
	git_otype ot = git_tree_entry_type(entry);
	file_result fr(fname);
	const char *buffer;
	git_blob *blob;
	char sha1[41];
	size_t size;
	int error;

	if (ot != GIT_OBJ_BLOB)
		return 0;

	auto type    = classifile(fname);
	auto handler = get_file_handler(type);

	fr.type = type;

	git_oid_fmt(sha1, oid);
	sha1[40] = 0;
	std::string hash = sha1;

	auto pos = cb_data->seen.find(hash);
	if (pos != cb_data->seen.end())
		fr.duplicate = true;

	cb_data->seen[hash] = true;

	error = git_blob_lookup(&blob, cb_data->repo, oid);
	if (error < 0)
		return error;

	buffer = static_cast<const char *>(git_blob_rawcontent(blob));
	size   = git_blob_rawsize(blob);

	handler(fr, buffer, size);
	cb_data->fl->emplace_back(std::move(fr));

	git_blob_free(blob);

	return 0;
}

static void git_counter(file_list &fl, const char *repo_path, const char *rev)
{
	struct git_walk_cb_data cb_data;
	git_repository *repo = nullptr;
	const git_oid *oid;
	git_commit *commit;
	git_object *head;
	git_tree *tree;
	int error;

	git_libgit2_init();

	error = git_repository_open(&repo, repo_path);
	if (error < 0)
		goto out;

	error = git_revparse_single(&head, repo, rev);
	if (error < 0)
		goto out;

	oid = git_object_id(head);

	error = git_commit_lookup(&commit, repo, oid);
	if (error < 0)
		goto out;

	error = git_commit_tree(&tree, commit);
	if (error < 0)
		goto out;

	cb_data.repo = repo;
	cb_data.fl   = &fl;

	error = git_tree_walk(tree, GIT_TREEWALK_PRE, git_tree_walker, &cb_data);
	if (error < 0)
		goto out;

	git_tree_free(tree);
	git_commit_free(commit);
	git_object_free(head);

out:
	if (error < 0) {
		const git_error *e = giterr_last();
		std::cerr << "Error: " << e->message << std::endl;
	}

	git_repository_free(repo);
	git_libgit2_shutdown();
}

static void usage(void)
{
	std::cout << "floc [options] [arguments...]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help, -h         Print this help message" << std::endl;
	std::cout << "  --repo, -r <repo>  Path to git-repository to use, implies --git" << std::endl;
	std::cout << "  --git, -g          Run in git-mode, arguments are interpreted as" << std::endl;
	std::cout << "                     git-revisions instead of filesystem paths" << std::endl;
}

enum {
	OPTION_HELP,
	OPTION_REPO,
	OPTION_GIT,
};

static struct option options[] = {
	{ "help",		no_argument,		0, OPTION_HELP           },
	{ "repo",		required_argument,	0, OPTION_REPO           },
	{ "git",		no_argument,		0, OPTION_GIT            },
};

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	const char *repo = ".";
	bool use_git = false;

	while (true) {
		int c, optidx;

		c = getopt_long(argc, argv, "hgr:", options, &optidx);
		if (c == -1)
			break;

		switch (c) {
		case OPTION_HELP:
		case 'h':
			usage();
			return 0;
		case OPTION_REPO:
			repo = optarg;
			/* Fall-through */
		case OPTION_GIT:
		case 'g':
			use_git = true;
			break;
		default:
			std::cerr << "Unknown option" << std::endl;
			usage();
			return 1;
		}
	}

	while (optind < argc)
		args.emplace_back(std::string(argv[optind++]));

	if (args.size() == 0) {
		if (use_git)
			args.emplace_back(std::string("HEAD"));
		else
			args.emplace_back(std::string("."));
	}

	for (auto &a : args) {
		uint32_t code = 0, comment = 0, whitespace = 0, files = 0, unique_files = 0;
		std::map<std::string, type_result> results;
		file_list fl;

		if (use_git)
			git_counter(fl, repo, a.c_str());
		else
			fs_counter(fl, a.c_str());

		for (auto &fr : fl) {
			if (fr.type == file_type::unknown)
				continue;

			files += 1;

			if (fr.duplicate)
				continue;

			unique_files += 1;

			auto &i = results[get_file_type_cstr(fr.type)];

			i.code       += fr.code;
			i.comment    += fr.comment;
			i.whitespace += fr.whitespace;
			i.files      += 1;

			code       += fr.code;
			comment    += fr.comment;
			whitespace += fr.whitespace;
		}

		std::cout << "Results for " << a << ":" << std::endl;
		std::cout << "  Scanned " << unique_files << " unique files (" << files << " total)" << std::endl;

		std::cout << std::left;
		std::cout << std::setw(20) << " ";
		std::cout << std::setw(12) << "Files";
		std::cout << std::setw(12) << "Code";
		std::cout << std::setw(12) << "Comment";
		std::cout << std::setw(12) << "Blank" << std::endl;

		std::cout << "  " << std::setw(68) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

		for (auto &ft : results) {
			const auto &type_str = ft.first;
			const auto &fr = ft.second;

			std::cout << "  " << std::setw(18) << type_str;
			std::cout << std::setw(12) << fr.files;
			std::cout << std::setw(12) << fr.code;
			std::cout << std::setw(12) << fr.comment;
			std::cout << std::setw(12) << fr.whitespace << std::endl;
		}

		std::cout << "  " << std::setw(69) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

		std::cout << std::setw(20) << "  Total";
		std::cout << std::setw(12) << files;
		std::cout << std::setw(12) << code;
		std::cout << std::setw(12) << comment;
		std::cout << std::setw(12) << whitespace << std::endl;
	}

	return 0;
}
