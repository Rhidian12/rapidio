# rapidio
An easy-to-use C++ 20 Win32 header-only memory-mapping library

## Quick Start
### Creating a new file
To create a new file, call `rapidio::FileView::CreateViewForNewFile()`. You can then write to that file view by passing a `std::string` to `view.Write()`
Note that `CreateViewForNewFile()` and `CreateViewFromExistingFile()` return a `std::optional`, and will return `std::nullopt` if the construction of the file view fails. Any errors are logged to STDERR.
```cpp
#include <rapidio.hpp>
#include <string>

int main()
{
  using namespace rapidio;

  // Not necessary, but we can provide an initial size to our FileView making it already create a file of that size
  size_t wantedFileSize = 12; // Hello World!

  FileView fileView = FileView::CreateViewForNewFile("somefile.txt", wantedFileSize).value();
  fileView.Write("Hello World!");

  fileView.Write("We can easily write more data");
  // Done!
}
```

### Accessing an existing file
To access an existing file, call `rapidio::FileView::CreateViewFromExistingFile()`. You must select either ReadWrite or ReadOnly access.
```cpp
#include <rapidio.hpp>
#include <string>

int main()
{
  using namespace rapidio;

  FileView fileView = FileView::CreateViewFromExistingFile("somefile.txt", FileAccessMode::ReadOnly, FileOpenMode::OpenExisting).value();
  std::string helloWorld = fileView.Read(12);
  // Filepointer has moved 12 bytes
  std::string nextData = fileView.Read(24);
  // We can go back to the start of the file by calling Seek()
  fileView.Seek(0);
  std::string helloWorldAgain = fileView.Read(12);
  // Done!
}
```

## Performance
You can find the (simple) benchmarks in the repository, run on a desktop with 32GB RAM, and AMD Ryzen 9 5900X 12-Core Processor @ 3.70 GHz

- RapidIO writing 100 MB to a new file (on average over 100 iterations): 60 milliseconds
- STL writing 100 MB to a new file (on average over 100 iterations): 226 milliseconds (~376% slower than RapidIO)
- RapidIO reading 100 MB of an existing file (on average over 100 iterations): 39 milliseconds
- STL reading 100 MB of an existing file (on average over 100 iterations): 833 milliseconds (~2135% slower than RapidIO)

## Future Work
- Add Linux support
- Filemappings are currently re-mapped to the entire file when re-allocated, they should grow linearly
- Make this library no longer header-only to avoid including `<Windows.h>`
