#include "erhe/toolkit/file.hpp"
#include "erhe/toolkit/defer.hpp"
#include "erhe/toolkit/toolkit_log.hpp"

#if defined(ERHE_OS_WINDOWS)
#   include <Windows.h>
#   include <shobjidl.h>
#endif

#include <filesystem>
#include <fstream>

namespace erhe::toolkit
{

auto read(const std::filesystem::path& path) -> std::optional<std::string>
{
    // Watch out for fio
    try {
        if (
            std::filesystem::exists(path) &&
            std::filesystem::is_regular_file(path) &&
            !std::filesystem::is_empty(path)
        ) {
            const std::size_t file_length = std::filesystem::file_size(path);
            std::FILE* file =
#if defined(_WIN32) // _MSC_VER
                _wfopen(path.c_str(), L"rb");
#else
                std::fopen(path.c_str(), "rb");
#endif
            if (file == nullptr) {
                log_file->error("Could not open file '{}' for reading", path.string());
                return {};
            }

            std::size_t bytes_to_read = file_length;
            std::size_t bytes_read = 0;
            std::string result(file_length, '\0');
            do {
                const auto read_byte_count = std::fread(result.data() + bytes_read, 1, bytes_to_read, file);
                if (read_byte_count == 0) {
                    log_file->error("Error reading file '{}'", path.string());
                    return {};
                }
                bytes_read += read_byte_count;
                bytes_to_read -= read_byte_count;
            } while (bytes_to_read > 0);

            std::fclose(file);

            return std::optional<std::string>(result);
        }
    } catch (...) {
        log_file->error("Error reading file '{}'", path.string());
    }
    return {};
}

#if defined(ERHE_OS_WINDOWS)
auto select_file() -> std::optional<std::filesystem::path>
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (!SUCCEEDED(hr)) {
        return {};
    }
    ERHE_DEFER( CoUninitialize(); );

    IFileOpenDialog* file_open_dialog{nullptr};
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&file_open_dialog));
    if (!SUCCEEDED(hr) || (file_open_dialog == nullptr)) {
        return {};
    }
    ERHE_DEFER( file_open_dialog->Release(); );

    FILEOPENDIALOGOPTIONS options{0};
    hr = file_open_dialog->GetOptions(&options);
    if (!SUCCEEDED(hr)) {
        return {};
    }
    options = options | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM;

    hr = file_open_dialog->SetOptions(options);
    if (!SUCCEEDED(hr)) {
        return {};
    }

    hr = file_open_dialog->Show(nullptr);
    if (!SUCCEEDED(hr)) {
        return {};
    }

    IShellItem* item{nullptr};
    hr = file_open_dialog->GetResult(&item);
    if (!SUCCEEDED(hr) || (item == nullptr)) {
        return {};
    }
    ERHE_DEFER( item->Release(); );

    PWSTR path{nullptr};
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
    if (!SUCCEEDED(hr) || (path == nullptr)) {
        return {};
    }
    ERHE_DEFER( CoTaskMemFree(path); );

    return std::filesystem::path(path);
}
#else
auto select_file() -> std::optional<std::filesystem::path>
{
    // TODO
    return {};
}
#endif

} // namespace erhe::toolkit
