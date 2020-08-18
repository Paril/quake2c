extern "C"
{
	#define QCVM_INTERNAL
	#include "shared/shared.h"
	#include "vm.h"
	#include "g_file.h"
	#include "vm_string.h"
}

#include <filesystem>
#include <vector>
#include <regex>
namespace fs = std::filesystem;

const char *qcvm_cpp_absolute_path(const qcvm_t *vm, const char *relative)
{
	return qcvm_temp_format(vm, "%s", fs::absolute(relative).string().c_str());
}

char **qcvm_get_file_list(const qcvm_t *vm, const char *path, const char *ext, int32_t *num)
{
	std::vector<std::string> paths;

	// filtered search
	if (strchr(path, '*') || strchr(path, '?') || strchr(path, '[') || strchr(path, ']'))
	{
		// convert to regex;
		// keep \ escapes intact
		// convert * to [^/\\]*
		// convert ? to .
		// convert [! to [^
		// escape . outside of character classes with \.
		std::string raw_regex(path);
		bool inside_class = false;

		for (size_t i = 0; i < raw_regex.size(); i++)
		{
			switch (raw_regex[i])
			{
			case '.':
				if (!inside_class)
				{
					raw_regex.insert(i, "\\");
					i++;
				}
				continue;
			case '\\':
				i++;
				continue;
			case '*':
				raw_regex.insert(i, "[^/\\\\]");
				i += 6;
				continue;
			case '[':
				inside_class = true;
				if (raw_regex[i + 1] == '!')
					raw_regex[i + 1] = '^';
				continue;
			case ']':
				inside_class = false;
				continue;
			}
		}

		std::regex reg(raw_regex);
		fs::path base_path = qcvm_temp_format(vm, "%s", vm->path);

		for (auto &p : fs::recursive_directory_iterator(base_path))
			if (p.is_regular_file())
			{
				std::string str = p.path().string().substr(strlen(vm->path));
				std::replace(str.begin(), str.end(), '\\', '/');
				bool matched = std::regex_match(str, reg);

				if (matched)
					paths.push_back(str);
			}
	}
	// basic search
	else
	{
		fs::path base_path = qcvm_temp_format(vm, "%s%s", vm->path, path);

		for (auto &p : fs::directory_iterator(base_path))
			if (p.is_regular_file() && p.path().has_extension() && p.path().extension().string().substr(1).compare(ext) == 0)
			{
				std::string str = p.path().string().substr(strlen(vm->path));
				std::replace(str.begin(), str.end(), '\\', '/');
				paths.push_back(str);
			}
	}

	char **results = (char **)qcvm_alloc(vm, sizeof(char *) * paths.size());

	for (size_t i = 0; i < paths.size(); i++)
	{
		results[i] = (char *)qcvm_alloc(vm, sizeof(char) * paths[i].size() + 1);
		memcpy(results[i], paths[i].data(), paths[i].size());
	}

	*num = (int32_t)paths.size();
	return results;
}