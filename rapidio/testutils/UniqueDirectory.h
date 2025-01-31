#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class UniqueDirectory
{
public:
	UniqueDirectory(fs::path const& InPath)
	{
		CreateUniqueDirectory(InPath);
	}

	~UniqueDirectory()
	{
		fs::remove_all(Path);
	}

	fs::path operator/(std::string const& InPath) const
	{
		return Path / InPath;
	}

	fs::path const& GetPath() const
	{
		return Path;
	}

private:
	void CreateUniqueDirectory(fs::path const& InPath)
	{
		do
		{
			Path = fs::temp_directory_path() / (InPath.string() + std::to_string(rand() % 500));
		} while (fs::exists(Path));
		fs::create_directories(Path);
	}

	fs::path Path;
};