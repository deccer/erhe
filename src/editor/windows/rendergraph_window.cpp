#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "windows/rendergraph_window.hpp"
#include "editor_log.hpp"
#include "editor_scenes.hpp"
#include "scene/scene_root.hpp"

#include "erhe/application/imgui/imgui_renderer.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/application/imgui/ImNodesEz.h"
#include "erhe/application/rendergraph/rendergraph.hpp"
#include "erhe/application/rendergraph/rendergraph_node.hpp"
#include "erhe/graphics/texture.hpp"
#include "erhe/gl/enum_string_functions.hpp"
#include "erhe/gl/gl_helpers.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

#include <vector>
#include <map>
#include <string>
#include <imgui.h>
#include <imgui_internal.h>


namespace editor
{

Rendergraph_window::Rendergraph_window()
    : erhe::components::Component    {c_type_name}
    , erhe::application::Imgui_window{c_title, c_type_name}
{
}

Rendergraph_window::~Rendergraph_window() noexcept
{
    ImNodes::Ez::FreeContext(m_imnodes_context);
}

void Rendergraph_window::declare_required_components()
{
    require<erhe::application::Imgui_windows>();
}

void Rendergraph_window::initialize_component()
{
    get<erhe::application::Imgui_windows>()->register_imgui_window(this);
}

void Rendergraph_window::post_initialize()
{
    m_editor_scenes  = get<Editor_scenes>();
    m_render_graph   = get<erhe::application::Rendergraph>();
    m_imgui_renderer = get<erhe::application::Imgui_renderer>();
}

auto Rendergraph_window::flags() -> ImGuiWindowFlags
{
    return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

namespace {

auto get_connection_color(const int key) -> ImVec4
{
    switch (key)
    {
        case erhe::application::Rendergraph_node_key::window:              return ImVec4{0.4f, 0.5f, 0.8f, 1.0f};
        case erhe::application::Rendergraph_node_key::viewport:            return ImVec4{0.8f, 1.0f, 0.2f, 1.0f};
        case erhe::application::Rendergraph_node_key::shadow_maps:         return ImVec4{0.6f, 0.6f, 0.6f, 1.0f};
        case erhe::application::Rendergraph_node_key::depth_visualization: return ImVec4{0.1f, 0.8f, 0.8f, 1.0f};
        default: return ImVec4{1.0f, 0.0f, 1.0f, 1.0f};
    }
}

}

void Rendergraph_window::imgui()
{
#if defined(ERHE_GUI_LIBRARY_IMGUI)
#if 0
    const ImGuiTreeNodeFlags parent_flags{
        ImGuiTreeNodeFlags_OpenOnArrow       |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanFullWidth
    };
    //const ImGuiTreeNodeFlags leaf_flags{
    //    ImGuiTreeNodeFlags_SpanFullWidth    |
    //    ImGuiTreeNodeFlags_NoTreePushOnOpen |
    //    ImGuiTreeNodeFlags_Leaf
    //};

    if (ImGui::TreeNodeEx("Render Graph", parent_flags))
    {
        const auto& render_graph_nodes = m_render_graph->get_nodes();
        for (const auto& node : render_graph_nodes)
        {
            if (ImGui::TreeNodeEx(node->name().c_str(), parent_flags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                const auto& inputs = node->get_inputs();
                for (const auto& input : inputs)
                {
                    const auto& producer_node = input.producer_node.lock();
                    if (producer_node)
                    {
                        ImGui::Text(
                            "Input '%s' Producer %s (%s)",
                            input.key.c_str(),
                            producer_node->name().c_str(),
                            c_str(input.resource_routing)
                        );
                    }
                }

                const auto& outputs = node->get_outputs();
                for (const auto& output : outputs)
                {
                    const auto& consumer_node = output.consumer_node.lock();
                    if (consumer_node)
                    {
                        ImGui::Text(
                            "Output '%s' Consumer %s (%s)",
                            output.key.c_str(),
                            consumer_node->name().c_str(),
                            c_str(output.resource_routing)
                        );
                    }
                }

                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    const auto& scene_roots = m_editor_scenes->get_scene_roots();
    if (ImGui::TreeNodeEx("Scenes", parent_flags))
    {
        for (const auto& scene_root : scene_roots)
        {
            ImGui::Text("%s", scene_root->name().c_str());
            //if (ImGui::TreeNodeEx(scene_root->name().c_str(), parent_flags))
            //{
            //    ImGui::TreePop();
            //}
        }
        ImGui::TreePop();
    }
#endif

    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Image Size", &m_image_size, 4.0f, 1000.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Curve Strength", &m_curve_strength, 0.0f, 100.0f);

    if (m_imnodes_context == nullptr)
    {
        m_imnodes_context = ImNodes::Ez::CreateContext();
    }
    ImNodes::Ez::SetContext(m_imnodes_context);

    ImNodes::Ez::BeginCanvas();

    ImNodes::CanvasState* canvas_state = ImNodes::GetCurrentCanvas();

    const float zoom = canvas_state->Zoom;

    //for (auto it = nodes.begin(); it != nodes.end();)
    //`{
    const auto& render_graph_nodes = m_render_graph->get_nodes();
    ImNodes::Ez::PushStyleVar(ImNodesStyleVar_CurveStrength, m_curve_strength);
    for (const auto& node : render_graph_nodes)
    {
        // Start rendering node
        const auto glm_position = node->get_position();
        const ImVec2 start_position{glm_position.x, glm_position.y};
        const bool start_selected = node->get_selected();
        ImVec2 position = start_position;
        bool selected = start_selected;
        if (ImNodes::Ez::BeginNode(node.get(), node->name().c_str(), &position, &selected))
        {
            const auto& inputs  = node->get_inputs();
            const auto& outputs = node->get_outputs();

            std::vector<ImNodes::Ez::SlotInfo> input_slot_infos;
            for (const auto& input : inputs)
            {
                input_slot_infos.emplace_back(input.label.c_str(), input.key);
            }

            ImNodes::Ez::InputSlots(input_slot_infos.data(), static_cast<int>(input_slot_infos.size()));

            // Custom node content may go here
            for (const auto& output : outputs)
            {
                if (output.resource_routing == erhe::application::Resource_routing::None)
                {
                    ImGui::Text("<%s>", output.label.c_str());
                    continue;
                }

                const auto& texture = node->get_producer_output_texture(
                    output.resource_routing,
                    output.key
                );
                if (
                    texture &&
                    (texture->target() == gl::Texture_target::texture_2d) &&
                    (texture->width () >= 1) &&
                    (texture->height() >= 1) &&
                    (gl_helpers::has_color(texture->internal_format()))
                )
                {
                    const float aspect = static_cast<float>(texture->width()) / static_cast<float>(texture->height());
                    ImGui::Text("%s:", output.label.c_str());
                    m_imgui_renderer->image(
                        texture,
                        static_cast<int>(zoom * aspect * m_image_size),
                        static_cast<int>(zoom * m_image_size),
                        glm::vec2{0.0f, 1.0f},
                        glm::vec2{1.0f, 0.0f},
                        glm::vec4{1.0f, 1.0f, 1.0f, 1.0f},
                        false
                    );
                    if (ImGui::IsItemHovered())
                    {
                        std::string size = fmt::format(
                            "Size: {} x {}",
                            texture->width(),
                            texture->height(),
                            gl::c_str(texture->internal_format())
                        );
                        std::string format = fmt::format(
                            "Format: {}",
                            gl::c_str(texture->internal_format())
                        );
                        ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePreviousTooltip, ImGuiWindowFlags_None);
                        ImGui::TextUnformatted(size.c_str());
                        ImGui::TextUnformatted(format.c_str());
                        ImGui::EndTooltip();
                    }
                }
                else
                {
                    ImGui::Text("(%s)", output.label.c_str());
                }
            }

            std::vector<ImNodes::Ez::SlotInfo> output_slot_infos;
            for (const auto& output : outputs)
            {
                output_slot_infos.emplace_back(output.label.c_str(), output.key);
            }

            // Render output nodes first (order is important)
            ImNodes::Ez::OutputSlots(output_slot_infos.data(), static_cast<int>(output_slot_infos.size()));

            // Store new connections when they are created
            //// void* input_node{nullptr};
            //// const char* input_slot_title{nullptr};
            //// void* output_node{nullptr};
            //// const char* output_slot_title{nullptr};
            //// if (
            ////     ImNodes::GetNewConnection(
            ////         &input_node,
            ////         &input_slot_title,
            ////         &output_node,
            ////         &output_slot_title
            ////     )
            //// )
            //// {
            ////     ((MyNode*) new_connection.InputNode)->Connections.push_back(new_connection);
            ////     ((MyNode*) new_connection.OutputNode)->Connections.push_back(new_connection);
            //// }

            // Render output connections of this node
            for (const auto& output : outputs)
            {
                for (const auto& consumer_node : output.consumer_nodes)
                {
                    const auto consumer = consumer_node.lock();
                    if (!consumer)
                    {
                        continue;
                    }
                    const erhe::application::Rendergraph_consumer_connector* consumer_input =
                        consumer->get_input(output.resource_routing, output.key);

                    ImNodes::Ez::PushStyleColor(ImNodesStyleCol_Connection, get_connection_color(output.key));
                    const bool connection_ok = ImNodes::Connection(
                        consumer.get(),
                        consumer_input->label.c_str(),
                        node.get(),
                        output.label.c_str()
                    );
                    ImNodes::Ez::PopStyleColor(1);
                    if (!connection_ok)
                    {
                        log_scene->info("Connection delete");
                        // Remove deleted connections
                        //((MyNode*) connection.InputNode)->DeleteConnection(connection);
                        //((MyNode*) connection.OutputNode)->DeleteConnection(connection);
                    }
                }
            }

        }

        // Node rendering is done. This call will render node background based on size of content inside node.
        ImNodes::Ez::EndNode();

        if (
            (position.x != start_position.x) ||
            (position.y != start_position.y)
        )
        {
            node->set_position(glm::vec2{position.x, position.y});
        }
        if (selected != start_selected)
        {
            node->set_selected(selected);
        }

        //if (node->Selected && ImGui::IsKeyPressed(ImGuiKey_Delete) && ImGui::IsWindowFocused())
        //{
        //    // Deletion order is critical: first we delete connections to us
        //    for (auto& connection : node->Connections)
        //    {
        //        if (connection.OutputNode == node)
        //        {
        //            ((MyNode*) connection.InputNode)->DeleteConnection(connection);
        //        }
        //        else
        //        {
        //            ((MyNode*) connection.OutputNode)->DeleteConnection(connection);
        //        }
        //    }
        //    // Then we delete our own connections, so we don't corrupt the list
        //    node->Connections.clear();
        //
        //    delete node;
        //    it = nodes.erase(it);
        //}
        //else
        //{
        //    ++it;
        //}
    }
    ImNodes::Ez::PopStyleVar(1);

    if (ImGui::IsMouseReleased(1) && ImGui::IsWindowHovered() && !ImGui::IsMouseDragging(1))
    {
        ImGui::FocusWindow(ImGui::GetCurrentWindow());
        ImGui::OpenPopup("NodesContextMenu");
    }

    if (ImGui::BeginPopup("NodesContextMenu"))
    {
        //// for (const auto& desc : available_nodes)
        //// {
        ////     if (ImGui::MenuItem(desc.first.c_str()))
        ////     {
        ////         nodes.push_back(desc.second());
        ////         ImNodes::AutoPositionNode(nodes.back());
        ////     }
        //// }

        ImGui::Separator();
        if (ImGui::MenuItem("Reset Zoom"))
        {
            ImNodes::GetCurrentCanvas()->Zoom = 1;
        }

        if (ImGui::IsAnyMouseDown() && !ImGui::IsWindowHovered())
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImNodes::Ez::EndCanvas();
#endif // defined(ERHE_GUI_LIBRARY_IMGUI)
}

} // namespace editor