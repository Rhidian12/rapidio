#include <iostream>

#include "PathUtils.hpp"

#include <rapidio.hpp>

#include <gtest/gtest.h>
#include <fstream>

namespace
{
	namespace fs = std::filesystem;

	constexpr const char* SIMPLE_FILE = "SimpleFile.txt";
	constexpr const char* NON_EXISTING_FILE = "NonExistingFile.txt";
	constexpr const char* BIG_FILE = "BigFile.txt";

	constexpr size_t SIMPLE_FILE_SIZE = 12;
	constexpr size_t BIG_FILE_SIZE = 1024 * 1024 * 100; // 100 MB

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

	using namespace rapidio;

	struct RapidIOFixture : public ::testing::Test
	{
		UniqueDirectory TmpDir{ "testrapidio" };

		void SetUp() override
		{
			{
				std::ofstream file{ TmpDir / SIMPLE_FILE };
				file << "Hello World!";
			}
		}
	};

	struct RapidIOFixtureBigFile : public ::testing::Test
	{
		UniqueDirectory TmpDir{ "testrapidio" };
		std::string BigFileData;

		void SetUp() override
		{
			BigFileData.reserve(BIG_FILE_SIZE);
			constexpr std::string_view Alphabet = "abcdefghijklmnopqrstuvwxyz";

			std::ofstream file{ TmpDir / BIG_FILE };

			// Write chunks of 10 MB at a time to our file
			for (int i{}; i < 10; ++i)
			{
				const std::string Temp = std::string(BIG_FILE_SIZE / 10, Alphabet[rand() % Alphabet.size()]);
				BigFileData += Temp;
				file << Temp;
			}
		}
	};

	TEST_F(RapidIOFixture, TestCreateFileViewFromExistingFile)
	{
		EXPECT_NE(FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting), std::nullopt);
		EXPECT_NE(FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadWrite, FileOpenMode::OpenExisting), std::nullopt);
	}

	TEST_F(RapidIOFixture, TestCreateFileWithWrongAccessMode)
	{
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting), std::nullopt);
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenAlways), std::nullopt);
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::CreateAlways), std::nullopt);
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::TruncateExisting), std::nullopt);
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::CreateNew), std::nullopt);
		EXPECT_EQ(FileView::CreateViewFromExistingFile(TmpDir / NON_EXISTING_FILE, FileAccessMode::ReadOnly, FileOpenMode::CreateAlways), std::nullopt);
	}

	TEST_F(RapidIOFixture, TestCreateNewFile)
	{
		EXPECT_NE(FileView::CreateViewForNewFile(TmpDir / NON_EXISTING_FILE, 30), std::nullopt);
		fs::remove(TmpDir / NON_EXISTING_FILE);
	}

	TEST_F(RapidIOFixture, TestReadSimpleFile)
	{
		FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();

		EXPECT_EQ(View.Read(SIMPLE_FILE_SIZE), "Hello World!");

		ASSERT_TRUE(View.Seek(3));
		EXPECT_EQ(View.Read(9), "lo World!");

		ASSERT_TRUE(View.Seek(6));
		EXPECT_EQ(View.Read(6), "World!");

		ASSERT_TRUE(View.Seek(0));
		EXPECT_EQ(View.Read(666), "Hello World!");
		
		EXPECT_EQ(View.Read(0), "");

		EXPECT_EQ(View.Read(15), "");
	}

	TEST_F(RapidIOFixture, TestReadSimpleFileWithLimitedFileMappingSize)
	{
		FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting, 5).value();

		EXPECT_EQ(View.Read(SIMPLE_FILE_SIZE, false), "");

		EXPECT_EQ(View.Read(5, false), "Hello");
		
		ASSERT_TRUE(View.Seek(2));
		EXPECT_EQ(View.Read(3, false), "llo");

		ASSERT_TRUE(View.Seek(0));
		EXPECT_EQ(View.Read(666, false), "");
	}

	TEST_F(RapidIOFixture, TestWriteToReadOnlyFile)
	{
		FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();
		EXPECT_FALSE(View.Write("Some Random Data"));
	}

	TEST_F(RapidIOFixture, TestWriteToExistingSimpleFile)
	{
		{
			FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadWrite, FileOpenMode::OpenExisting).value();
	
			EXPECT_TRUE(View.Write("More Data!", SIMPLE_FILE_SIZE));
		}

		std::ifstream File{ TmpDir / SIMPLE_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_EQ(FileContents, "Hello World!More Data!");
	}

	TEST_F(RapidIOFixture, TestWriteFileMappingAutoGrow)
	{
		{
			FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadWrite, FileOpenMode::OpenExisting, SIMPLE_FILE_SIZE / 2).value();

			EXPECT_TRUE(View.Write("Boooo!", SIMPLE_FILE_SIZE / 2, false, true));
		}

		std::ifstream File{ TmpDir / SIMPLE_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_EQ(FileContents, "Hello Boooo!");
	}

	TEST_F(RapidIOFixture, TestWriteFileSizeAutoGrow)
	{
		{
			FileView View = FileView::CreateViewFromExistingFile(TmpDir / SIMPLE_FILE, FileAccessMode::ReadWrite, FileOpenMode::OpenExisting).value();

			EXPECT_TRUE(View.Write("More characters!", SIMPLE_FILE_SIZE, true, true));
		}

		std::ifstream File{ TmpDir / SIMPLE_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_EQ(FileContents, "Hello World!More characters!");
	}

	TEST_F(RapidIOFixture, TestCreateAndWriteToNewFile)
	{
		{
			FileView View = FileView::CreateViewForNewFile(TmpDir / NON_EXISTING_FILE, SIMPLE_FILE_SIZE).value();

			EXPECT_TRUE(View.Write("Hello World!", 0, false, false));
		}

		std::ifstream File{ TmpDir / NON_EXISTING_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_EQ(FileContents, "Hello World!");
	}

	TEST_F(RapidIOFixture, TestCreateAndWriteToNewFileWithAutoGrow)
	{
		{
			FileView View = FileView::CreateViewForNewFile(TmpDir / NON_EXISTING_FILE, 6).value();

			EXPECT_TRUE(View.Write("Hello World!"));
		}

		std::ifstream File{ TmpDir / NON_EXISTING_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_EQ(FileContents, "Hello World!");
	}

	TEST_F(RapidIOFixtureBigFile, TestReadBigFileInBlocks)
	{
		// Read in blocks of 10 MB
		FileView View = FileView::CreateViewFromExistingFile(TmpDir / BIG_FILE, FileAccessMode::ReadWrite, FileOpenMode::OpenExisting, BIG_FILE_SIZE / 10).value();

		for (int i{}; i < 10; ++i)
		{
			const std::string Data = View.Read(BIG_FILE_SIZE / 10);
			ASSERT_TRUE(Data == BigFileData.substr(i * BIG_FILE_SIZE / 10, BIG_FILE_SIZE / 10));
		}
	}

	TEST_F(RapidIOFixtureBigFile, TestReadBigFileInOneGo)
	{
		FileView View = FileView::CreateViewFromExistingFile(TmpDir / BIG_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();

		const std::string Data = View.Read(BIG_FILE_SIZE);
		EXPECT_TRUE(Data == BigFileData);
	}

	TEST_F(RapidIOFixtureBigFile, TestReadBigFileUnknownSize)
	{
		// Assume we don't know our file's size here

		FileView View = FileView::CreateViewFromExistingFile(TmpDir / BIG_FILE, FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();

		constexpr int FIVE_MB = 1024 * 1024 * 5;
		int Counter = 0;
		std::string Data;
		while (true)
		{
			Data = View.Read(FIVE_MB);
			if (Data.empty())
			{
				break;
			}

			ASSERT_TRUE(Data == BigFileData.substr(Counter++ * FIVE_MB, FIVE_MB));
		}
	}

	TEST_F(RapidIOFixtureBigFile, TestWriteBigAmountOfDataInChunks)
	{
		{
			FileView View = FileView::CreateViewForNewFile(TmpDir / NON_EXISTING_FILE, BIG_FILE_SIZE).value();

			for (int i{}; i < 2; ++i)
			{
				View.Write(BigFileData.substr(i * BIG_FILE_SIZE / 2, BIG_FILE_SIZE / 2), i * BIG_FILE_SIZE / 2);
			}
		}

		std::ifstream File{ TmpDir / NON_EXISTING_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_TRUE(FileContents == BigFileData);
	}

	TEST_F(RapidIOFixtureBigFile, TestWriteBigAmountOfData)
	{
		{
			FileView View = FileView::CreateViewForNewFile(TmpDir / NON_EXISTING_FILE, BIG_FILE_SIZE).value();

			EXPECT_TRUE(View.Write(BigFileData));
		}

		std::ifstream File{ TmpDir / NON_EXISTING_FILE };
		const std::string FileContents{ std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>() };
		EXPECT_TRUE(FileContents == BigFileData);
	}
}