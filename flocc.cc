// SPDX-License-Identifier: GPL-2.0+
/*
 * Fast Lines of Code Counter
 *
 * Copyright (C) 2021 SUSE
 *
 * Author: Jörg Rödel <jroedel@suse.de>
 */
#include <experimental/filesystem>
#include <functional>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cerrno>
#include <list>
#include <map>
#include <fstream>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <git2.h>

#include "classifier.h"
#include "counters.h"
#include "filetree.h"
#include "md4.h"

#include "version.h"

namespace fs = std::experimental::filesystem;

struct type_result {
	uint32_t code = 0;
	uint32_t comment = 0;
	uint32_t whitespace = 0;
	uint32_t files = 0;
};

struct timing {
	uint64_t start;
	uint64_t stop;
};

struct file_buffer {
	size_t size = 0;
	char *buffer = nullptr;

	void resize_buffer(size_t new_size)
	{
		if (new_size <= size)
			return;

		delete [] buffer;
		buffer = new char[new_size];
	}

	~file_buffer()
	{
		if (buffer != nullptr)
			delete [] buffer;
	}
};

using file_handler = std::function<void(struct file_result &r, const char *buffer, size_t size)>;
using file_list    = std::list<file_result>;

static uint64_t timeval(void)
{
	struct timeval tv;
	uint64_t val;

	gettimeofday(&tv, NULL);

	val  = tv.tv_sec * 1000;
	val += tv.tv_usec / 1000;

	return val;
}

static void record_start(struct timing &t)
{
	t.start = timeval();
}

static void record_stop(struct timing &t)
{
	t.stop = timeval();
}

/*
 * Use static variants for strlen and strncmp for performance reasons.
 * Performance counting LOC for C/C++ dropped by factor 4 using the
 * variants from the C library.
 */
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

static file_handler get_file_handler(file_type type)
{
	file_handler fh_default = [](struct file_result &r, const char *buffer, size_t size) {};

	file_handler unknown;
	switch (type) {
	case file_type::c:
	case file_type::c_cpp_header:
	case file_type::cpp:
	case file_type::java:
	case file_type::yacc:
	case file_type::dts:
	case file_type::cocci:
	case file_type::go:
	case file_type::javascript:
	case file_type::lex:
	case file_type::typescript:
		return count_c;
	case file_type::assembly:
		return count_asm;
	case file_type::python:
		return count_python;
	case file_type::perl:
		return count_perl;
	case file_type::xml:
	case file_type::html:
	case file_type::svg:
	case file_type::xslt:
		return count_xml;
	case file_type::makefile:
	case file_type::kconfig:
	case file_type::shell:
	case file_type::yaml:
	case file_type::sed:
	case file_type::awk:
		return count_shell;
	case file_type::latex:
		return count_latex;
	case file_type::text:
	case file_type::json:
		return count_text;
	case file_type::asn1:
		return count_asn1;
	case file_type::rust:
		return count_rust;
	case file_type::css:
		return count_css;
	case file_type::ruby:
		return count_ruby;
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

static bool fs_count_one(struct file_result &r,
			 const fs::directory_entry &p,
			 std::map<std::string, bool> &seen,
			 file_buffer &fb)
{
	const auto &path = p.path();

	if (!fs::is_regular_file(p))
		return false;

	auto type    = classifile(path);
	auto handler = get_file_handler(type);
	auto size    = fs::file_size(path);

	if (type == file_type::ignore)
		return false;

	r.type = type;

	fb.resize_buffer(size);

	if (read_file_to_buffer(path.c_str(), fb.buffer, size)) {
		std::string hash = hash_buffer(fb.buffer, size);
		auto pos = seen.find(hash);

		if (pos != seen.end())
			r.duplicate = true;
		else
			seen[hash] = true;

		handler(r, fb.buffer, size);
	}

	return true;
}

static bool ignore_entry(const fs::directory_entry &e)
{
	for (const auto &c : e.path()) {
		auto s = c.string();
		if (s[0] == '.' && s != "." && s != "..")
			return true;
	}

	return false;
}

static void fs_counter(file_list &fl, const char *path)
{
	std::map<std::string, bool> seen;
	fs::path input = path;
	file_buffer fb;

	if (fs::is_regular_file(input)) {
		fs::directory_entry entry(input);
		file_result fr(input.string());
		if (fs_count_one(fr, entry, seen, fb))
			fl.emplace_back(std::move(fr));
	} else if (fs::is_directory(input)) {
		std::string::size_type base_len;
		std::string base_path = path;

		if (*base_path.rbegin() != '/')
			base_path += '/';

		base_len = base_path.length();

		for (auto &p : fs::recursive_directory_iterator(path)) {
			if (ignore_entry(p) || !fs::is_regular_file(p))
				continue;

			file_result fr(p.path().string().substr(base_len));
			if (fs_count_one(fr, p, seen, fb))
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

	if (type == file_type::ignore)
		return 0;

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

	if (git_object_type(head) == GIT_OBJ_TAG) {
		git_tag *tag;

		error = git_tag_lookup(&tag, repo, oid);
		if (error < 0)
			goto out;

		oid = git_tag_target_id(tag);

		git_tag_free(tag);
	}

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

static void print_timing(std::ostream &os, uint64_t t,
			 uint32_t files, uint32_t lines)
{
	uint64_t files_per_msec;
	uint64_t lines_per_msec;

	files_per_msec = ((uint64_t)files * 10000) / t;
//	files_per_msec = files_per_msec / 1000;

	lines_per_msec = ((uint64_t)lines * 10000) / t;
//	lines_per_msec = lines_per_msec / 1000;

	os << "  T=";
	os << t / 1000 << '.';
	os << std::setw(3) << std::setfill('0') << t % 1000 << 's';
	os << std::setw(0) << std::setfill(' ');

	os << " (" << files_per_msec / 10 << '.';
	os << files_per_msec % 10 << " files/s";

	os << ",  " << lines_per_msec / 10 << '.';
	os << lines_per_msec % 10 << " lines/s";

	os << ')' << std::endl;
}

static void print_results_default(std::string arg, file_list &fl, struct timing timing)
{
	uint32_t code = 0, comment = 0, whitespace = 0, files = 0, unique_files = 0;
	std::map<std::string, type_result> results;
	uint64_t t = timing.stop - timing.start;

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

	std::cout << "Results for " << arg << ":" << std::endl;
	std::cout << "  Scanned " << unique_files << " unique files (" << files << " total)" << std::endl;

	print_timing(std::cout, t, unique_files, code + comment + whitespace);

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

	std::cout << "  " << std::setw(68) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	std::cout << std::setw(20) << "  Total";
	std::cout << std::setw(12) << files;
	std::cout << std::setw(12) << code;
	std::cout << std::setw(12) << comment;
	std::cout << std::setw(12) << whitespace << std::endl;
}

static void print_results_json(std::string arg, file_list &fl, std::ofstream &os)
{
	file_entry root;

	// Build File-Tree
	for (auto &fr : fl)
		insert_file_result(&root, fr);

	// Write Json Data
	root.jsonize(os, arg);
}

static void usage(void)
{
	std::cout << "flocc [options] [arguments...]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help, -h         Print this help message" << std::endl;
	std::cout << "  --version          Print version information and exit" << std::endl;
	std::cout << "  --repo, -r <repo>  Path to git-repository to use, implies --git" << std::endl;
	std::cout << "  --git, -g          Run in git-mode, arguments are interpreted as" << std::endl;
	std::cout << "                     git-revisions instead of filesystem paths" << std::endl;
	std::cout << "  --json <file>      Write detailed statistics to <file> in JSON format" << std::endl;
	std::cout << "  --dump-unknown     Dump counts of unknown file extensions" << std::endl;
}

static void version(void)
{
	std::cout << "Fast Lines Of Code Counter (flocc) version " << FLOCC_VERSION << std::endl;
	std::cout << "Licensed under the GNU General Public License, version 2 or later" << std::endl;
	std::cout << "Copyright (c) 2021 SUSE" << std::endl;
}

enum {
	OPTION_HELP,
	OPTION_VERSION,
	OPTION_REPO,
	OPTION_GIT,
	OPTION_JSON,
	OPTION_DUMP_UNKNOWN,
};

static struct option options[] = {
	{ "help",		no_argument,		0, OPTION_HELP           },
	{ "version",		no_argument,		0, OPTION_VERSION        },
	{ "repo",		required_argument,	0, OPTION_REPO           },
	{ "git",		no_argument,		0, OPTION_GIT            },
	{ "json",		required_argument,	0, OPTION_JSON           },
	{ "dump-unknown",	no_argument,		0, OPTION_DUMP_UNKNOWN   },
};

int main(int argc, char **argv)
{
	const char *json_file = nullptr;
	std::vector<std::string> args;
	bool dump_unknown = false;
	const char *repo = ".";
	bool use_git = false;
	std::ofstream json;
	bool first = true;

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
		case OPTION_VERSION:
			version();
			return 0;
		case OPTION_REPO:
			repo = optarg;
			/* Fall-through */
		case OPTION_GIT:
		case 'g':
			use_git = true;
			break;
		case OPTION_JSON:
			json_file = optarg;
			break;
		case OPTION_DUMP_UNKNOWN:
			dump_unknown = true;
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

	if (json_file != nullptr) {
		json.open(json_file);
		if (!json.is_open()) {
			std::cerr << "Can't open json file for writing " << json_file << std::endl;
			return 1;
		}
	}

	if (json_file != nullptr)
		json << "[";

	for (auto &a : args) {
		struct timing timing;
		file_list fl;

		try {
			record_start(timing);
			if (use_git)
				git_counter(fl, repo, a.c_str());
			else
				fs_counter(fl, a.c_str());
			record_stop(timing);
		} catch (const fs::filesystem_error& f) {
			std::cerr << "Can not access path " << f.path1() << std::endl;
			continue;
		} catch (const std::runtime_error& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			continue;
		}

		if (json_file == nullptr) {
			print_results_default(a, fl, timing);
		} else {
			if (!first)
				json << ",";
			first = false;
			print_results_json(a, fl, json);
		}
	}

	if (json_file != nullptr)
		json << "]";

	if (dump_unknown)
		dump_unknown_exts();

	return 0;
}
