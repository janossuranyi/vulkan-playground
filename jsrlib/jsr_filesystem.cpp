#include <fstream>
#include <sstream>
#include <regex>
#include "jsr_resources.h"
#include "jsr_logger.h"

namespace fs = std::filesystem;

namespace jsrlib {

	Filesystem Filesystem::root{};


	Filesystem::Filesystem(const std::string& aBaseDir)
	{
		baseDir = fs::path(aBaseDir);
	}

	Filesystem::Filesystem() : Filesystem("")
	{
	}

	void Filesystem::SetBaseDir(const std::string& aBaseDir)
	{
		baseDir = fs::path(aBaseDir);
	}

	std::string Filesystem::Resolv(const std::string& dir)
	{
		const auto path = baseDir / fs::path(dir);

		return path.generic_string();
	}

	bool Filesystem::ReadFileAsText(const std::string& path, std::string& buffer)
	{
		std::ifstream ifs((baseDir / path).generic_string().c_str(), std::ios::in);

		if (ifs.is_open())
		{
			std::stringstream sstr;
			sstr << ifs.rdbuf();
			buffer = sstr.str();
			ifs.close();
		}
		else
		{
			Error("Cannot load file %s", path.c_str());

			return false;
		}

		return true;
	}

	std::vector<unsigned char> Filesystem::ReadFile(const std::string& path)
	{
		std::vector<unsigned char> result;

		auto infile = baseDir / fs::path(path);
		std::ifstream input{ infile.generic_string().c_str(), std::ios::binary };

		if (input.good())
		{
			// copies all data into buffer
			result = std::vector<unsigned char>{ std::istreambuf_iterator<char>(input), {} };

			input.close();
		}

		return result;
	}

	std::vector<std::string> Filesystem::GetDirectoryEntries(const std::string& dirname, bool recursive, const char* filter)
	{
		std::vector<std::string> result;
		const fs::path path = fs::absolute(fs::path(dirname));

		std::filesystem::directory_iterator start;
		try {
			start = fs::directory_iterator{ path };
		}
		catch (std::exception e)
		{
			Error("%s", e.what());
			return result;
		}

		if (!filter)
		{
			for (auto const& e : start)
			{
				if (e.is_regular_file()) {
					result.push_back(e.path().generic_string());
				}
				else if (e.is_directory() && recursive)
				{
					auto a = GetDirectoryEntries(e.path().generic_string(), recursive);
					if (!a.empty()) result.insert(result.end(), a.begin(), a.end());
				}
			}
		}
		else
		{
			try
			{
				std::regex fr{ filter, std::regex::icase };
				for (auto const& e : fs::directory_iterator{ path })
				{
					if (e.is_regular_file() && std::regex_match(e.path().generic_string(), fr)) {
						result.push_back(e.path().generic_string());
					}
					else if (e.is_directory() && recursive)
					{
						auto a = GetDirectoryEntries(e.path().generic_string(), recursive, filter);
						if (!a.empty()) result.insert(result.end(), a.begin(), a.end());
					}
				}
			}
			catch (const std::exception& ex)
			{
				Error("Invalid regexp (%s) %s", filter, ex.what());
			}

		}
		return result;
	}

	std::string Filesystem::GetBaseDir() const
	{
		return baseDir.generic_string();
	}

}
