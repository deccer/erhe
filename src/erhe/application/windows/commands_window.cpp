#include "erhe/application/windows/commands_window.hpp"

#include "erhe/application/imgui/imgui_viewport.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"

#include "erhe/application/commands/command.hpp"
#include "erhe/application/commands/commands.hpp"
#include "erhe/application/commands/key_binding.hpp"
#include "erhe/application/commands/mouse_button_binding.hpp"
#include "erhe/application/commands/mouse_drag_binding.hpp"
#include "erhe/application/commands/mouse_motion_binding.hpp"
#include "erhe/application/commands/mouse_wheel_binding.hpp"
#include "erhe/application/commands/update_binding.hpp"
#include "erhe/toolkit/view.hpp"

#include <gsl/gsl>

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

namespace erhe::application
{

Commands_window::Commands_window()
    : erhe::components::Component{c_type_name}
    , Imgui_window               {c_title}
{
}

Commands_window::~Commands_window() noexcept
{
}

void Commands_window::declare_required_components()
{
    require<Imgui_windows>();
}

void Commands_window::initialize_component()
{
    ERHE_VERIFY(g_imgui_windows != nullptr);

    g_imgui_windows->register_imgui_window(this, "commands");
}

void Commands_window::imgui()
{
#if defined(ERHE_GUI_LIBRARY_IMGUI)

    const auto* viewport = get_viewport();
    if (viewport->want_capture_keyboard()) {
        ImGui::TextUnformatted("ImGui Want Capture Keyboard");
    }
    if (viewport->want_capture_mouse()) {
        ImGui::TextUnformatted("ImGui Want Capture Mouse");
    }

    g_commands->imgui();
#endif
}

} // namespace erhe::application
