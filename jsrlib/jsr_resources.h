#pragma once

#include <string>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <iostream>

namespace jsrlib {

	typedef std::unordered_map<std::string, std::string> glsl_defs_t;

	class Filesystem
	{
	public:
		Filesystem(const std::string& aBaseDir);
		Filesystem();
		void SetBaseDir(const std::string& aBaseDir);
		std::string Resolv(const std::string& dir);
		bool ReadFileAsText(const std::string& path, std::string& buffer);
		std::vector<unsigned char> ReadFile(const std::string& path);

		static std::vector<std::string> GetDirectoryEntries(const std::string& dirname, bool recursive = false, const char* filter = nullptr);

		std::string GetBaseDir() const;

		static Filesystem root;
	private:
		std::filesystem::path baseDir;
	};

	class ResourceManager 
	{
	public:
		ResourceManager();
		bool AddResourcePath(const std::string& path);
		std::string GetTextResource(const std::string& name);
		std::string GetShaderSource(const std::string& name, int max_depth = 2);
		std::string GetShaderSourceWithVersion(const std::string& name, int max_depth = 2);
		std::string GetShaderSourceWithVersionAndDefs(const std::string& name, const glsl_defs_t& defs, int max_depth = 2);
		std::vector<unsigned char> GetBinaryResource(const std::string& name);
		std::string GetResource(const std::string& name);
		void SetDefaultVersionFile(const std::string& s);
		static ResourceManager instance;
	private:
		std::unordered_map<std::string, std::string> resource_map;
		std::set<std::string> sources;
		std::string default_version_file_name;

	};

	extern ResourceManager* resourceManager;
}