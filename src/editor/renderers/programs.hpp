#pragma once

#include "erhe/components/components.hpp"

#include <memory>

namespace erhe::graphics
{
    class Sampler;
    class Shader_resource;
    class Shader_stages;
}

namespace editor {

enum class Shader_stages_variant : int
{
    standard,
    anisotropic_slope,
    anisotropic_engine_ready,
    circular_brushed_metal,
    debug_depth,
    debug_normal,
    debug_tangent,
    debug_bitangent,
    debug_texcoord,
    debug_vertex_color_rgb,
    debug_vertex_color_alpha,
    debug_omega_o,
    debug_omega_i,
    debug_omega_g,
    debug_misc
};

static constexpr const char* c_shader_stages_variant_strings[] =
{
    "Standard",
    "Anisotropic Slope",
    "Anisotropic Engine-Ready",
    "Circular Brushed Metal",
    "Debug Depth",
    "Debug Normal",
    "Debug Tangent",
    "Debug Bitangent",
    "Debug TexCoord",
    "Debug Vertex Color RGB",
    "Debug Vertex Color Alpha",
    "Debug Omega o",
    "Debug Omega i",
    "Debug Omega g",
    "Debug Miscellaneous"
};

class IPrograms
{
public:
    virtual ~IPrograms() noexcept;

    static constexpr std::size_t s_texture_unit_count = 15; // for non bindless textures

    // Public members
    std::unique_ptr<erhe::graphics::Shader_resource> shadow_map_default_uniform_block; // for non-bindless textures
    std::unique_ptr<erhe::graphics::Shader_resource> textured_default_uniform_block;   // for non-bindless textures
    int                                              shadow_texture_unit{15};
    int                                              base_texture_unit{0};
    std::unique_ptr<erhe::graphics::Sampler>         nearest_sampler;
    std::unique_ptr<erhe::graphics::Sampler>         linear_sampler;
    std::unique_ptr<erhe::graphics::Sampler>         linear_mipmap_linear_sampler;

    std::unique_ptr<erhe::graphics::Shader_stages> brdf_slice;
    std::unique_ptr<erhe::graphics::Shader_stages> brush;
    std::unique_ptr<erhe::graphics::Shader_stages> standard;
    std::unique_ptr<erhe::graphics::Shader_stages> anisotropic_slope;
    std::unique_ptr<erhe::graphics::Shader_stages> anisotropic_engine_ready;
    std::unique_ptr<erhe::graphics::Shader_stages> circular_brushed_metal;
    std::unique_ptr<erhe::graphics::Shader_stages> textured;
    std::unique_ptr<erhe::graphics::Shader_stages> sky;
    std::unique_ptr<erhe::graphics::Shader_stages> wide_lines_draw_color;
    std::unique_ptr<erhe::graphics::Shader_stages> wide_lines_vertex_color;
    std::unique_ptr<erhe::graphics::Shader_stages> points;
    std::unique_ptr<erhe::graphics::Shader_stages> depth;
    std::unique_ptr<erhe::graphics::Shader_stages> id;
    std::unique_ptr<erhe::graphics::Shader_stages> tool;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_depth;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_normal;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_tangent;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_bitangent;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_texcoord;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_vertex_color_rgb;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_vertex_color_alpha;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_omega_o;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_omega_i;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_omega_g;
    std::unique_ptr<erhe::graphics::Shader_stages> debug_misc;
};

class Programs_impl;

class Programs
    : public erhe::components::Component
{
public:
    static constexpr std::string_view c_type_name{"Programs"};
    static constexpr uint32_t c_type_hash = compiletime_xxhash::xxh32(c_type_name.data(), c_type_name.size(), {});

    Programs      ();
    ~Programs     () noexcept override;
    Programs      (const Programs&) = delete;
    void operator=(const Programs&) = delete;
    Programs      (Programs&&)      = delete;
    void operator=(Programs&&)      = delete;

    // Implements Component
    [[nodiscard]] auto get_type_hash() const -> uint32_t override { return c_type_hash; }
    void declare_required_components() override;
    void initialize_component       () override;
    void deinitialize_component     () override;

private:
    std::unique_ptr<Programs_impl> m_impl;
};

extern IPrograms* g_programs;

}
