#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace erhe::toolkit
{

class Timer
{
public:
    explicit Timer(const char* label);
    ~Timer() noexcept;

    Timer         (const Timer&) = delete;
    auto operator=(const Timer&) = delete;
    Timer         (Timer&&)      = delete;
    auto operator=(Timer&&)      = delete;

    [[nodiscard]] auto duration() const -> std::optional<std::chrono::steady_clock::duration>;
    [[nodiscard]] auto label   () const -> const char*;
    void begin();
    void end  ();

    static auto all_timers() -> std::vector<Timer*>;

private:
    static std::mutex          s_mutex;
    static std::vector<Timer*> s_all_timers;

    std::optional<std::chrono::steady_clock::time_point> m_start_time;
    std::optional<std::chrono::steady_clock::time_point> m_end_time;
    const char*                                          m_label{nullptr};
};

class Scoped_timer
{
public:
    explicit Scoped_timer(Timer& timer);
    ~Scoped_timer() noexcept;

private:
    Timer& m_timer;
};

}