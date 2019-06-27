#ifndef __FILETREE_H
#define __FILETREE_H

#include <string>
#include <map>

#include "counters.h"

struct loc_result {
	uint32_t code;
	uint32_t comment;
	uint32_t whitespace;

	loc_result();
	loc_result& operator+=(const loc_result&);
};

class file_entry {
protected:
	file_type m_type;

	std::map<file_type, loc_result>   m_results;
	std::map<std::string, file_entry> m_entries;

public:
	file_entry();
	file_entry *get_entry(std::string, file_type);
	void add_results(file_type, const loc_result&);
};

void insert_file_result(file_entry *root, const struct file_result r);

#endif
