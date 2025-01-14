#include "tools/brushes/create/create_box.hpp"

#include "renderers/render_context.hpp"
#include "scene/scene_view.hpp"
#include "tools/brushes/brush.hpp"

#include "erhe/application/renderers/line_renderer.hpp"
#include "erhe/application/imgui/imgui_helpers.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/geometry/geometry.hpp"
#include "erhe/geometry/shapes/box.hpp"
#include "erhe/physics/icollision_shape.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#   include <imgui/misc/cpp/imgui_stdlib.h>
#endif

namespace editor
{

void Create_box::render_preview(
    const Create_preview_settings& preview_settings
)
{
    const Render_context& render_context = preview_settings.render_context;
    if (render_context.scene_view == nullptr) {
        return;
    }

    const auto& view_camera = render_context.scene_view->get_camera();
    if (view_camera) {
        auto& line_renderer = *erhe::application::g_line_renderer_set->hidden.at(2).get();
        line_renderer.add_cube(
            preview_settings.transform.matrix(),
            preview_settings.major_color,
            -0.5f * m_size,
             0.5f * m_size
        );
    }
}

void Create_box::imgui()
{
    ImGui::Text("Box Parameters");

    ImGui::SliderFloat3("Size",  &m_size.x, 0.0f, 10.0f);
    ImGui::SliderInt3  ("Steps", &m_steps.x, 1, 10);
    ImGui::SliderFloat ("Power", &m_power, 0.0f, 10.0f);
}

[[nodiscard]] auto Create_box::create(
    Brush_data& brush_create_info
) const -> std::shared_ptr<Brush>
{
    brush_create_info.geometry = std::make_shared<erhe::geometry::Geometry>(
        erhe::geometry::shapes::make_box(
            m_size,
            m_steps,
            m_power
        )
    );

    //brush_create_info.geometry->transform(erhe::toolkit::mat4_swap_xy);
    brush_create_info.geometry->build_edges();
    brush_create_info.geometry->compute_polygon_normals();
    brush_create_info.geometry->compute_tangents();
    brush_create_info.geometry->compute_polygon_centroids();
    brush_create_info.geometry->compute_point_normals(erhe::geometry::c_point_normals_smooth);
    //// brush_create_info.collision_shape = TODO

    std::shared_ptr<Brush> brush = std::make_shared<Brush>(brush_create_info);
    return brush;
}


} // namespace editor
