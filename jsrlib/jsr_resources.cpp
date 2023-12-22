#include <fstream>
#include <sstream>
#include <functional>
#include <regex>
#include "jsr_resources.h"
#include "jsr_logger.h"

namespace fs = std::filesystem;

namespace jsrlib {

    ResourceManager ResourceManager::instance{};

    ResourceManager* resourceManager = &ResourceManager::instance;

    static void dir_traversal(const fs::path& start, std::function<void(const fs::path&)> callback)
    {
        auto it = fs::directory_iterator{ start };
        for (auto const& e : it)
        {
            if (e.is_regular_file())
            {
                callback(e);
            }
            else if (e.is_directory())
            {
                dir_traversal(e, callback);
            }
        }
    }


    ResourceManager::ResourceManager()
    {
        default_version_file_name = "shaders/builtin/version.inc.glsl";
    }

    bool ResourceManager::AddResourcePath(const std::string& path)
    {
        const auto src = fs::absolute(fs::path(path));
        const auto rel = src.filename();
        const auto src_len = src.generic_string().length();

        if (!fs::exists(src)) return false;

        if (sources.count(src.generic_string()) > 0) return false;
        sources.insert(src.generic_string());

        dir_traversal(src, [&](const fs::path& elem) {
            auto subs = fs::path(elem.generic_string().substr(src_len));
            const auto key = rel.generic_string() + subs.generic_string();
            const auto val = elem.generic_string();
            resource_map.emplace(key, val);
            //Info("%s --> %s", key.c_str(), val.c_str());
            });

        Info("[ResourceManager]: %s added", src.generic_string().c_str());

        return true;
    }

    std::string ResourceManager::GetTextResource(const std::string& name)
    {
        if (resource_map.count(name) == 0) return "";

        const auto& fn = resource_map.at(name);

        std::string result;
        Filesystem::root.ReadFileAsText(fn, result);

        return result;

    }

    std::string ResourceManager::GetResource(const std::string& name)
    {
        if (resource_map.count(name) == 0) return "";

        return resource_map.at(name);
    }

    void ResourceManager::SetDefaultVersionFile(const std::string& s)
    {
        default_version_file_name = s;
    }

    std::vector<unsigned char> ResourceManager::GetBinaryResource(const std::string& name)
    {
        if (resource_map.count(name) == 0)
            return std::vector<unsigned char>{};

        const auto& fn = resource_map.at(name);

        return Filesystem::root.ReadFile(fn);

    }

    std::string ResourceManager::GetShaderSource(const std::string& name, int depth)
    {
        static const std::regex include_regex("^[#@]include[ ]*\"(.+)\"");
        std::string output;

        if (depth < 0 || resource_map.count(name) == 0)
        {
            return output;
        }

        const auto& fileName = resource_map.at(name);
        const auto fn = fs::path(name).filename().generic_string();
        const auto dir = name.substr(0, name.find(fn));

        std::ifstream ifs(fileName, std::ios::in);
        std::string line;
        if (ifs.is_open())
        {
            while (std::getline(ifs, line))
            {
                std::smatch m;
                if (std::regex_search(line, m, include_regex))
                {
                    const auto realname = dir + m[1].str();
                    if (realname != name) {
                        output += GetShaderSource(realname, depth - 1) + "\n";
                    }
                }
                else
                {
                    output += line + "\n";
                }
            }
            ifs.close();
        }

        return output;
    }

    std::string ResourceManager::GetShaderSourceWithVersion(const std::string& name, int max_depth)
    {
        auto output = GetShaderSource(name, max_depth);
        auto version = GetTextResource(default_version_file_name);

        return version + "\n" + output;
    }

    std::string ResourceManager::GetShaderSourceWithVersionAndDefs(const std::string& name, const glsl_defs_t& defs, int max_depth)
    {
        auto output = GetShaderSource(name, max_depth);
        auto version = GetTextResource(default_version_file_name);

        std::string defs_str("");
        for (const auto& item : defs)
        {
            defs_str = defs_str
                .append("#define ")
                .append(item.first)
                .append(" ")
                .append(item.second)
                .append("\n");
        }
        
        return version
            .append("\n")
            .append(defs_str)
            .append(output);
    }

}
