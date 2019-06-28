#include <experimental/filesystem>
#include <iostream>
#include <iomanip>

#include "classifier.h"
#include "filetree.h"

namespace fs = std::experimental::filesystem;

loc_result::loc_result()
	: code(0), comment(0), whitespace(0), files(0)
{
}

loc_result &loc_result::operator+=(const loc_result &r)
{
	code       += r.code;
	comment    += r.comment;
	whitespace += r.whitespace;
	files      += r.files;

	return *this;
}

file_entry::file_entry()
	: m_type(file_type::directory)
{
}

file_entry *file_entry::get_entry(std::string name, file_type type)
{
	auto p = m_entries.find(name);

	if (m_type != file_type::directory)
		throw std::invalid_argument("get_entry() called on regular file");

	if (p == m_entries.end()) {
		file_entry entry;

		entry.m_type = type;
		auto i = m_entries.emplace(std::make_pair(name, entry));
		p = i.first;

	}

	return &p->second;
}

void file_entry::add_results(file_type type, const loc_result& r)
{
	m_results[type] += r;
}

void file_entry::jsonize(std::ostream& os)
{
	bool first = true;

	// Open Object
	os << "{";

	// Print type
	os << "\"Type\":\"" << get_file_type_cstr(m_type) << "\"";

	os << ",\"Results\":[";
	for (auto &pr : m_results) {
		if (!first)
			os << ",";
		first = false;
		os << "{";
		os << "\"Type\":\"" << get_file_type_cstr(pr.first) << "\",";
		os << "\"Files\":" << pr.second.files << ",";
		os << "\"Code\":" << pr.second.code << ",";
		os << "\"Comment\":" << pr.second.comment << ",";
		os << "\"Blank\":" << pr.second.whitespace;
		os << "}";
	}
	os << "]";

	if (m_type == file_type::directory) {
		os << ",\"Entries\":{";
		first = true;
		for (auto &pe : m_entries) {
			if (!first)
				os << ",";
			first = false;
			os << "\"" << pe.first << "\":";
			pe.second.jsonize(os);
		}
		os << "}";
	}

	// Close Object
	os << "}";
}

void insert_file_result(file_entry *root, const struct file_result &r)
{
	fs::path fpath           = r.name;
	fs::path ppath           = fpath.parent_path();
	std::string filename     = fpath.filename();
	struct file_entry *entry = root;
	loc_result result;

	result.code       = r.code;
	result.comment    = r.comment;
	result.whitespace = r.whitespace;
	result.files      = 1;
	root->add_results(r.type, result);

	for (auto &de : ppath) {
		entry = entry->get_entry(de, file_type::directory);
		entry->add_results(r.type, result);
	}

	entry = entry->get_entry(filename, r.type);
	entry->add_results(r.type, result);
}

