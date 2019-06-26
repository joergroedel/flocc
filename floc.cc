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

#include "counters.h"
#include "md4.h"

namespace fs = std::experimental::filesystem;

struct type_result {
	uint32_t code = 0;
	uint32_t comment = 0;
	uint32_t whitespace = 0;
	uint32_t files = 0;
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

static const char *get_file_type_cstr(file_type t)
{
	switch (t) {
	case file_type::unknown:	return "Unknown";
	case file_type::c:		return "C";
	case file_type::c_cpp_header:	return "C/C++ Header";
	case file_type::cpp:		return "C++";
	case file_type::assembly:	return "Assembler";
	case file_type::python:		return "Python";
	case file_type::perl:		return "Perl";
	case file_type::xml:		return "XML";
	case file_type::html:		return "HTML";
	case file_type::svg:		return "SVG";
	case file_type::xslt:		return "XSLT";
	case file_type::java:		return "Java";
	case file_type::yacc:		return "Yacc";
	case file_type::dts:		return "Device-Tree";
	case file_type::makefile:	return "Makefile";
	case file_type::kconfig:	return "Kconfig";
	case file_type::shell:		return "Shell";
	case file_type::yaml:		return "YAML";
	case file_type::latex:		return "LaTeX";
	case file_type::text:		return "Text";
	case file_type::cocci:		return "Coccinelle";
	case file_type::asn1:		return "ASN.1";
	case file_type::sed:		return "Sed";
	case file_type::awk:		return "Awk";
	case file_type::rust:		return "Rust";
	case file_type::go:		return "Go";
	case file_type::json:		return "JSON";
	case file_type::javascript:	return "JavaScript";
	case file_type::css:		return "CSS";
	case file_type::lex:		return "Lex";
	case file_type::ruby:		return "Ruby";
	case file_type::typescript:	return "TypeScript";
	}

	return nullptr;
}

std::map<std::string, unsigned> unknown_exts;

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

static void update_unknown_exts(std::string ext)
{
	auto p = unknown_exts.find(ext);

	if (p == unknown_exts.end())
		unknown_exts[ext]  = 1;
	else
		unknown_exts[ext] += 1;
}

static void dump_unknown_exts(void)
{
	std::cout << "Unknown Extensions:" << std::endl;
	for (auto &e : unknown_exts)
		std::cout << "  [" << e.first << "]: " << e.second << std::endl;
}

static file_type classifile(std::string path)
{
	std::string name, ext;

	// Extract filename first to eliminate "./path/to/file" cases
	auto pos = path.find_last_of("/");
	if (pos != std::string::npos)
		path = path.substr(pos + 1);

	pos = path.find_last_of(".");
	if (pos == std::string::npos) {
		if (path == "Makefile")
			return file_type::makefile;
		else if (path == "Kconfig")
			return file_type::kconfig;
		else
			return file_type::unknown;
	}

	name = path.substr(0, pos - 1);
	ext  = path.substr(pos);

	if (ext == ".c")
		return file_type::c;
	if (ext == ".h" || ext == ".hh")
		return file_type::c_cpp_header;
	if (ext == ".cc" || ext == ".C" || ext == ".c++")
		return file_type::cpp;
	if (ext == ".S")
		return file_type::assembly;
	if (ext == ".py")
		return file_type::python;
	if (ext == ".pl" || ext == ".pm")
		return file_type::perl;
	if (ext == ".xml")
		return file_type::xml;
	if (ext == ".html" || ext == ".htm" || ext == ".xhtml")
		return file_type::html;
	if (ext == ".svg")
		return file_type::svg;
	if (ext == ".xsl" || ext == ".xslt")
		return file_type::xslt;
	if (ext == ".java")
		return file_type::java;
	if (ext == ".y")
		return file_type::yacc;
	if (ext == ".dts" || ext == ".dtsi")
		return file_type::dts;
	if (ext == ".sh")
		return file_type::shell;
	if (ext == ".yaml")
		return file_type::yaml;
	if (ext == ".tex")
		return file_type::latex;
	if (ext == ".txt" || ext == ".rst")
		return file_type::text;
	if (ext == ".cocci")
		return file_type::cocci;
	if (ext == ".asn1")
		return file_type::asn1;
	if (ext == ".sed")
		return file_type::sed;
	if (ext == ".awk")
		return file_type::awk;
	if (ext == ".rs")
		return file_type::rust;
	if (ext == ".go")
		return file_type::go;
	if (ext == ".json")
		return file_type::json;
	if (ext == ".js")
		return file_type::javascript;
	if (ext == ".css")
		return file_type::css;
	if (ext == ".l")
		return file_type::lex;
	if (ext == ".rb")
		return file_type::ruby;
	if (ext == ".ts" || ext == ".tsx")
		return file_type::typescript;
	if (name == "Kconfig")
		return file_type::kconfig;

	update_unknown_exts(ext);

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

static void fs_count_one(struct file_result &r,
			 const fs::directory_entry &p,
			 std::map<std::string, bool> &seen,
			 file_buffer &fb)
{
	const auto &path = p.path();

	if (!fs::is_regular_file(p))
		return;

	auto type    = classifile(path);
	auto handler = get_file_handler(type);
	auto size    = fs::file_size(path);

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
		fs_count_one(fr, entry, seen, fb);
		fl.emplace_back(std::move(fr));
	} else if (fs::is_directory(input)) {
		for (auto &p : fs::recursive_directory_iterator(path)) {
			if (ignore_entry(p))
				continue;

			file_result fr(p.path());
			fs_count_one(fr, p, seen, fb);
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
	std::cout << "  --dump-unknown     Dump counts of unknown file extensions" << std::endl;
	std::cout << "                     git-revisions instead of filesystem paths" << std::endl;
}

enum {
	OPTION_HELP,
	OPTION_REPO,
	OPTION_GIT,
	OPTION_DUMP_UNKNOWN,
};

static struct option options[] = {
	{ "help",		no_argument,		0, OPTION_HELP           },
	{ "repo",		required_argument,	0, OPTION_REPO           },
	{ "git",		no_argument,		0, OPTION_GIT            },
	{ "dump-unknown",	no_argument,		0, OPTION_DUMP_UNKNOWN   },
};

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	bool dump_unknown = false;
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

		std::cout << "  " << std::setw(68) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

		std::cout << std::setw(20) << "  Total";
		std::cout << std::setw(12) << files;
		std::cout << std::setw(12) << code;
		std::cout << std::setw(12) << comment;
		std::cout << std::setw(12) << whitespace << std::endl;
	}

	if (dump_unknown)
		dump_unknown_exts();

	return 0;
}
