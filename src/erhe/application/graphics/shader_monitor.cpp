#include "erhe/application/graphics/shader_monitor.hpp"

#include "erhe/application/application_log.hpp"
#include "erhe/application/configuration.hpp"

#include "erhe/toolkit/profile.hpp"

#include "erhe/toolkit/file.hpp"
#include "erhe/toolkit/verify.hpp"

namespace erhe::application
{

using std::string;

Shader_monitor* g_shader_monitor{nullptr};

Shader_monitor::Shader_monitor()
    : erhe::components::Component{c_type_name}
    , m_run                      {false}
{
}

Shader_monitor::~Shader_monitor() noexcept
{
    ERHE_VERIFY(g_shader_monitor == nullptr);
}

void Shader_monitor::deinitialize_component()
{
    ERHE_VERIFY(g_shader_monitor == this);

    log_shader_monitor->info("Shader_monitor shutting down");
    set_run(false);
    log_shader_monitor->info("Joining shader monitor poll thread");
    if (m_poll_filesystem_thread.joinable()) {
        m_poll_filesystem_thread.join();
    }
    log_shader_monitor->info("Shader_monitor shut down complete");
    m_files.clear();
    m_reload_list.clear();

    g_shader_monitor = nullptr;
}

void Shader_monitor::declare_required_components()
{
    require<Configuration>();
}

void Shader_monitor::initialize_component()
{
    ERHE_PROFILE_FUNCTION
    ERHE_VERIFY(g_shader_monitor == nullptr);
    g_shader_monitor = this; // here due to early exit

    auto ini = erhe::application::get_ini("erhe.ini", "shader_monitor");
    ini->get("enabled", config.enabled);

    if (!config.enabled) {
        log_startup->info("Shader monitor disabled due to erhe.ini setting");
        return;
    }

    set_run(true);
    m_poll_filesystem_thread = std::thread(&Shader_monitor::poll_thread, this);
}

void Shader_monitor::set_enabled(const bool enabled)
{
    set_run(enabled);
}

void Shader_monitor::add(
    erhe::graphics::Shader_stages::Create_info    create_info,
    gsl::not_null<erhe::graphics::Shader_stages*> shader_stages
)
{
    for (const auto& shader : create_info.shaders) {
        if (shader.source.empty() && std::filesystem::exists(shader.path)) {
            add(shader.path, create_info, shader_stages);
        }
    }
}

void Shader_monitor::add(
    const std::filesystem::path&                      path,
    const erhe::graphics::Shader_stages::Create_info& create_info,
    gsl::not_null<erhe::graphics::Shader_stages*>     shader_stages
)
{
    const std::lock_guard<std::mutex> lock{m_mutex};

    auto i = m_files.find(path);
    if (i == m_files.end()) {
        File f;
        m_files[path] = f;
    }

    auto& f = m_files[path];

    ERHE_VERIFY(std::filesystem::exists(path));

    f.path = path;

    if (!std::filesystem::exists(f.path)) {
        f.path.clear();
    } else {
        f.last_time = std::filesystem::last_write_time(f.path);
        f.reload_entries.emplace(create_info, shader_stages);
    }
}

void Shader_monitor::poll_thread()
{
    while (m_run) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        {
            ERHE_PROFILE_SCOPE("Shader_monitor::poll_thread");

            const std::lock_guard<std::mutex> lock{m_mutex};

            for (auto& i : m_files) {
                auto& f = i.second;

                // Watch out; filesystem can throw exception for some random reason,
                // like file being externally modified at the same time.
                try {
                    const bool ok =
                        std::filesystem::exists(f.path)    &&
                        !std::filesystem::is_empty(f.path) &&
                        std::filesystem::is_regular_file(f.path);
                    if (ok) {
                        const auto time = std::filesystem::last_write_time(f.path);
                        if (f.last_time != time) {
                            m_reload_list.emplace_back(&f);
                            continue;
                        }
                    }
                } catch (...) {
                    log_shader_monitor->warn("Failed to poll file {}", f.path.generic_string());
                    // Never mind exceptions.
                }
            }
        }
    }
    log_shader_monitor->info("Exiting shader monitor poll thread");
}

// static constexpr const char* c_shader_monitor_poll = "shader monitor poll";

void Shader_monitor::update_once_per_frame(const erhe::components::Time_context& time_context)
{
    static_cast<void>(time_context);
    ERHE_PROFILE_FUNCTION

    const std::lock_guard<std::mutex> lock{m_mutex};

    for (auto* f : m_reload_list) {
        for (const auto& entry : f->reload_entries) {
            const auto& create_info = entry.create_info;
            erhe::graphics::Shader_stages::Prototype prototype{create_info};
            if (prototype.is_valid()) {
                entry.shader_stages->reload(std::move(prototype));
            }
        }
        f->last_time = std::filesystem::last_write_time(f->path);
    }
    m_reload_list.clear();
}

} // namespace erhe::application
