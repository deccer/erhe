#include "erhe/application/renderers/line_renderer.hpp"
#include "erhe/application/configuration.hpp"
#include "erhe/application/graphics/gl_context_provider.hpp"
#include "erhe/application/graphics/shader_monitor.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/application/windows/log_window.hpp"
#include "erhe/application/application_log.hpp"

#include "erhe/gl/enum_bit_mask_operators.hpp"
#include "erhe/gl/wrapper_functions.hpp"
#include "erhe/graphics/buffer.hpp"
#include "erhe/graphics/debug.hpp"
#include "erhe/graphics/instance.hpp"
#include "erhe/graphics/opengl_state_tracker.hpp"
#include "erhe/graphics/shader_stages.hpp"
#include "erhe/graphics/shader_resource.hpp"
#include "erhe/graphics/vertex_attribute_mappings.hpp"
#include "erhe/graphics/vertex_format.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/viewport.hpp"
#include "erhe/toolkit/math_util.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <iomanip>
#include <cstdarg>

namespace erhe::application
{

namespace
{

static constexpr gl::Buffer_storage_mask storage_mask_persistent{
    gl::Buffer_storage_mask::map_coherent_bit   |
    gl::Buffer_storage_mask::map_persistent_bit |
    gl::Buffer_storage_mask::map_write_bit
};
static constexpr gl::Buffer_storage_mask storage_mask_not_persistent{
    gl::Buffer_storage_mask::map_write_bit
};
inline auto storage_mask() -> gl::Buffer_storage_mask
{
    return erhe::graphics::Instance::info.use_persistent_buffers
        ? storage_mask_persistent
        : storage_mask_not_persistent;
}

static constexpr gl::Map_buffer_access_mask access_mask_persistent{
    gl::Map_buffer_access_mask::map_coherent_bit   |
    gl::Map_buffer_access_mask::map_persistent_bit |
    gl::Map_buffer_access_mask::map_write_bit
};
static constexpr gl::Map_buffer_access_mask access_mask_not_persistent{
    gl::Map_buffer_access_mask::map_write_bit
};
inline auto access_mask() -> gl::Map_buffer_access_mask
{
    return erhe::graphics::Instance::info.use_persistent_buffers
        ? access_mask_persistent
        : access_mask_not_persistent;
}

}

using erhe::graphics::Shader_stages;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

Line_renderer_pipeline::Line_renderer_pipeline()
    : fragment_outputs{
        erhe::graphics::Fragment_output{
            .name     = "out_color",
            .type     = gl::Fragment_shader_output_type::float_vec4,
            .location = 0
        }
    }
    , attribute_mappings{
        erhe::graphics::Vertex_attribute_mapping{
            .layout_location = 0,
            .shader_type     = gl::Attribute_type::float_vec4,
            .name            = "a_position",
            .src_usage =
            {
                .type        = erhe::graphics::Vertex_attribute::Usage_type::position
            },
        },
        erhe::graphics::Vertex_attribute_mapping{
            .layout_location = 1,
            .shader_type     = gl::Attribute_type::float_vec4,
            .name            = "a_color",
            .src_usage =
            {
                .type        = erhe::graphics::Vertex_attribute::Usage_type::color
            }
        }
    }
    , vertex_format{
        erhe::graphics::Vertex_attribute{
            .usage =
            {
                .type      = erhe::graphics::Vertex_attribute::Usage_type::position
            },
            .shader_type   = gl::Attribute_type::float_vec4,
            .data_type =
            {
                .type      = gl::Vertex_attrib_type::float_,
                .dimension = 4
            }
        },
        erhe::graphics::Vertex_attribute{
            .usage =
            {
                .type       = erhe::graphics::Vertex_attribute::Usage_type::color,
            },
            .shader_type    = gl::Attribute_type::float_vec4,
            .data_type =
            {
                .type       = gl::Vertex_attrib_type::float_,
                .dimension  = 4
            }
        }
    }
{
}

Line_renderer_set* g_line_renderer_set{nullptr};

Line_renderer_set::Line_renderer_set()
    : Component{c_type_name}
{
}

Line_renderer_set::~Line_renderer_set() noexcept
{
    ERHE_VERIFY(g_line_renderer_set == nullptr);
}

void Line_renderer_set::deinitialize_component()
{
    ERHE_VERIFY(g_line_renderer_set == this);
    for (auto& i : visible) {
        i.reset();
    }
    for (auto& i : hidden) {
        i.reset();
    }
    m_pipeline.reset();
    g_line_renderer_set = nullptr;
}

void Line_renderer_set::declare_required_components()
{
    require<Gl_context_provider>();
    require<Configuration      >();
    require<Shader_monitor     >();
}

static constexpr std::string_view c_line_renderer_initialize_component{"Line_renderer_set::initialize_component()"};

void Line_renderer_set::initialize_component()
{
    ERHE_PROFILE_FUNCTION
    ERHE_VERIFY(g_line_renderer_set == nullptr);

    const Scoped_gl_context gl_context;

    erhe::graphics::Scoped_debug_group line_renderer_initialization{c_line_renderer_initialize_component};

    m_pipeline = Line_renderer_pipeline();
    m_pipeline->initialize();

    for (unsigned int stencil_reference = 0; stencil_reference <= s_max_stencil_reference; ++stencil_reference) {
        visible.at(stencil_reference) = std::make_unique<Line_renderer>("visible", 8u + stencil_reference, &m_pipeline.value());
        hidden .at(stencil_reference) = std::make_unique<Line_renderer>("hidden",  8u + stencil_reference, &m_pipeline.value());
    }

    g_line_renderer_set = this;
}

void Line_renderer_pipeline::initialize()
{
    view_block = std::make_unique<erhe::graphics::Shader_resource>(
        "view",
        0,
        erhe::graphics::Shader_resource::Type::uniform_block
    );

    clip_from_world_offset        = view_block->add_mat4("clip_from_world"       )->offset_in_parent();
    view_position_in_world_offset = view_block->add_vec4("view_position_in_world")->offset_in_parent();
    viewport_offset               = view_block->add_vec4("viewport"              )->offset_in_parent();
    fov_offset                    = view_block->add_vec4("fov"                   )->offset_in_parent();

    {
        ERHE_PROFILE_SCOPE("shader");

        const auto shader_path = std::filesystem::path("res") / std::filesystem::path("shaders");
        const std::filesystem::path vs_path = shader_path / std::filesystem::path("line.vert");
        const std::filesystem::path gs_path = shader_path / std::filesystem::path("line.geom");
        const std::filesystem::path fs_path = shader_path / std::filesystem::path("line.frag");
        Shader_stages::Create_info create_info{
            .name                      = "line",
            .defines                   = {
                { "ERHE_LINE_SHADER_SHOW_DEBUG_LINES",        "0"},
                { "ERHE_LINE_SHADER_PASSTHROUGH_BASIC_LINES", "0"},
                { "ERHE_LINE_SHADER_STRIP",                   "1"}
            },
            .interface_blocks          = { view_block.get() },
            .vertex_attribute_mappings = &attribute_mappings,
            .fragment_outputs          = &fragment_outputs,
            .shaders = {
                { gl::Shader_type::vertex_shader,   vs_path },
                { gl::Shader_type::geometry_shader, gs_path },
                { gl::Shader_type::fragment_shader, fs_path }
            }
        };

        Shader_stages::Prototype prototype(create_info);
        if (prototype.is_valid()) {
            shader_stages = std::make_unique<Shader_stages>(std::move(prototype));

            if (g_shader_monitor != nullptr) {
                g_shader_monitor->add(create_info, shader_stages.get());
            }
        } else {
            const auto current_path = std::filesystem::current_path();
            log_startup->error(
                "Unable to load Line_renderer shader - check working directory '{}'",
                current_path.string()
            );
        }
    }
}

void Line_renderer_set::begin()
{
    for (auto& entry : visible) entry->begin();
    for (auto& entry : hidden ) entry->begin();
}

void Line_renderer_set::end()
{
    for (auto& entry : visible) entry->end();
    for (auto& entry : hidden ) entry->end();
}

void Line_renderer_set::next_frame()
{
    for (auto& entry : visible) entry->next_frame();
    for (auto& entry : hidden ) entry->next_frame();
}

void Line_renderer_set::render(
    const erhe::scene::Viewport viewport,
    const erhe::scene::Camera&  camera
)
{
    for (auto& entry: hidden ) entry->render(viewport, camera, true, false);
    for (auto& entry: visible) entry->render(viewport, camera, true, true);
    erhe::graphics::g_opengl_state_tracker->depth_stencil.reset(); // workaround issue in stencil state tracking
}

[[nodiscard]] auto Line_renderer::Frame_resources::make_pipeline(
    const bool                           reverse_depth,
    erhe::graphics::Shader_stages* const shader_stages,
    const bool                           visible,
    const unsigned int                   stencil_reference
) -> erhe::graphics::Pipeline
{
    const gl::Depth_function depth_compare_op0 = visible ? gl::Depth_function::less : gl::Depth_function::gequal;
    const gl::Depth_function depth_compare_op  = reverse_depth ? erhe::graphics::reverse(depth_compare_op0) : depth_compare_op0;
    return erhe::graphics::Pipeline{
        {
            .name           = "Line Renderer depth pass",
            .shader_stages  = shader_stages,
            .vertex_input   = &vertex_input,
            .input_assembly = erhe::graphics::Input_assembly_state::lines,
            .rasterization  = erhe::graphics::Rasterization_state::cull_mode_none,
            .depth_stencil  = {
                .depth_test_enable   = true,
                .depth_write_enable  = false,
                .depth_compare_op    = depth_compare_op,
                .stencil_test_enable = true,
                .stencil_front = {
                    .stencil_fail_op = gl::Stencil_op::keep,
                    .z_fail_op       = gl::Stencil_op::keep,
                    .z_pass_op       = gl::Stencil_op::replace,
                    .function        = gl::Stencil_function::gequal,
                    .reference       = stencil_reference,
                    .test_mask       = 0xffu,
                    .write_mask      = 0xffu
                },
                .stencil_back = {
                    .stencil_fail_op = gl::Stencil_op::keep,
                    .z_fail_op       = gl::Stencil_op::keep,
                    .z_pass_op       = gl::Stencil_op::replace,
                    .function        = gl::Stencil_function::gequal,
                    .reference       = stencil_reference,
                    .test_mask       = 0xffu,
                    .write_mask      = 0xffu
                },
            },

            .color_blend    = visible ? erhe::graphics::Color_blend_state::color_blend_premultiplied : erhe::graphics::Color_blend_state{
                .enabled  = true,
                .rgb      = {
                    .equation_mode      = gl::Blend_equation_mode::func_add,
                    .source_factor      = gl::Blending_factor::constant_alpha,
                    .destination_factor = gl::Blending_factor::one_minus_constant_alpha
                },
                .alpha    = {
                    .equation_mode      = gl::Blend_equation_mode::func_add,
                    .source_factor      = gl::Blending_factor::constant_alpha,
                    .destination_factor = gl::Blending_factor::one_minus_constant_alpha
                },
                .constant = { 0.0f, 0.0f, 0.0f, 0.1f },
            }
        }
    };
}

Line_renderer::Frame_resources::Frame_resources(
    const unsigned int                        stencil_reference,
    const bool                                reverse_depth,
    const std::size_t                         view_stride,
    const std::size_t                         view_count,
    const std::size_t                         vertex_count,
    erhe::graphics::Shader_stages* const      shader_stages,
    erhe::graphics::Vertex_attribute_mappings attribute_mappings,
    erhe::graphics::Vertex_format&            vertex_format,
    const std::string&                        style_name,
    const std::size_t                         slot
)
    : vertex_buffer{
        gl::Buffer_target::array_buffer,
        vertex_format.stride() * vertex_count,
        storage_mask(),
        access_mask()
    }
    , view_buffer{
        gl::Buffer_target::uniform_buffer,
        view_stride * view_count,
        storage_mask(),
        access_mask()
    }
    , vertex_input{
        erhe::graphics::Vertex_input_state_data::make(
            attribute_mappings,
            vertex_format,
            &vertex_buffer,
            nullptr
        )
    }
    , pipeline_visible{make_pipeline(reverse_depth, shader_stages, true,  stencil_reference)}
    , pipeline_hidden {make_pipeline(reverse_depth, shader_stages, false, stencil_reference)}
{
    vertex_buffer.set_debug_label(fmt::format("Line Renderer {} Vertex {}", style_name, slot));
    view_buffer  .set_debug_label(fmt::format("Line Renderer {} View {}",   style_name, slot));
}

Line_renderer::Line_renderer(
    const char* const       name,
    const unsigned int      stencil_reference,
    Line_renderer_pipeline* pipeline
)
    : m_name    {name}
    , m_pipeline{pipeline}
{
    ERHE_PROFILE_FUNCTION

    const auto            reverse_depth = g_configuration->graphics.reverse_depth;
    constexpr std::size_t vertex_count  = 512 * 1024;
    constexpr std::size_t view_stride   = 256;
    constexpr std::size_t view_count    = 16;
    for (std::size_t slot = 0; slot < s_frame_resources_count; ++slot) {
        m_frame_resources.emplace_back(
            stencil_reference,
            reverse_depth,
            view_stride,
            view_count,
            vertex_count,
            pipeline->shader_stages.get(),
            pipeline->attribute_mappings,
            pipeline->vertex_format,
            m_name,
            slot
        );
    }
}

auto Line_renderer::current_frame_resources() -> Frame_resources&
{
    return m_frame_resources[m_current_frame_resource_slot];
}

void Line_renderer::next_frame()
{
    ERHE_VERIFY(!m_inside_begin_end);
    m_current_frame_resource_slot = (m_current_frame_resource_slot + 1) % s_frame_resources_count;
    m_view_writer  .reset();
    m_vertex_writer.reset();
    m_line_count = 0;
}

void Line_renderer::begin()
{
    ERHE_VERIFY(!m_inside_begin_end);

    m_vertex_writer.begin(&current_frame_resources().vertex_buffer);
    m_line_count       = 0;
    m_inside_begin_end = true;
}

void Line_renderer::end()
{
    ERHE_VERIFY(m_inside_begin_end);

    m_inside_begin_end = false;
    //m_view_writer  .end();
    m_vertex_writer.end();
}

void Line_renderer::put(
    const vec3&             point,
    const float             thickness,
    const vec4&             color,
    const gsl::span<float>& gpu_float_data,
    std::size_t&            word_offset
)
{
    gpu_float_data[word_offset++] = point.x;
    gpu_float_data[word_offset++] = point.y;
    gpu_float_data[word_offset++] = point.z;
    gpu_float_data[word_offset++] = thickness;
    gpu_float_data[word_offset++] = color.r;
    gpu_float_data[word_offset++] = color.g;
    gpu_float_data[word_offset++] = color.b;
    gpu_float_data[word_offset++] = color.a;
}

void Line_renderer::add_lines(
    const mat4&                       transform,
    const std::initializer_list<Line> lines
)
{
    ERHE_VERIFY(m_inside_begin_end);

    auto                   vertex_gpu_data = current_frame_resources().vertex_buffer.map();
    std::byte* const       start           = vertex_gpu_data.data();
    const std::size_t      byte_count      = vertex_gpu_data.size_bytes();
    const std::size_t      word_count      = byte_count / sizeof(float);
    const gsl::span<float> gpu_float_data{reinterpret_cast<float*>(start), word_count};

    std::size_t word_offset = 0;
    for (const Line& line : lines) {
        const vec4 p0{transform * vec4{line.p0, 1.0f}};
        const vec4 p1{transform * vec4{line.p1, 1.0f}};
        put(vec3{p0} / p0.w, m_line_thickness, m_line_color, gpu_float_data, word_offset);
        put(vec3{p1} / p1.w, m_line_thickness, m_line_color, gpu_float_data, word_offset);
    }

    m_vertex_writer.write_offset += lines.size() * 2 * m_pipeline->vertex_format.stride();
    m_line_count += lines.size();
}

void Line_renderer::add_lines(
    const mat4&                        transform,
    const std::initializer_list<Line4> lines
)
{
    ERHE_VERIFY(m_inside_begin_end);

    auto                   vertex_gpu_data = current_frame_resources().vertex_buffer.map();
    std::byte* const       start           = vertex_gpu_data.data();
    const std::size_t      byte_count      = vertex_gpu_data.size_bytes();
    const std::size_t      word_count      = byte_count / sizeof(float);
    const gsl::span<float> gpu_float_data{reinterpret_cast<float*>(start), word_count};

    std::size_t word_offset = 0;
    for (const Line4& line : lines) {
        const vec4 p0{transform * vec4{vec3{line.p0}, 1.0f}};
        const vec4 p1{transform * vec4{vec3{line.p1}, 1.0f}};
        put(vec3{p0} / p0.w, line.p0.w, m_line_color, gpu_float_data, word_offset);
        put(vec3{p1} / p1.w, line.p1.w, m_line_color, gpu_float_data, word_offset);
    }

    m_vertex_writer.write_offset += lines.size() * 2 * m_pipeline->vertex_format.stride();
    m_line_count += lines.size();
}

void Line_renderer::set_line_color(const float r, const float g, const float b, const float a)
{
    ERHE_VERIFY(m_inside_begin_end);

    m_line_color = vec4{r, g, b, a};
}

void Line_renderer::set_line_color(const vec3& color)
{
    ERHE_VERIFY(m_inside_begin_end);

    m_line_color = vec4{color, 1.0f};
}

void Line_renderer::set_line_color(const vec4& color)
{
    m_line_color = color;
}

#if defined(ERHE_GUI_LIBRARY_IMGUI)
void Line_renderer::set_line_color(const ImVec4 color)
{
    ERHE_VERIFY(m_inside_begin_end);

    m_line_color = vec4{color.x, color.y, color.z, color.w};
}
#endif

void Line_renderer::set_thickness(const float thickness)
{
    ERHE_VERIFY(m_inside_begin_end);

    m_line_thickness = thickness;
}

void Line_renderer::add_lines(
    const std::initializer_list<Line> lines
)
{
    ERHE_VERIFY(m_inside_begin_end);

    auto                   vertex_gpu_data = current_frame_resources().vertex_buffer.map();
    std::byte* const       start           = vertex_gpu_data.data();
    const std::size_t      byte_count      = vertex_gpu_data.size_bytes();
    const std::size_t      word_count      = byte_count / sizeof(float);
    const gsl::span<float> gpu_float_data{reinterpret_cast<float*>(start), word_count};

    std::size_t word_offset = 0;
    for (const Line& line : lines) {
        put(line.p0, m_line_thickness, m_line_color, gpu_float_data, word_offset);
        put(line.p1, m_line_thickness, m_line_color, gpu_float_data, word_offset);
    }

    m_vertex_writer.write_offset += lines.size() * 2 * m_pipeline->vertex_format.stride();
    m_line_count += lines.size();
}

void Line_renderer::add_cube(
    const mat4& transform,
    const vec4& color,
    const vec3& min_corner,
    const vec3& max_corner,
    const bool  z_cross
)
{
    const auto a = min_corner;
    const auto b = max_corner;
    vec3 p[8] = {
        vec3{a.x, a.y, a.z},
        vec3{b.x, a.y, a.z},
        vec3{b.x, b.y, a.z},
        vec3{a.x, b.y, a.z},
        vec3{a.x, a.y, b.z},
        vec3{b.x, a.y, b.z},
        vec3{b.x, b.y, b.z},
        vec3{a.x, b.y, b.z}
    };
    add_lines(
        transform,
        color,
        {
            // near plane
            { p[0], p[1] },
            { p[1], p[2] },
            { p[2], p[3] },
            { p[3], p[0] },

            // far plane
            { p[4], p[5] },
            { p[5], p[6] },
            { p[6], p[7] },
            { p[7], p[4] },

            // near to far
            { p[0], p[4] },
            { p[1], p[5] },
            { p[2], p[6] },
            { p[3], p[7] }
        }
    );
    if (z_cross)
    {
        add_lines(
            transform,
            color,
            {
                // near to far middle
                { 0.5f * p[0] + 0.5f * p[1], 0.5f * p[4] + 0.5f * p[5] },
                { 0.5f * p[1] + 0.5f * p[2], 0.5f * p[5] + 0.5f * p[6] },
                { 0.5f * p[2] + 0.5f * p[3], 0.5f * p[6] + 0.5f * p[7] },
                { 0.5f * p[3] + 0.5f * p[0], 0.5f * p[7] + 0.5f * p[4] },

                // near+far/2 plane
                { 0.5f * p[0] + 0.5f * p[4], 0.5f * p[1] + 0.5f * p[5] },
                { 0.5f * p[1] + 0.5f * p[5], 0.5f * p[2] + 0.5f * p[6] },
                { 0.5f * p[2] + 0.5f * p[6], 0.5f * p[3] + 0.5f * p[7] },
                { 0.5f * p[3] + 0.5f * p[7], 0.5f * p[0] + 0.5f * p[4] },
            }
        );
    }
}

void Line_renderer::add_sphere(
    const erhe::scene::Transform&       transform,
    const vec4&                         edge_color,
    const vec4&                         great_circle_color,
    const float                         edge_thickness,
    const float                         great_circle_thickness,
    const vec3&                         local_center,
    const float                         radius,
    const erhe::scene::Transform* const camera_world_from_node,
    const int                           step_count
)
{
    const mat4 m      = transform.matrix();
    const vec3 center = vec3{m * vec4{local_center, 1.0f}};
    const vec3 axis_x{radius, 0.0f, 0.0f};
    const vec3 axis_y{0.0f, radius, 0.0f};
    const vec3 axis_z{0.0f, 0.0f, radius};
    const mat4 I{1.0f};
    set_thickness(great_circle_thickness);
    for (int i = 0; i < step_count; ++i) {
        const float t0 = glm::two_pi<float>() * static_cast<float>(i    ) / static_cast<float>(step_count);
        const float t1 = glm::two_pi<float>() * static_cast<float>(i + 1) / static_cast<float>(step_count);
        add_lines(
            great_circle_color,
            {
                {
                    center + std::cos(t0) * axis_x + std::sin(t0) * axis_y,
                    center + std::cos(t1) * axis_x + std::sin(t1) * axis_y
                },
                {
                    center + std::cos(t0) * axis_y + std::sin(t0) * axis_z,
                    center + std::cos(t1) * axis_y + std::sin(t1) * axis_z
                },
                {
                    center + std::cos(t0) * axis_x + std::sin(t0) * axis_z,
                    center + std::cos(t1) * axis_x + std::sin(t1) * axis_z
                }
            }
        );
        //add_lines(
        //    0xffff0000,
        //    {
        //        {
        //            center + std::cos(t0) * axis_x + std::sin(t0) * axis_y,
        //            center + std::cos(t1) * axis_x + std::sin(t1) * axis_y
        //        }
        //    }
        //);
        //add_lines(
        //    0xff0000ff,
        //    {
        //        {
        //            center + std::cos(t0) * axis_y + std::sin(t0) * axis_z,
        //            center + std::cos(t1) * axis_y + std::sin(t1) * axis_z
        //        }
        //    }
        //);
        //add_lines(
        //    0xff00ff00,
        //    {
        //        {
        //            center + std::cos(t0) * axis_x + std::sin(t0) * axis_z,
        //            center + std::cos(t1) * axis_x + std::sin(t1) * axis_z
        //        }
        //    }
        //);
    }

    if (camera_world_from_node == nullptr) {
        return;
    }

    //                             C = sphere center        .
    //                             r = sphere radius        .
    //         /|                  V = camera center        .
    //        / |  .               d = distance(C, V)       .
    //      r/  |     . b          d*d = r*r + b*b          .
    //      /   |h       .         d*d - r*r = b*b          .
    //     /    |           .      b = sqrt(d*d - r*r)      .
    //    /___p_|_____q________.   h = (r*b) / d            .
    //   C      P d             V  p*p + h*h = r*r          .
    //                             p = sqrt(r*r - h*h)      .

    const vec3 camera_position                 = vec3{camera_world_from_node->matrix() * vec4{0.0f, 0.0f, 0.0f, 1.0f}};
    const vec3 from_camera_to_sphere           = center - camera_position;
    const vec3 from_sphere_to_camera           = camera_position - center;
    const vec3 from_camera_to_sphere_direction = glm::normalize(from_camera_to_sphere);
    const vec3 from_sphere_to_camera_direction = glm::normalize(from_sphere_to_camera);

    const float r2 = radius * radius;
    const float d2 = glm::length2(from_camera_to_sphere);
    const float d  = std::sqrt(d2);
    const float b2 = d2 - r2;
    const float b  = std::sqrt(b2);
    const float h  = radius * b / d;
    const float h2 = h * h;
    const float p  = std::sqrt(r2 - h2);

    const vec3 P              = center + p * from_sphere_to_camera_direction;
    const vec3 up0_direction  = vec3{camera_world_from_node->matrix() * vec4{0.0f, 1.0f, 0.0f, 0.0f}};
    const vec3 side_direction = erhe::toolkit::safe_normalize_cross<float>(from_camera_to_sphere_direction, up0_direction);
    const vec3 up_direction   = erhe::toolkit::safe_normalize_cross<float>(side_direction, from_camera_to_sphere_direction);
    const vec3 axis_a         = h * side_direction;
    const vec3 axis_b         = h * up_direction;

    set_thickness(edge_thickness);
    for (int i = 0; i < step_count; ++i) {
        const float t0 = glm::two_pi<float>() * static_cast<float>(i    ) / static_cast<float>(step_count);
        const float t1 = glm::two_pi<float>() * static_cast<float>(i + 1) / static_cast<float>(step_count);
        add_lines(
            //0xffffffff,
            edge_color,
            {
                {
                    P + std::cos(t0) * axis_a + std::sin(t0) * axis_b,
                    P + std::cos(t1) * axis_a + std::sin(t1) * axis_b
                }
            }
        );
    }
}

auto sign(const float x) -> float
{
    return (x < 0.0f) ? -1.0f : (x == 0.0f) ? 0.0f : 1.0f;
}

auto sign(const double x) -> double
{
    return (x < 0.0) ? -1.0 : (x == 0.0) ? 0.0 : 1.0;
}

void Line_renderer::add_cone(
    const erhe::scene::Transform& world_from_node,
    const vec4&                   major_color,
    const vec4&                   minor_color,
    const float                   major_thickness,
    const float                   minor_thickness,
    const vec3&                   bottom_center,
    const float                   height,
    const float                   bottom_radius,
    const float                   top_radius,
    const vec3&                   camera_position_in_world,
    const int                     side_count
)
{
    constexpr vec3 axis_x       {1.0f, 0.0f, 0.0f};
    constexpr vec3 axis_y       {0.0f, 1.0f, 0.0f};
    constexpr vec3 axis_z       {0.0f, 0.0f, 1.0f};
    constexpr vec3 bottom_normal{0.0f, -1.0f, 0.0f};
    constexpr vec3 top_normal   {0.0f,  1.0f, 0.0f};

    const mat4 m                       = world_from_node.matrix();
    const mat4 node_from_world         = world_from_node.inverse_matrix();
    const vec3 top_center              = bottom_center + vec3{0.0f, height, 0.0f};
    const vec3 camera_position_in_node = vec4{node_from_world * vec4{camera_position_in_world, 1.0f}};

    set_thickness(major_thickness);

    class Cone_edge
    {
    public:
        Cone_edge(
            const vec3& p0,
            const vec3& p1,
            const vec3& n,
            const vec3& t,
            const vec3& b,
            const float phi,
            const float n_dot_v
        )
        : p0     {p0}
        , p1     {p1}
        , n      {n}
        , t      {t}
        , b      {b}
        , phi    {phi}
        , n_dot_v{n_dot_v}
        {
        }

        vec3  p0;
        vec3  p1;
        vec3  n;
        vec3  t;
        vec3  b;
        float phi;
        float n_dot_v;
    };

    std::vector<Cone_edge> cone_edges;
    for (int i = 0; i < side_count; ++i) {
        const float phi = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(side_count);
        const vec3  sin_phi_z = std::cos(phi) * axis_x;
        const vec3  cos_phi_x = std::sin(phi) * axis_z;

        const vec3 p0       {bottom_center + bottom_radius * cos_phi_x + bottom_radius * sin_phi_z};
        const vec3 p1       {top_center    + top_radius    * cos_phi_x + top_radius    * sin_phi_z};
        const vec3 mid_point{0.5f * (p0 + p1)};

        const vec3 B = normalize(p1 - p0); // generatrix
        const vec3 T{
            static_cast<float>(std::cos(phi + glm::half_pi<float>())),
            0.0f,
            static_cast<float>(std::sin(phi + glm::half_pi<float>()))
        };
        const vec3  N       = erhe::toolkit::safe_normalize_cross<float>(B, T);
        const vec3  v       = glm::normalize(camera_position_in_node - mid_point);
        const float n_dot_v = dot(N, v);

        cone_edges.emplace_back(
            p0,
            p1,
            N,
            T,
            B,
            phi,
            n_dot_v
        );
    }

    std::vector<Cone_edge> sign_flip_edges;

    const vec3  bottom_v       = glm::normalize(camera_position_in_node -bottom_center);
    const float bottom_n_dot_v = glm::dot(bottom_normal, bottom_v);
    const bool  bottom_visible = bottom_n_dot_v >= 0.0f;

    const vec3  top_v        = glm::normalize(camera_position_in_node - top_center);
    const float top_n_dot_v  = glm::dot(top_normal, top_v);
    const bool  top_visible  = top_n_dot_v >= 0.0f;

    set_thickness(minor_thickness);
    add_lines(
        m,
        minor_color,
        {
            {
                bottom_center - bottom_radius * axis_x,
                bottom_center + bottom_radius * axis_x,
            },
            {
                bottom_center - bottom_radius * axis_z,
                bottom_center + bottom_radius * axis_z
            },
            {
                top_center - top_radius * axis_x,
                top_center + top_radius * axis_x,
            },
            {
                top_center - top_radius * axis_z,
                top_center + top_radius * axis_z
            },
            {
                bottom_center,
                top_center
            },
            {
                bottom_center - bottom_radius * axis_x,
                top_center    - top_radius    * axis_x
            },
            {
                bottom_center + bottom_radius * axis_x,
                top_center    + top_radius    * axis_x
            },
            {
                bottom_center - bottom_radius * axis_z,
                top_center    - top_radius    * axis_z
            },
            {
                bottom_center + bottom_radius * axis_z,
                top_center    + top_radius    * axis_z
            }
        }
    );

    for (size_t i = 0; i < cone_edges.size(); ++i) {
        const std::size_t next_i      = (i + 1) % cone_edges.size();
        const auto&       edge        = cone_edges[i];
        const auto&       next_edge   = cone_edges[next_i];
        const float       avg_n_dot_v = 0.5f * edge.n_dot_v + 0.5f * next_edge.n_dot_v;
        if (sign(edge.n_dot_v) != sign(next_edge.n_dot_v)) {
            if (std::abs(edge.n_dot_v) < std::abs(next_edge.n_dot_v)) {
                sign_flip_edges.push_back(edge);
            } else {
                sign_flip_edges.push_back(next_edge);
            }
        }
        if (bottom_radius > 0.0f) {
            add_lines(
                m,
                bottom_visible || (avg_n_dot_v > 0.0)
                    ? major_color
                    : minor_color,
                {
                    {
                        edge.p0,
                        next_edge.p0
                    }
                }
            );
        }

        if (top_radius > 0.0f) {
            add_lines(
                m,
                top_visible || (avg_n_dot_v > 0.0)
                    ? major_color
                    : minor_color,
                {
                    {
                        edge.p1,
                        next_edge.p1
                    }
                }
            );
        }
    }

    for (auto& edge : sign_flip_edges) {
        add_lines(m, major_color, { { edge.p0, edge.p1 } } );
    }
}

namespace {

struct Torus_point
{
    vec3 p;
    vec3 n;
};

[[nodiscard]] auto torus_point(
    const double R,
    const double r,
    const double rel_major,
    const double rel_minor
) -> Torus_point
{
    const double theta     = (glm::pi<double>() * 2.0 * rel_major);
    const double phi       = (glm::pi<double>() * 2.0 * rel_minor);
    const double sin_theta = std::sin(theta);
    const double cos_theta = std::cos(theta);
    const double sin_phi   = std::sin(phi);
    const double cos_phi   = std::cos(phi);

    const double vx = (R + r * cos_phi) * cos_theta;
    const double vy = (R + r * cos_phi) * sin_theta;
    const double vz =      r * sin_phi;

    const double tx = -sin_theta;
    const double ty =  cos_theta;
    const double tz = 0.0f;
    const vec3   T{tx, ty, tz};

    const double bx = -sin_phi * cos_theta;
    const double by = -sin_phi * sin_theta;
    const double bz =  cos_phi;
    const vec3   B{bx, by, bz};
    const vec3   N = glm::normalize(glm::cross(T, B));

    return Torus_point{
        .p = vec3{vx, vy, vz},
        .n = N
    };
}

// Adapted from https://www.shadertoy.com/view/4sBGDy
//
// The MIT License
// Copyright (C) 2014 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// f(x) = (|x|^2 + R^2 - r^2)^2 - 4R^2|xy|^2 = 0
auto ray_torus_intersection(
    const vec3 ro,
    const vec3 rd,
    const vec2 tor
) -> float
{
    float po = 1.0;

    float Ra2 = tor.x * tor.x;
    float ra2 = tor.y * tor.y;

    float m = glm::dot(ro, ro);
    float n = glm::dot(ro, rd);

    // bounding sphere
    {
    	float h = n * n - m + (tor.x + tor.y) * (tor.x + tor.y);
    	if (h < 0.0f) {
            return -1.0f;
        }
    	//float t = -n-sqrt(h); // could use this to compute intersections from ro+t*rd
    }

	// find quartic equation
    float k  = (m - ra2 - Ra2) / 2.0f;
    float k3 = n;
    float k2 = n * n + Ra2 * rd.z * rd.z + k;
    float k1 = k * n + Ra2 * ro.z * rd.z;
    float k0 = k * k + Ra2 * ro.z * ro.z - Ra2 * ra2;

#if 1
    // prevent |c1| from being too close to zero
    if (std::abs(k3 * (k3 * k3 - k2) + k1) < 0.01) {
        po = -1.0f;
        float tmp = k1; k1 = k3; k3 = tmp;
        k0 = 1.0f / k0;
        k1 = k1 * k0;
        k2 = k2 * k0;
        k3 = k3 * k0;
    }
#endif

    float c2 = 2.0f * k2 - 3.0f * k3 * k3;
    float c1 = k3 * (k3 * k3 - k2) + k1;
    float c0 = k3 * (k3 * (-3.0f * k3 * k3 + 4.0f * k2) - 8.0f * k1) + 4.0f * k0;

    c2 /= 3.0f;
    c1 *= 2.0f;
    c0 /= 3.0f;

    float Q = c2 * c2 + c0;
    float R = 3.0f * c0 * c2 - c2 * c2 * c2 - c1 * c1;


    float h = R * R - Q * Q * Q;
    float z = 0.0f;
    if (h < 0.0f) {
    	// 4 intersections
        float sQ = std::sqrt(Q);
        z = 2.0f * sQ * std::cos(
            std::acos(R / (sQ * Q)) / 3.0f
        );
    } else {
        // 2 intersections
        float sQ = std::pow(
            std::sqrt(h) + std::abs(R),
            1.0f / 3.0f
        );
        z = sign(R) * std::abs(sQ + Q / sQ);
    }
    z = c2 - z;

    float d1 = z     - 3.0f * c2;
    float d2 = z * z - 3.0f * c0;
    if (std::abs(d1) < 1.0e-4) {
        if (d2 < 0.0f) {
            return -1.0f;
        }
        d2 = std::sqrt(d2);
    } else {
        if (d1 < 0.0f) {
            return -1.0f;
        }
        d1 = std::sqrt(d1 / 2.0f);
        d2 = c1 / d1;
    }

    //----------------------------------

    float result = 1e20;

    h = d1 * d1 - z + d2;
    if (h > 0.0f) {
        h = std::sqrt(h);
        float t1 = -d1 - h - k3; t1 = (po < 0.0f) ? 2.0f / t1 : t1;
        float t2 = -d1 + h - k3; t2 = (po < 0.0f) ? 2.0f / t2 : t2;
        if (t1 > 0.0f) result = t1;
        if (t2 > 0.0f) result = std::min(result, t2);
    }

    h = d1 * d1 - z - d2;
    if (h > 0.0f) {
        h = std::sqrt(h);
        float t1 = d1 - h - k3; t1 = (po < 0.0f) ? 2.0f / t1 : t1;
        float t2 = d1 + h - k3; t2 = (po < 0.0f) ? 2.0f / t2 : t2;
        if (t1 > 0.0f) result = std::min(result, t1);
        if (t2 > 0.0f) result = std::min(result, t2);
    }

    return result;
}

auto ray_torus_intersection(
    const glm::dvec3 ro,
    const glm::dvec3 rd,
    const glm::dvec2 tor
) -> double
{
    double po = 1.0;

    double Ra2 = tor.x * tor.x;
    double ra2 = tor.y * tor.y;

    double m = glm::dot(ro, ro);
    double n = glm::dot(ro, rd);

    // bounding sphere
    {
    	double h = n * n - m + (tor.x + tor.y) * (tor.x + tor.y);
    	if (h < 0.0) {
            return -1.0;
        }
    	//float t = -n-sqrt(h); // could use this to compute intersections from ro+t*rd
    }

	// find quartic equation
    double k  = (m - ra2 - Ra2) / 2.0;
    double k3 = n;
    double k2 = n * n + Ra2 * rd.z * rd.z + k;
    double k1 = k * n + Ra2 * ro.z * rd.z;
    double k0 = k * k + Ra2 * ro.z * ro.z - Ra2 * ra2;

    #if 1
    // prevent |c1| from being too close to zero
    if (std::abs(k3 * (k3 * k3 - k2) + k1) < 0.001) {
        po = -1.0;
        double tmp = k1; k1 = k3; k3 = tmp;
        k0 = 1.0 / k0;
        k1 = k1 * k0;
        k2 = k2 * k0;
        k3 = k3 * k0;
    }
	#endif

    double c2 = 2.0 * k2 - 3.0 * k3 * k3;
    double c1 = k3 * (k3 * k3 - k2) + k1;
    double c0 = k3 * (k3 * (-3.0 * k3 * k3 + 4.0 * k2) - 8.0 * k1) + 4.0 * k0;

    c2 /= 3.0;
    c1 *= 2.0;
    c0 /= 3.0;

    double Q = c2 * c2 + c0;
    double R = 3.0 * c0 * c2 - c2 * c2 * c2 - c1 * c1;

    double h = R * R - Q * Q * Q;
    double z = 0.0;
    if (h < 0.0) {
    	// 4 intersections
        double sQ = std::sqrt(Q);
        z = 2.0 * sQ * std::cos(
            std::acos(R / (sQ * Q)) / 3.0
        );
    } else {
        // 2 intersections
        double sQ = std::pow(
            std::sqrt(h) + std::abs(R),
            1.0 / 3.0
        );
        z = sign(R) * std::abs(sQ + Q / sQ);
    }
    z = c2 - z;

    double d1 = z     - 3.0 * c2;
    double d2 = z * z - 3.0 * c0;
    if (std::abs(d1) < 1.0e-6) {
        if (d2 < 0.0) {
            return -1.0;
        }
        d2 = std::sqrt(d2);
    } else {
        if (d1 < 0.0) {
            return -1.0;
        }
        d1 = std::sqrt(d1 / 2.0);
        d2 = c1 / d1;
    }

    //----------------------------------

    double result = 1e20;

    h = d1 * d1 - z + d2;
    if (h > 0.0) {
        h = std::sqrt(h);
        double t1 = -d1 - h - k3; t1 = (po < 0.0) ? 2.0 / t1 : t1;
        double t2 = -d1 + h - k3; t2 = (po < 0.0) ? 2.0 / t2 : t2;
        if (t1 > 0.0) result = t1;
        if (t2 > 0.0) result = std::min(result, t2);
    }

    h = d1 * d1 - z - d2;
    if (h > 0.0) {
        h = std::sqrt(h);
        double t1 = d1 - h - k3; t1 = (po < 0.0) ? 2.0 / t1 : t1;
        double t2 = d1 + h - k3; t2 = (po < 0.0) ? 2.0 / t2 : t2;
        if (t1 > 0.0) result = std::min(result, t1);
        if (t2 > 0.0) result = std::min(result, t2);
    }

    return result;
}

} // anonymous namespace

void Line_renderer::add_torus(
    const erhe::scene::Transform& world_from_node,
    const vec4&                   major_color,
    const vec4&                   minor_color,
    const float                   major_thickness,
    const float                   major_radius,
    const float                   minor_radius,
    const glm::vec3&              camera_position_in_world,
    const int                     major_step_count,
    const int                     minor_step_count,
    const float                   epsilon,
    const int                     debug_major,
    const int                     debug_minor
)
{
    static_cast<void>(major_color);
    static_cast<void>(minor_color);
    static_cast<void>(debug_major);
    static_cast<void>(debug_minor);
    constexpr vec3 axis_x{1.0f, 0.0f, 0.0f};
    constexpr vec3 axis_y{0.0f, 1.0f, 0.0f};
    constexpr vec3 axis_z{0.0f, 0.0f, 1.0f};
    const     mat4 m                       = world_from_node.matrix();
    const     mat4 node_from_world         = world_from_node.inverse_matrix();
    const     vec3 camera_position_in_node = vec4{node_from_world * vec4{camera_position_in_world, 1.0f}};
    const     vec2 tor                     = vec2{major_radius, minor_radius};
    constexpr int  k = 8;
    set_thickness(major_thickness);
    for (int i = 0; i < major_step_count; ++i) {
        const float rel_major = static_cast<float>(i) / static_cast<float>(major_step_count);
        for (int j = 0; j < minor_step_count * k; ++j) {
            const float       rel_minor      = static_cast<float>(j    ) / static_cast<float>(minor_step_count * k);
            const float       rel_minor_next = static_cast<float>(j + 1) / static_cast<float>(minor_step_count * k);
            const Torus_point a       = torus_point(major_radius, minor_radius, rel_major, rel_minor);
            const Torus_point b       = torus_point(major_radius, minor_radius, rel_major, rel_minor_next);
            const Torus_point c       = torus_point(major_radius, minor_radius, rel_major, 0.5f * (rel_minor + rel_minor_next));
            const glm::dvec3  ray_dir = glm::normalize(glm::dvec3{camera_position_in_node} - glm::dvec3{c.p});
            const glm::dvec3  ray_org = glm::dvec3{c.p} + 1.5 * epsilon * ray_dir;
            const double      t       = ray_torus_intersection(glm::dvec3{ray_org}, glm::dvec3{ray_dir}, glm::dvec2{tor});
            const glm::dvec3  P0      = ray_org + t * ray_dir;
            const float       d       = static_cast<float>(glm::distance(P0, glm::dvec3{c.p}));
            const bool        visible = (t == -1.0f) || (t > 1e10) || (d < epsilon) || (d > glm::distance(c.p, camera_position_in_node));

            add_lines(
                m,
                visible ? major_color : minor_color,
                { { a.p, b.p } }
            );
#if 0
            if ((i == debug_major) && (j == debug_minor))
            {
                add_lines( // blue: normal
                    m,
                    vec4{0.0f, 0.0f, 1.0f, 0.5f},
                    { { c.p, c.p + 0.1f * c.n } }
                );
                add_lines( // cyan: point to camera
                    m,
                    vec4{0.0f, 1.0f, 1.0f, 0.5f},
                    { { camera_position_in_node, c.p } }
                );
                add_lines( // green: center to point
                    m,
                    vec4{0.0f, 1.0f, 0.0f, 1.0f},
                    { { vec3{0.0f, 0.0f, 0.0f}, c.p } }
                );
                if ((t > 0.0f) && (t < 1e10)) // && (d > epsilon))
                {
                    add_lines( // red: point to intersection
                        m,
                        vec4{1.0f, 0.0f, 0.0f, 1.0f},
                        { { ray_org, P0 } }
                    );
                }
            }
#endif
        }
    }

    for (int j = 0; j < minor_step_count; ++j) {
        const float rel_minor = static_cast<float>(j) / static_cast<float>(minor_step_count);
        for (int i = 0; i < major_step_count * k; ++i) {
            const float       rel_major      = static_cast<float>(i    ) / static_cast<float>(major_step_count * k);
            const float       rel_major_next = static_cast<float>(i + 1) / static_cast<float>(major_step_count * k);
            const Torus_point a = torus_point(major_radius, minor_radius, rel_major,      rel_minor);
            const Torus_point b = torus_point(major_radius, minor_radius, rel_major_next, rel_minor);
            const Torus_point c = torus_point(major_radius, minor_radius, 0.5f * (rel_major + rel_major_next), rel_minor);
            const glm::dvec3  ray_dir = glm::normalize(glm::dvec3{camera_position_in_node} - glm::dvec3{c.p});
            const glm::dvec3  ray_org = glm::dvec3{c.p} + 1.5 * epsilon * ray_dir;
            const double      t       = ray_torus_intersection(glm::dvec3{ray_org}, glm::dvec3{ray_dir}, glm::dvec2{tor});
            const glm::dvec3  P0      = ray_org + t * ray_dir;
            const float       d       = static_cast<float>(glm::distance(P0, glm::dvec3{c.p}));
            const bool        visible = (t == -1.0f) || (t > 1e10) || (d < epsilon) || (d > glm::distance(c.p, camera_position_in_node));

            add_lines(
                m,
                visible ? major_color : minor_color,
                { { a.p, b.p } }
            );
        }
    }
}

static constexpr std::string_view c_line_renderer_render{"Line_renderer::render()"};

void Line_renderer::render(
    const erhe::scene::Viewport viewport,
    const erhe::scene::Camera&  camera,
    const bool                  show_visible_lines,
    const bool                  show_hidden_lines
)
{
    if (m_line_count == 0) {
        return;
    }

    const auto* camera_node = camera.get_node();
    if (camera_node == nullptr) {
        return;
    }

    ERHE_PROFILE_FUNCTION
    ERHE_PROFILE_GPU_SCOPE(c_line_renderer_render)

    erhe::graphics::Scoped_debug_group line_renderer_initialization{c_line_renderer_render};

    m_view_writer.begin(&current_frame_resources().view_buffer);

    const auto  projection_transforms  = camera.projection_transforms(viewport);
    const mat4  clip_from_world        = projection_transforms.clip_from_world.matrix();
    const vec4  view_position_in_world = camera_node->position_in_world();
    const auto  fov_sides              = camera.projection()->get_fov_sides(viewport);
    auto* const view_buffer            = &current_frame_resources().view_buffer;
    const auto  view_gpu_data          = view_buffer->map();
    const float viewport_floats[4] {
        static_cast<float>(viewport.x),
        static_cast<float>(viewport.y),
        static_cast<float>(viewport.width),
        static_cast<float>(viewport.height)
    };
    const float fov_floats[4] {
        fov_sides.left,
        fov_sides.right,
        fov_sides.up,
        fov_sides.down
    };

    using erhe::graphics::write;
    using erhe::graphics::as_span;
    write(view_gpu_data, m_view_writer.write_offset + m_pipeline->clip_from_world_offset,        as_span(clip_from_world       ));
    write(view_gpu_data, m_view_writer.write_offset + m_pipeline->view_position_in_world_offset, as_span(view_position_in_world));
    write(view_gpu_data, m_view_writer.write_offset + m_pipeline->viewport_offset,               as_span(viewport_floats       ));
    write(view_gpu_data, m_view_writer.write_offset + m_pipeline->fov_offset,                    as_span(fov_floats            ));

    m_view_writer.write_offset += m_pipeline->view_block->size_bytes();
    m_view_writer.end();

    gl::disable          (gl::Enable_cap::primitive_restart_fixed_index);
    gl::enable           (gl::Enable_cap::sample_alpha_to_coverage);
    gl::enable           (gl::Enable_cap::sample_alpha_to_one);
    gl::viewport         (viewport.x, viewport.y, viewport.width, viewport.height);
    gl::bind_buffer_range(
        view_buffer->target(),
        static_cast<GLuint>    (m_pipeline->view_block->binding_point()),
        static_cast<GLuint>    (view_buffer->gl_name()),
        static_cast<GLintptr>  (m_view_writer.range.first_byte_offset),
        static_cast<GLsizeiptr>(m_view_writer.range.byte_count)
    );

    const auto first = static_cast<GLint>(
        m_vertex_writer.range.first_byte_offset / m_pipeline->vertex_format.stride()
    );
    const auto count = static_cast<GLsizei>(m_line_count * 2);

    if (show_hidden_lines) {
        const auto& pipeline = current_frame_resources().pipeline_hidden;
        erhe::graphics::g_opengl_state_tracker->execute(pipeline);

        gl::draw_arrays(
            pipeline.data.input_assembly.primitive_topology,
            first,
            count
        );
    }

    if (show_visible_lines) {
        const auto& pipeline = current_frame_resources().pipeline_visible;
        erhe::graphics::g_opengl_state_tracker->execute(pipeline);

        gl::draw_arrays(
            pipeline.data.input_assembly.primitive_topology,
            first,
            count
        );
    }

    gl::disable(gl::Enable_cap::sample_alpha_to_coverage);
    gl::disable(gl::Enable_cap::sample_alpha_to_one);
}

} // namespace erhe::application
