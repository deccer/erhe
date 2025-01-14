#pragma once

#include "tools/tool.hpp"

#include "erhe/scene/node.hpp"

#include <glm/glm.hpp>

namespace erhe::application
{
    class Line_renderer_set;
}

namespace editor
{

class Selection_tool;

// TODO Negative half planes
enum class Grid_plane_type : unsigned int
{
    XZ = 0,
    XY,
    YZ,
    Node
};

static constexpr const char* grid_plane_type_strings[] =
{
    "XZ-Plane Y+",
    "XY-Plane Z+",
    "YZ-Plane X+",
    "Node"
};

auto get_plane_transform(Grid_plane_type plane_type) -> glm::mat4;

class Grid
    : public erhe::scene::Node_attachment
{
public:
    Grid();

    // Implements Node_attachment
    [[nodiscard]] auto get_type () const -> uint64_t    override;
    [[nodiscard]] auto type_name() const -> const char* override;

    // Public API
    [[nodiscard]] auto get_name           () const -> const std::string&;
    [[nodiscard]] auto snap_world_position(const glm::vec3& position_in_world) const -> glm::vec3;
    [[nodiscard]] auto snap_grid_position (const glm::vec3& position_in_grid ) const -> glm::vec3;
    [[nodiscard]] auto world_from_grid    () const -> glm::mat4;
    [[nodiscard]] auto grid_from_world    () const -> glm::mat4;
    [[nodiscard]] auto intersect_ray(
        const glm::vec3& ray_origin_in_world,
        const glm::vec3& ray_direction_in_world
    ) -> std::optional<glm::vec3>;

    void render(const Render_context& context);
    void imgui ();

private:
    std::string     m_name;
    Grid_plane_type m_plane_type      {Grid_plane_type::XZ};
    float           m_rotation        {0.0f}; // Used only if plane type != node
    glm::vec3       m_center          {0.0f}; // Used only if plane type != node
    bool            m_enable          {true};
    bool            m_see_hidden_major{false};
    bool            m_see_hidden_minor{false};
    float           m_cell_size       {1.0f};
    int             m_cell_div        {2};
    int             m_cell_count      {10};
    float           m_major_width     {4.0f};
    float           m_minor_width     {2.0f};
    glm::vec4       m_major_color     {0.0f, 0.0f, 0.0f, 0.729f};
    glm::vec4       m_minor_color     {0.0f, 0.0f, 0.0f, 0.737f};
    glm::mat4       m_world_from_grid {1.0f};
    glm::mat4       m_grid_from_world {1.0f};
};


} // namespace editor
