#include <iostream>
#include <map>

#include "classifier.h"

std::map<std::string, unsigned> unknown_exts;

static void update_unknown_exts(std::string ext)
{
	auto p = unknown_exts.find(ext);

	if (p == unknown_exts.end())
		unknown_exts[ext]  = 1;
	else
		unknown_exts[ext] += 1;
}

void dump_unknown_exts(void)
{
	std::cout << "Unknown Extensions:" << std::endl;
	for (auto &e : unknown_exts)
		std::cout << "  [" << e.first << "]: " << e.second << std::endl;
}

file_type classifile(std::string path)
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

const char *get_file_type_cstr(file_type t)
{
	switch (t) {
	case file_type::ignore:		return "Ignore";
	case file_type::directory:	return "Directory";
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
