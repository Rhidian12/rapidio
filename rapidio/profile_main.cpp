#include <rapidio.hpp>

#include "testutils/UniqueDirectory.h"

#include <numeric>
#include <chrono>
#include <fstream>
#include <vector>

static int NR_ITERATIONS = 100;
static constexpr std::string_view NEW_BIG_FILE = "NewBigFile.txt";
static constexpr std::string_view BIG_FILE = "BigFile.txt";
constexpr size_t BIG_FILE_SIZE = 1024 * 1024 * 100; // 100 MB
static constexpr std::string_view ALPHABET = "abcdefghijklmnopqrstuvwxyz";

using Clock = std::chrono::steady_clock;

uint64_t GetAverageTime(std::vector<uint64_t>&& vec)
{
	std::sort(vec.begin(), vec.end());

	vec.erase(vec.begin(), vec.begin() + vec.size() / 10);
	vec.erase(vec.end() - 1 - vec.size() / 10, vec.end());

	return std::accumulate(vec.cbegin(), vec.cend(), 0ULL) / vec.size();
}

uint64_t BenchmarkWriteTests(std::function<void(fs::path, std::string)> Func)
{
	std::vector<uint64_t> Times;
	Times.reserve(NR_ITERATIONS);

	std::string BigFileData;
	BigFileData.reserve(BIG_FILE_SIZE);

	BigFileData += std::string(BIG_FILE_SIZE, ALPHABET[rand() % ALPHABET.size()]);

	UniqueDirectory Dir{ "rapidioperformance" };

	for (int i{}; i < NR_ITERATIONS; ++i)
	{
		Clock::time_point start = Clock::now();
		Func(Dir.GetPath(), BigFileData);
		Times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());

		for (const auto& entry : fs::directory_iterator(Dir.GetPath()))
		{
			fs::remove_all(entry.path());
		}
	}

	return GetAverageTime(std::move(Times));
}

uint64_t BenchmarkReadTests(std::function<void(fs::path)> func)
{
	std::vector<uint64_t> times;
	times.reserve(NR_ITERATIONS);

	UniqueDirectory Dir{ "rapidioperformance" };

	{
		std::string BigFileData;
		BigFileData.reserve(BIG_FILE_SIZE);

		BigFileData += std::string(BIG_FILE_SIZE, ALPHABET[rand() % ALPHABET.size()]);

		auto view = rapidio::FileView::CreateViewForNewFile(Dir.GetPath() / BIG_FILE, BIG_FILE_SIZE).value();
		view.Write(BigFileData);
	}

	for (int i{}; i < NR_ITERATIONS; ++i)
	{
		Clock::time_point start = Clock::now();
		func(Dir.GetPath());
		times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
	}

	return GetAverageTime(std::move(times));
}

int main()
{
	using namespace rapidio;

	uint64_t RapidIOWriteNewFileTime{ BenchmarkWriteTests([](const fs::path& Path, const std::string& Data)
		{
			FileView View = FileView::CreateViewForNewFile(Path / NEW_BIG_FILE, BIG_FILE_SIZE).value();

			View.Write(Data);
		}) };

	std::cout << "Average RapidIO Time of creating new file of 100 MB over " << NR_ITERATIONS << " iterations: " << RapidIOWriteNewFileTime << "ms \n";

	uint64_t STLWriteNewFileTime{ BenchmarkWriteTests([](const fs::path& Path, const std::string& Data)
		{
			std::ofstream file(Path / NEW_BIG_FILE);

			file << Data;
		}) };

	std::cout << "Average STL Time of creating new file of 100 MB over " << NR_ITERATIONS << " iterations: " << STLWriteNewFileTime << "ms \n";

	uint64_t RapidIOReadFileTime{ BenchmarkReadTests([](const fs::path& Path)
		{
			FileView View = FileView::CreateViewFromExistingFile(Path / BIG_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();

			std::string Temp = View.Read(BIG_FILE_SIZE);
		}) };

	std::cout << "Average RapidIO Time of reading an existing file of 100 MB over " << NR_ITERATIONS << " iterations: " << RapidIOReadFileTime << "ms \n";

	uint64_t STLReadFileTime{ BenchmarkReadTests([](const fs::path& Path)
		{
			std::ifstream File(Path / BIG_FILE);

			std::string Temp((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		}) };

	std::cout << "Average STL Time of reading an existing file of 100 MB over " << NR_ITERATIONS << " iterations: " << STLReadFileTime << "ms \n";
}