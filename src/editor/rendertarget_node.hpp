#pragma once

#include "renderers/renderpass.hpp"
#if defined(ERHE_XR_LIBRARY_OPENXR)
#   include "xr/hand_tracker.hpp"
#endif

#include "erhe/scene/mesh.hpp"

#include <glm/glm.hpp>

#include <string_view>

namespace erhe::components
{
    class Components;
}

namespace erhe::graphics
{
    class Framebuffer;
    //class OpenGL_state_tracker;
    class Sampler;
    class Texture;
}

namespace erhe::primitive
{
    class Material;
}

namespace erhe::scene
{
    class Mesh;
}

namespace editor
{

class Forward_renderer;
//// class Node_raytrace;
class Hand_tracker;
class Render_context;
class Scene_root;
class Viewport_config;
class Viewport_window;

/// <summary>
/// A textured quad mesh and framebuffer for rendering into
/// </summary>
/// TODO Should this be a Rendergraph_node?
class Rendertarget_node
    : public erhe::scene::Mesh
{
public:
    Rendertarget_node(
        Scene_root&                         host_scene_root,
        Viewport_window&                    host_viewport_window,
        const erhe::components::Components& components,
        const int                           width,
        const int                           height,
        const double                        dots_per_meter
    );

    // Public API
    [[nodiscard]] auto texture        () const -> std::shared_ptr<erhe::graphics::Texture>;
    [[nodiscard]] auto framebuffer    () const -> std::shared_ptr<erhe::graphics::Framebuffer>;
    [[nodiscard]] auto width          () const -> float;
    [[nodiscard]] auto height         () const -> float;
    [[nodiscard]] auto get_pointer    () const -> std::optional<glm::vec2>;
    [[nodiscard]] auto world_to_window(glm::vec3 world_position) const -> std::optional<glm::vec2>;

#if defined(ERHE_XR_LIBRARY_OPENXR)
    void update_headset(Headset_renderer& headset_renderer);
    [[nodiscard]] auto get_pointer_finger    () const -> std::optional<Finger_point>;
    [[nodiscard]] auto get_finger_trigger    () const -> bool;
    [[nodiscard]] auto get_controller_pose   () const -> const erhe::xr::Pose&;
    [[nodiscard]] auto get_controller_trigger() const -> float;
#endif

    auto update_pointer() -> bool;
    void bind          ();
    void clear         (glm::vec4 clear_color);
    void render_done   (); // generates mipmaps, updates lod bias

private:
    void init_rendertarget(const int width, const int height);
    void add_primitive    (const erhe::components::Components& components);

    Scene_root&                                  m_host_scene_root;
    Viewport_window&                             m_host_viewport_window;

    std::shared_ptr<Viewport_config>             m_viewport_config;
    double                                       m_dots_per_meter{0.0};
    double                                       m_local_width   {0.0};
    double                                       m_local_height  {0.0};
    //// std::shared_ptr<Node_raytrace>                      m_node_raytrace;
    std::shared_ptr<erhe::graphics::Texture>     m_texture;
    std::shared_ptr<erhe::graphics::Sampler>     m_sampler;
    std::shared_ptr<erhe::primitive::Material>   m_material;
    std::shared_ptr<erhe::graphics::Framebuffer> m_framebuffer;
    std::optional<glm::vec2>                     m_pointer;

#if defined(ERHE_XR_LIBRARY_OPENXR)
    std::optional<Finger_point> m_pointer_finger;
    bool                        m_finger_trigger          {false};
    erhe::xr::Pose              m_controller_pose         {};
    float                       m_controller_trigger_value{0.0f};
#endif
};

[[nodiscard]] auto is_rendertarget(const erhe::scene::Node* const node) -> bool;
[[nodiscard]] auto is_rendertarget(const std::shared_ptr<erhe::scene::Node>& node) -> bool;
[[nodiscard]] auto as_rendertarget(erhe::scene::Node* const node) -> Rendertarget_node*;
[[nodiscard]] auto as_rendertarget(const std::shared_ptr<erhe::scene::Node>& node) -> std::shared_ptr<Rendertarget_node>;

} // namespace erhe::application
