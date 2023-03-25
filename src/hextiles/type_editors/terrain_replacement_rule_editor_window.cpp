#include "type_editors/terrain_replacement_rule_editor_window.hpp"
#include "type_editors/type_editor.hpp"
#include "menu_window.hpp"
#include "tiles.hpp"

#include "erhe/application/imgui/imgui_windows.hpp"

#include <imgui.h>

namespace hextiles
{

Terrain_replacement_rule_editor_window* g_terrain_replacement_rule_editor_window{nullptr};

Terrain_replacement_rule_editor_window::Terrain_replacement_rule_editor_window()
    : erhe::components::Component{c_type_name}
    , Imgui_window               {c_title}
{
}

Terrain_replacement_rule_editor_window::~Terrain_replacement_rule_editor_window() noexcept
{
    ERHE_VERIFY(g_terrain_replacement_rule_editor_window == this);
    g_terrain_replacement_rule_editor_window = nullptr;
}

void Terrain_replacement_rule_editor_window::declare_required_components()
{
    require<erhe::application::Imgui_windows>();
}

void Terrain_replacement_rule_editor_window::initialize_component()
{
    ERHE_VERIFY(g_terrain_replacement_rule_editor_window == nullptr);
    erhe::application::g_imgui_windows->register_imgui_window(this, "terrain_replacement_rule_editor");
    hide();
    g_terrain_replacement_rule_editor_window = this;
}

void Terrain_replacement_rule_editor_window::imgui()
{
    constexpr ImVec2 button_size{110.0f, 0.0f};

    if (ImGui::Button("Back to Menu", button_size)) {
        g_menu_window->show_menu();
    }
    ImGui::SameLine();

    if (ImGui::Button("Load", button_size)) {
        g_tiles->load_terrain_replacement_rule_defs();
    }
    ImGui::SameLine();

    if (ImGui::Button("Save", button_size)) {
        g_tiles->save_terrain_replacement_rule_defs();
    }

    g_type_editor->terrain_replacement_rule_editor_imgui();
}

} // namespace hextiles
