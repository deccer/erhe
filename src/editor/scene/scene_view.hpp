#pragma once

#include "editor_message.hpp"
#include "renderers/programs.hpp"
#include "scene/node_raytrace_mask.hpp"

#include "erhe/application/rendergraph/rendergraph_node.hpp"
#include "erhe/application/commands/command.hpp"
#include "erhe/application/commands/command_context.hpp"
#include "erhe/application/imgui/imgui_window.hpp"
#include "erhe/components/components.hpp"
#include "erhe/message_bus/message_bus.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/viewport.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <optional>

namespace erhe::application
{
    class Configuration;
    class Imgui_viewport;
    class Imgui_windows;
    class Multisample_resolve_node;
    class Rendergraph;
    class Rendergraph_node;
    class View;
}

namespace erhe::geometry
{
    class Geometry;
}

namespace erhe::graphics
{
    class Framebuffer;
    class Texture;
}

namespace erhe::scene
{
    class Camera;
    class Mesh;
}

namespace editor
{

class Editor_message;
class Grid;
class Light_projections;
class Node_raytrace;
class Render_context;
class Scene_root;
class Shadow_render_node;

class Hover_entry
{
public:
    static constexpr std::size_t content_slot      = 0;
    static constexpr std::size_t tool_slot         = 1;
    static constexpr std::size_t brush_slot        = 2;
    static constexpr std::size_t rendertarget_slot = 3;
    static constexpr std::size_t grid_slot         = 4;
    static constexpr std::size_t slot_count        = 5;
    static constexpr std::size_t content_bit       = (1 << 0u);
    static constexpr std::size_t tool_bit          = (1 << 1u);
    static constexpr std::size_t brush_bit         = (1 << 2u);
    static constexpr std::size_t rendertarget_bit  = (1 << 3u);
    static constexpr std::size_t grid_bit          = (1 << 4u);
    static constexpr std::size_t all_bits          = 0xffffffffu;

    static constexpr std::array<uint32_t, slot_count> raytrace_slot_masks = {
        Raytrace_node_mask::content,
        Raytrace_node_mask::tool,
        Raytrace_node_mask::brush,
        Raytrace_node_mask::rendertarget,
        Raytrace_node_mask::grid
    };

    static constexpr std::array<const char*, slot_count> slot_names = {
        "content",
        "tool",
        "brush",
        "rendertarget",
        "grid"
    };

    [[nodiscard]] auto get_name() const -> const std::string&;

    void reset();

    std::size_t                               slot         {slot_count};
    uint32_t                                  mask         {0};
    bool                                      valid        {false};
    const Node_raytrace*                      raytrace_node{nullptr};
    std::shared_ptr<erhe::scene::Mesh>        mesh         {};
    const Grid*                               grid         {nullptr};
    std::shared_ptr<erhe::geometry::Geometry> geometry     {};
    std::optional<glm::vec3>                  position     {};
    std::optional<glm::vec3>                  normal       {};
    std::optional<glm::vec2>                  uv           {};
    std::size_t                               primitive    {std::numeric_limits<std::size_t>::max()};
    std::size_t                               local_index  {std::numeric_limits<std::size_t>::max()};
};

class Viewport_window;

class Scene_view
{
public:
    // Virtual interface
    [[nodiscard]] virtual auto get_scene_root        () const -> std::shared_ptr<Scene_root> = 0;
    [[nodiscard]] virtual auto get_camera            () const -> std::shared_ptr<erhe::scene::Camera> = 0;
    [[nodiscard]] virtual auto get_shadow_render_node() const -> Shadow_render_node* { return nullptr; }
    [[nodiscard]] virtual auto get_shadow_texture    () const -> erhe::graphics::Texture*;
    [[nodiscard]] virtual auto get_rendergraph_node  () -> std::shared_ptr<erhe::application::Rendergraph_node> = 0;
    [[nodiscard]] virtual auto get_light_projections () const -> const Light_projections*;
    [[nodiscard]] virtual auto as_viewport_window    () -> Viewport_window*;
    [[nodiscard]] virtual auto as_viewport_window    () const -> const Viewport_window*;

    // "Pointing"
    void set_world_from_control(
        glm::vec3 near_position_in_world,
        glm::vec3 far_position_in_world
    );

    void set_world_from_control     (const glm::mat4& world_from_control);
    void reset_control_transform    ();
    void reset_hover_slots          ();
    void update_hover_with_id_render();
    void update_hover_with_raytrace ();
    void update_grid_hover          ();

    [[nodiscard]] auto get_world_from_control                   () const -> std::optional<glm::mat4>;
    [[nodiscard]] auto get_control_from_world                   () const -> std::optional<glm::mat4>;
    [[nodiscard]] auto get_control_ray_origin_in_world          () const -> std::optional<glm::vec3>;
    [[nodiscard]] auto get_control_ray_direction_in_world       () const -> std::optional<glm::vec3>;
    [[nodiscard]] auto get_control_position_in_world_at_distance(float distance) const -> std::optional<glm::vec3>;
    [[nodiscard]] auto get_hover                                (std::size_t slot) const -> const Hover_entry&;
    [[nodiscard]] auto get_nearest_hover                        (uint32_t slot_mask) const -> const Hover_entry&;

protected:
    void set_hover(std::size_t slot, const Hover_entry& entry);

    std::optional<glm::mat4> m_world_from_control;
    std::optional<glm::mat4> m_control_from_world;

private:
    std::array<Hover_entry, Hover_entry::slot_count> m_hover_entries;
};

} // namespace editor
