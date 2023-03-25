#include "erhe/log/log.hpp"
#include "erhe/toolkit/timestamp.hpp"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#if defined _WIN32
#   include <spdlog/sinks/msvc_sink.h>
#endif

#if defined _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#endif

#include <vector>

namespace erhe::log
{

void console_init()
{
#if defined _WIN32
    HWND   hwnd           = GetConsoleWindow();
    HICON  icon           = LoadIcon(NULL, MAKEINTRESOURCE(32516));
    HANDLE hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode           = 0;
    GetConsoleMode(hConsoleHandle, &mode);
    SetConsoleMode(hConsoleHandle, (mode & ~ENABLE_MOUSE_INPUT) | ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS);

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)(icon));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)(icon));

    std::setlocale(LC_CTYPE, ".UTF8");
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

store_log_sink::store_log_sink()
{
}

auto store_log_sink::get_log() const -> const std::deque<Entry>&
{
    return m_entries;
}

void store_log_sink::trim(size_t trim_size)
{
    if (m_entries.size() > trim_size) {
        const auto trim_count = m_entries.size() - trim_size;
        m_entries.erase(
            m_entries.begin(),
            m_entries.begin() + trim_count
        );
        assert(m_entries.size() == trim_size);
    }
}

void store_log_sink::set_paused(bool paused)
{
    m_is_paused = paused;
}

auto store_log_sink::is_paused() const -> bool
{
    return m_is_paused;
}

void store_log_sink::sink_it_(const spdlog::details::log_msg& msg)
{
    if (m_is_paused) {
        return;
    }
    m_entries.push_back(
        Entry{
            .timestamp    = erhe::toolkit::timestamp(),
            .message      = std::string{msg.payload.begin(), msg.payload.end()},
            .repeat_count = 0,
            .level        = msg.level,
        }
    );
}

void store_log_sink::flush_()
{
}

namespace {

#if defined _WIN32
std::shared_ptr<spdlog::sinks::msvc_sink_mt>         sink_msvc;
#endif
std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> sink_console;
std::shared_ptr<spdlog::sinks::basic_file_sink_mt>   sink_log_file;
std::shared_ptr<store_log_sink>                      tail_store_log;
std::shared_ptr<store_log_sink>                      frame_store_log;
bool s_log_to_console{false};

}

auto get_tail_store_log() -> const std::shared_ptr<store_log_sink>&
{
    return tail_store_log;
}
auto get_frame_store_log() -> const std::shared_ptr<store_log_sink>&
{
    return frame_store_log;
}

void log_to_console()
{
    s_log_to_console = true;
}

void initialize_log_sinks()
{
#if defined _WIN32
    sink_msvc       = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#endif
    sink_console    = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink_log_file   = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
    tail_store_log  = std::make_shared<store_log_sink>();
    frame_store_log = std::make_shared<store_log_sink>();

    sink_log_file->set_pattern("[%H:%M:%S %z] [%n] [%L] [%t] %v");
}

auto make_logger(
    std::string               name,
    spdlog::level::level_enum level,
    bool                      tail
) -> std::shared_ptr<spdlog::logger>
{
    auto logger = std::make_shared<spdlog::logger>(
        name,
        spdlog::sinks_init_list{
#if defined _WIN32
            sink_msvc,
#endif
            //sink_console,
            sink_log_file,
            tail ? tail_store_log : frame_store_log
        }
    );
    if (s_log_to_console) {
        logger->sinks().push_back(sink_console);
    }
    spdlog::register_logger(logger);
    logger->set_level(level);
    logger->flush_on(spdlog::level::trace);
    return logger;
}

} // namespace erhe::log

