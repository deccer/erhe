#include "renderers/forward_renderer.hpp"

#include "editor_log.hpp"
#include "renderers/mesh_memory.hpp"
#include "renderers/program_interface.hpp"
#include "renderers/programs.hpp"
#include "renderers/shadow_renderer.hpp"

#include "erhe/application/configuration.hpp"
#include "erhe/application/graphics/gl_context_provider.hpp"
#include "erhe/gl/draw_indirect.hpp"
#include "erhe/gl/wrapper_functions.hpp"
#include "erhe/graphics/buffer.hpp"
#include "erhe/graphics/debug.hpp"
#include "erhe/graphics/instance.hpp"
#include "erhe/graphics/opengl_state_tracker.hpp"
#include "erhe/graphics/shader_resource.hpp"
#include "erhe/graphics/shader_stages.hpp"
#include "erhe/graphics/state/vertex_input_state.hpp"
#include "erhe/graphics/vertex_format.hpp"
#include "erhe/primitive/primitive.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/light.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/toolkit/math_util.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <functional>

namespace editor
{

using erhe::graphics::Vertex_input_state;
using erhe::graphics::Input_assembly_state;
using erhe::graphics::Rasterization_state;
using erhe::graphics::Depth_stencil_state;
using erhe::graphics::Color_blend_state;

Forward_renderer* g_forward_renderer{nullptr};

Forward_renderer::Forward_renderer()
    : Component{c_type_name}
{
}

Forward_renderer::~Forward_renderer() noexcept
{
}

void Forward_renderer::deinitialize_component()
{
    ERHE_VERIFY(g_forward_renderer == this);
    m_material_buffers     .reset();
    m_light_buffers        .reset();
    m_camera_buffers       .reset();
    m_draw_indirect_buffers.reset();
    m_primitive_buffers    .reset();
    m_dummy_texture        .reset();
    g_forward_renderer = nullptr;
}

void Forward_renderer::declare_required_components()
{
    require<erhe::application::Configuration      >();
    require<erhe::application::Gl_context_provider>();
    require<Mesh_memory      >();
    require<Program_interface>();
    require<Programs         >();
}

static constexpr std::string_view c_forward_renderer_initialize_component{"Forward_renderer::initialize_component()"};
void Forward_renderer::initialize_component()
{
    ERHE_PROFILE_FUNCTION
    ERHE_VERIFY(g_forward_renderer == nullptr);

    const erhe::application::Scoped_gl_context gl_context;

    erhe::graphics::Scoped_debug_group forward_renderer_initialization{c_forward_renderer_initialize_component};

    auto& shader_resources  = *g_program_interface->shader_resources.get();
    m_material_buffers      = Material_buffer     {&shader_resources.material_interface};
    m_light_buffers         = Light_buffer        {&shader_resources.light_interface};
    m_camera_buffers        = Camera_buffer       {&shader_resources.camera_interface};
    m_draw_indirect_buffers = Draw_indirect_buffer{
        static_cast<size_t>(g_program_interface->config.max_draw_count)
    };
    m_primitive_buffers     = Primitive_buffer    {&shader_resources.primitive_interface};

    m_dummy_texture = erhe::graphics::create_dummy_texture();

    g_forward_renderer = this;
}

static constexpr std::string_view c_forward_renderer_render{"Forward_renderer::render()"};

void Forward_renderer::next_frame()
{
    m_material_buffers     ->next_frame();
    m_light_buffers        ->next_frame();
    m_camera_buffers       ->next_frame();
    m_draw_indirect_buffers->next_frame();
    m_primitive_buffers    ->next_frame();
}

auto Forward_renderer::primitive_settings() -> Primitive_interface_settings&
{
    return m_primitive_buffers->settings;
}

auto Forward_renderer::primitive_settings() const -> const Primitive_interface_settings&
{
    return m_primitive_buffers->settings;
}

void Forward_renderer::render(const Render_parameters& parameters)
{
    ERHE_PROFILE_FUNCTION

    const auto& viewport       = parameters.viewport;
    const auto* camera         = parameters.camera;
    const auto& mesh_spans     = parameters.mesh_spans;
    const auto& lights         = parameters.lights;
    const auto& materials      = parameters.materials;
    const auto& passes         = parameters.passes;
    const auto& filter         = parameters.filter;
    const bool  enable_shadows =
        (g_shadow_renderer != nullptr) &&
        (!lights.empty()) &&
        (parameters.shadow_texture != nullptr);

    const uint64_t shadow_texture_handle = enable_shadows
        ?
            erhe::graphics::get_handle(
                *parameters.shadow_texture,
                *g_programs->nearest_sampler.get()
            )
        : 0;
    const uint64_t fallback_texture_handle = erhe::graphics::get_handle(
        *m_dummy_texture.get(),
        *g_programs->nearest_sampler.get()
    );

    gl::viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    if (camera != nullptr) {
        const auto range = m_camera_buffers->update(
            *camera->projection(),
            *camera->get_node(),
            viewport,
            camera->get_exposure()
        );
        m_camera_buffers->bind(range);
    }

    if (!erhe::graphics::Instance::info.use_bindless_texture) {
        erhe::graphics::s_texture_unit_cache.reset(g_programs->base_texture_unit);
    }

    const auto naterial_range = m_material_buffers->update(materials);
    m_material_buffers->bind(naterial_range);

    // This must be done even if lights is empty.
    // For example, the number of lights is read from the light buffer.
    const auto light_range = m_light_buffers->update(
        lights,
        parameters.light_projections,
        parameters.ambient_light
    );
    m_light_buffers->bind_light_buffer(light_range);

    if (erhe::graphics::Instance::info.use_bindless_texture) {
        ERHE_PROFILE_SCOPE("make textures resident");

        if (enable_shadows) {
            gl::make_texture_handle_resident_arb(shadow_texture_handle);
        }
        for (const uint64_t handle : m_material_buffers->used_handles()) {
            gl::make_texture_handle_resident_arb(handle);
        }
    } else {
        ERHE_PROFILE_SCOPE("bind texture units");

        if (enable_shadows) {
            gl::bind_texture_unit(g_programs->shadow_texture_unit, parameters.shadow_texture->gl_name());
            gl::bind_sampler     (g_programs->shadow_texture_unit, g_programs->nearest_sampler->gl_name());
        }

        erhe::graphics::s_texture_unit_cache.bind(fallback_texture_handle);
    }

    for (auto& pass : passes) {
        const auto& pipeline = pass->pipeline;
        if (!pipeline.data.shader_stages) {
            continue;
        }

        const auto primitive_mode = pass->primitive_mode; //select_primitive_mode(pass);

        if (pass->begin) {
            ERHE_PROFILE_SCOPE("pass begin");
            pass->begin();
        }

        erhe::graphics::Scoped_debug_group pass_scope{pass->pipeline.data.name};

        erhe::graphics::g_opengl_state_tracker->execute(pipeline);

        for (const auto& meshes : mesh_spans) {
            ERHE_PROFILE_SCOPE("mesh span");
            ERHE_PROFILE_GPU_SCOPE(c_forward_renderer_render);
            if (meshes.empty()) {
                continue;
            }

            const auto primitive_range            = m_primitive_buffers->update(meshes, filter);
            const auto draw_indirect_buffer_range = m_draw_indirect_buffers->update(meshes, primitive_mode, filter);
            if (draw_indirect_buffer_range.draw_indirect_count == 0) {
                continue;
            }
            m_primitive_buffers->bind(primitive_range);
            m_draw_indirect_buffers->bind(draw_indirect_buffer_range.range);

            {
                ERHE_PROFILE_SCOPE("mdi");
                gl::multi_draw_elements_indirect(
                    pipeline.data.input_assembly.primitive_topology,
                    g_mesh_memory->gl_index_type(),
                    reinterpret_cast<const void *>(draw_indirect_buffer_range.range.first_byte_offset),
                    static_cast<GLsizei>(draw_indirect_buffer_range.draw_indirect_count),
                    static_cast<GLsizei>(sizeof(gl::Draw_elements_indirect_command))
                );
            }
        }

        if (pass->end) {
            ERHE_PROFILE_SCOPE("pass end");
            pass->end();
        }
    }

    if (erhe::graphics::Instance::info.use_bindless_texture) {
        ERHE_PROFILE_SCOPE("make textures non resident");

        if (enable_shadows) {
            ERHE_PROFILE_SCOPE("shadow texture non resident");
            gl::make_texture_handle_non_resident_arb(shadow_texture_handle);
        }
        for (const uint64_t handle : m_material_buffers->used_handles()) {
            gl::make_texture_handle_non_resident_arb(handle);
        }
    }
}

void Forward_renderer::render_fullscreen(
    const Render_parameters&  parameters,
    const erhe::scene::Light* light
)
{
    ERHE_PROFILE_FUNCTION

    const auto& viewport       = parameters.viewport;
    const auto* camera         = parameters.camera;
    const auto& lights         = parameters.lights;
    const auto& passes         = parameters.passes;
    const bool  enable_shadows =
        (g_shadow_renderer != nullptr) &&
        (!lights.empty()) &&
        (parameters.shadow_texture != nullptr);

    const uint64_t shadow_texture_handle = enable_shadows
        ?
            erhe::graphics::get_handle(
                *parameters.shadow_texture,
                *g_programs->nearest_sampler.get()
            )
        : 0;

    erhe::graphics::Scoped_debug_group forward_renderer_render{c_forward_renderer_render};

    gl::viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    const auto material_range = m_material_buffers->update(parameters.materials);
    m_material_buffers->bind(material_range);

    if (camera != nullptr) {
        const auto camera_range = m_camera_buffers->update(
            *camera->projection(),
            *camera->get_node(),
            viewport,
            camera->get_exposure()
        );
        m_camera_buffers->bind(camera_range);
    }

    if (light != nullptr) {
        const auto* light_projection_transforms = parameters.light_projections->get_light_projection_transforms_for_light(light);
        if (light_projection_transforms != nullptr) {
            const auto control_range = m_light_buffers->update_control(light_projection_transforms->index);
            m_light_buffers->bind_control_buffer(control_range);
        } else {
            //// log_render->warn("Light {} has no light projection transforms", light->name());
        }
    }

    {
        const auto light_range = m_light_buffers->update(lights, parameters.light_projections, parameters.ambient_light);
        m_light_buffers->bind_light_buffer(light_range);
    }

    if (enable_shadows) {
        if (erhe::graphics::Instance::info.use_bindless_texture) {
            gl::make_texture_handle_resident_arb(shadow_texture_handle);
        } else {
            gl::bind_texture_unit(g_programs->shadow_texture_unit, parameters.shadow_texture->gl_name());
            gl::bind_sampler     (g_programs->shadow_texture_unit, g_programs->nearest_sampler->gl_name());
        }
    }

    for (auto& pass : passes) {
        const auto& pipeline = pass->pipeline;
        if (!pipeline.data.shader_stages) {
            continue;
        }

        if (pass->begin) {
            pass->begin();
        }

        erhe::graphics::Scoped_debug_group pass_scope{pass->pipeline.data.name};

        erhe::graphics::g_opengl_state_tracker->execute(pipeline);
        gl::draw_arrays(pipeline.data.input_assembly.primitive_topology, 0, 3);

        if (pass->end) {
            pass->end();
        }
    }

    if (enable_shadows) {
        if (erhe::graphics::Instance::info.use_bindless_texture) {
            gl::make_texture_handle_non_resident_arb(shadow_texture_handle);
        }
    }
}

} // namespace editor
