#pragma once

#include "erhe/application/configuration.hpp"
#include "erhe/components/components.hpp"
#include "erhe/toolkit/window.hpp"

namespace erhe::application {

class Imgui_window;

}

namespace hextiles {

class Application_impl;

class Application
    : public erhe::components::Component
{
public:
    static constexpr std::string_view c_type_name{"Application"};
    static constexpr uint32_t c_type_hash{
        compiletime_xxhash::xxh32(
            c_type_name.data(),
            c_type_name.size(),
            {}
        )
    };

    Application ();
    ~Application() noexcept override;

    // Implements Component
    [[nodiscard]] auto get_type_hash() const -> uint32_t override { return c_type_hash; }
    void initialize_component() override;

    auto initialize_components(int argc, char** argv) -> bool;
    void run();

    void component_initialization_complete(const bool initialization_succeeded);

private:
    Application_impl* m_impl;
};

extern Application* g_application;

} // namespace erhe::application
