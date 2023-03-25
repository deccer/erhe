#include "windows/post_processing_window.hpp"

#include "rendergraph/post_processing.hpp"
#include "scene/viewport_window.hpp"
#include "scene/viewport_windows.hpp"

#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/graphics/texture.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

namespace editor
{

Post_processing_window* g_post_processing_window{nullptr};

Post_processing_window::Post_processing_window()
    : Component   {c_type_name}
    , Imgui_window{c_title}
{
}

Post_processing_window::~Post_processing_window()
{
    ERHE_VERIFY(g_post_processing_window == this);
    g_post_processing_window = nullptr;

}

void Post_processing_window::declare_required_components()
{
    require<erhe::application::Imgui_windows>();
}

void Post_processing_window::initialize_component()
{
    ERHE_VERIFY(g_post_processing_window == nullptr);
    erhe::application::g_imgui_windows->register_imgui_window(this, "post_processing");
    g_post_processing_window = this;
}

void Post_processing_window::imgui()
{
#if defined(ERHE_GUI_LIBRARY_IMGUI)
    ERHE_PROFILE_FUNCTION

    //ImGui::DragInt("Taps",   &m_taps,   1.0f, 1, 32);
    //ImGui::DragInt("Expand", &m_expand, 1.0f, 0, 32);
    //ImGui::DragInt("Reduce", &m_reduce, 1.0f, 0, 32);
    //ImGui::Checkbox("Linear", &m_linear);

    //const auto discrete = kernel_binom(m_taps, m_expand, m_reduce);
    //if (ImGui::TreeNodeEx("Discrete", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
    //    for (size_t i = 0; i < discrete.weights.size(); ++i) {
    //        ImGui::Text(
    //            "W: %.3f O: %.3f",
    //            discrete.weights.at(i),
    //            discrete.offsets.at(i)
    //        );
    //    }
    //    ImGui::TreePop();
    //}
    //if (m_linear) {
    //    const auto linear = kernel_binom_linear(discrete);
    //    if (ImGui::TreeNodeEx("Linear", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
    //        for (size_t i = 0; i < linear.weights.size(); ++i) {
    //            ImGui::Text(
    //                "W: %.3f O: %.3f",
    //                linear.weights.at(i),
    //                linear.offsets.at(i)
    //            );
    //        }
    //        ImGui::TreePop();
    //    }
    //}

    const auto viewport_window = g_viewport_windows->last_window();
    if (!viewport_window) {
        return;
    }
    const auto  post_processing_node = viewport_window->get_post_processing_node();
    const auto& downsample_nodes     = post_processing_node->get_downsample_nodes();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    for (auto& node : downsample_nodes) {
        if (
            !node.texture                ||
            (node.texture->width () < 1) ||
            (node.texture->height() < 1)
        ) {
            continue;
        }

        if (node.axis == 0) {
            ImGui::SameLine();
        }
        image(
            node.texture,
            node.texture->width (),
            node.texture->height()
        );
    }
    ImGui::PopStyleVar();
#endif
}

} // namespace editor
