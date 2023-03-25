// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "renderers/material_buffer.hpp"
#include "renderers/programs.hpp"
#include "renderers/program_interface.hpp"
#include "editor_log.hpp"

#include "erhe/graphics/texture.hpp"
#include "erhe/primitive/material.hpp"
#include "erhe/toolkit/profile.hpp"

#include <imgui.h>

namespace editor
{

Material_interface::Material_interface(std::size_t max_material_count)
    : material_block {"material", 0, erhe::graphics::Shader_resource::Type::shader_storage_block}
    , material_struct{"Material"}
    , offsets        {
        .roughness    = material_struct.add_vec2 ("roughness"   )->offset_in_parent(),
        .metallic     = material_struct.add_float("metallic"    )->offset_in_parent(),
        .reflectance  = material_struct.add_float("reflectance" )->offset_in_parent(),
        .base_color   = material_struct.add_vec4 ("base_color"  )->offset_in_parent(),
        .emissive     = material_struct.add_vec4 ("emissive"    )->offset_in_parent(),
        .base_texture = material_struct.add_uvec2("base_texture")->offset_in_parent(),
        .opacity      = material_struct.add_float("opacity"     )->offset_in_parent(),
        .reserved     = material_struct.add_float("reserved"    )->offset_in_parent()
    },
    max_material_count{max_material_count}
{
    material_block.add_struct(
        "materials",
        &material_struct,
        erhe::graphics::Shader_resource::unsized_array
    );
}

Material_buffer::Material_buffer(Material_interface* material_interface)
    : Multi_buffer        {"material"}
    , m_material_interface{material_interface}
{
    Multi_buffer::allocate(
        gl::Buffer_target::shader_storage_buffer,
        m_material_interface->material_block.binding_point(),
        m_material_interface->material_struct.size_bytes() * m_material_interface->max_material_count
    );
}

auto Material_buffer::update(
    const gsl::span<const std::shared_ptr<erhe::primitive::Material>>& materials
) -> erhe::application::Buffer_range
{
    ERHE_PROFILE_FUNCTION

    SPDLOG_LOGGER_TRACE(
        log_render,
        "materials.size() = {}, m_material_writer.write_offset = {}",
        materials.size(),
        m_writer.write_offset
    );

    auto&             buffer         = current_buffer();
    const auto        entry_size     = m_material_interface->material_struct.size_bytes();
    const auto&       offsets        = m_material_interface->offsets;
    const std::size_t max_byte_count = materials.size() * entry_size;
    const auto        gpu_data       = m_writer.begin(&buffer, max_byte_count);
    
    m_used_handles.clear();
    uint32_t material_index = 0;
    for (const auto& material : materials) {
        if ((m_writer.write_offset + entry_size) > m_writer.write_end) {
            log_render->critical("material buffer capacity {} exceeded", buffer.capacity_byte_count());
            ERHE_FATAL("material buffer capacity exceeded");
            break;
        }
        memset(reinterpret_cast<uint8_t*>(gpu_data.data()) + m_writer.write_offset, 0, entry_size);
        using erhe::graphics::as_span;
        using erhe::graphics::write;

        const uint64_t handle = material->texture
            ?
                erhe::graphics::get_handle(
                    *material->texture.get(),
                    material->sampler
                        ? *material->sampler.get()
                        : *g_programs->linear_sampler.get()
                )
            : 0;

        if (handle != 0) {
            m_used_handles.insert(handle);
        }

        material->material_buffer_index = material_index;

        write(gpu_data, m_writer.write_offset + offsets.metallic   , as_span(material->metallic   ));
        write(gpu_data, m_writer.write_offset + offsets.roughness  , as_span(material->roughness  ));
        write(gpu_data, m_writer.write_offset + offsets.reflectance, as_span(material->reflectance));
        write(gpu_data, m_writer.write_offset + offsets.base_color , as_span(material->base_color ));
        write(gpu_data, m_writer.write_offset + offsets.emissive   , as_span(material->emissive   ));
        write(gpu_data, m_writer.write_offset + offsets.opacity    , as_span(material->opacity    ));

        if (erhe::graphics::Instance::info.use_bindless_texture) {
            write(gpu_data, m_writer.write_offset + offsets.base_texture, as_span(handle));
        } else {
            std::optional<std::size_t> opt_texture_unit = erhe::graphics::s_texture_unit_cache.allocate_texture_unit(handle);
            const uint64_t texture_unit = static_cast<uint64_t>(opt_texture_unit.has_value() ? opt_texture_unit.value() : 0);
            const uint64_t magic        = static_cast<uint64_t>(0x7fff'ffff);
            const uint64_t value        = texture_unit | (magic << 32);
            write(gpu_data, m_writer.write_offset + offsets.base_texture, as_span(value));
        }

        m_writer.write_offset += entry_size;
        ERHE_VERIFY(m_writer.write_offset <= m_writer.write_end);
        ++material_index;
    }
    m_writer.end();

    SPDLOG_LOGGER_TRACE(log_draw, "wrote {} entries to material buffer", material_index);

    return m_writer.range;
}

[[nodiscard]] auto Material_buffer::used_handles() const -> const std::set<uint64_t>&
{
    return m_used_handles;
}

} // namespace editor
