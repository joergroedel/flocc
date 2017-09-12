#include <experimental/filesystem>
#include <iostream>
#include <cstdint>
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <git2.h>

namespace fs = std::experimental::filesystem;

struct result {
	uint32_t code;
	uint32_t comment;
	uint32_t whitespace;
	uint32_t files;

	result()
		: code(0), comment(0), whitespace(0), files(0)
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

static bool classifile(std::string path)
{
	std::string ext;

	auto pos = path.find_last_of(".");
	if (pos == std::string::npos)
		return false;

	ext = path.substr(pos);

	return (ext == ".c" || ext == ".h" || ext == ".cc");
}

static void fs_counter(struct result &r, const char *path)
{
	char *buffer = NULL;
	size_t buf_size = 0;
	uint32_t files = 0;

	for (auto &p : fs::recursive_directory_iterator(path)) {
		const auto &path = p.path();

		if (!fs::is_regular_file(p))
			continue;

		if (!classifile(path))
			continue;

		r.files += 1;

		auto size = fs::file_size(path);
		if (size > buf_size) {
			delete[] buffer;
			buffer = new char[size];
		}

		if (read_file_to_buffer(path.c_str(), buffer, size))
			count_c(r, buffer, size);
	}

	delete[] buffer;
}

struct git_walk_cb_data {
	git_repository *repo;
	struct result *r;

	git_walk_cb_data()
		: repo(NULL), r(NULL)
	{ }
};

static int git_tree_walker(const char *root, const git_tree_entry *entry, void *payload)
{
	struct git_walk_cb_data *cb_data = static_cast<struct git_walk_cb_data *>(payload);
	std::string fname = git_tree_entry_name(entry);
	const git_oid *oid = git_tree_entry_id(entry);
	git_otype ot = git_tree_entry_type(entry);
	const char *buffer;
	git_blob *blob;
	size_t size;
	int error;

	if (ot != GIT_OBJ_BLOB)
		return 0;

	if (!classifile(fname))
		return 0;

	error = git_blob_lookup(&blob, cb_data->repo, oid);
	if (error < 0)
		return error;

	cb_data->r->files += 1;

	buffer = static_cast<const char *>(git_blob_rawcontent(blob));
	size   = git_blob_rawsize(blob);

	count_c(*(cb_data->r), buffer, size);

	git_blob_free(blob);

	return 0;
}

static void git_counter(struct result &r, const char *rev)
{
	struct git_walk_cb_data cb_data;
	git_repository *repo = NULL;
	const git_oid *oid;
	git_commit *commit;
	git_object *head;
	git_tree *tree;
	int error;

	git_libgit2_init();

	error = git_repository_open(&repo, ".");
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
	cb_data.r = &r;

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
	std::cout << "  --help, -h      Print this help message" << std::endl;
	std::cout << "  --git, -g       Run in git-mode, arguments are interpreted as git-revisions instead of filesystem paths" << std::endl;
}

enum {
	OPTION_HELP,
	OPTION_GIT,
};

static struct option options[] = {
	{ "help",		no_argument,		0, OPTION_HELP           },
	{ "git",		no_argument,		0, OPTION_GIT            },
};

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	bool use_git = false;

	while (true) {
		int c, optidx;

		c = getopt_long(argc, argv, "hg", options, &optidx);
		if (c == -1)
			break;

		switch (c) {
		case OPTION_HELP:
		case 'h':
			usage();
			break;
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
		struct result r;

		if (use_git)
			git_counter(r, a.c_str());
		else
			fs_counter(r, a.c_str());

		std::cout << "Results for " << a << ":" << std::endl;
		std::cout << "  Scanned " << r.files << " files" << std::endl;
		std::cout << "  Code Lines       : " << r.code << std::endl;
		std::cout << "  Comment Lines    : " << r.comment << std::endl;
		std::cout << "  Whitespace Lines : " << r.whitespace << std::endl;
	}

	return 0;
}
