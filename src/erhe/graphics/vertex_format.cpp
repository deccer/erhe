#include "erhe/graphics/vertex_format.hpp"
#include "erhe/gl/gl_helpers.hpp"
#include "erhe/toolkit/verify.hpp"

#include <gsl/assert>

#include <algorithm>
#include <cassert>

namespace erhe::graphics
{

Vertex_format::Vertex_format()
{
}

Vertex_format::Vertex_format(std::initializer_list<Vertex_attribute> attributes)
{
    for (auto& attribute : attributes) {
        add_attribute(attribute);
    }
}

void Vertex_format::align_to(const std::size_t alignment)
{
    ERHE_VERIFY(alignment > 0);

    while ((m_stride % alignment) != 0) {
        ++m_stride;
    }
}

void Vertex_format::add_attribute(
    const Vertex_attribute& attribute
)
{
    ERHE_VERIFY(
        (attribute.data_type.dimension >= 1) &&
        (attribute.data_type.dimension <= 4)
    );

    const std::size_t attribute_stride = attribute.data_type.dimension * gl_helpers::size_of_type(attribute.data_type.type);
    // Note: Vertex attributes have no alignment requirements - do *not* align attribute offsets
    auto& new_attribute = m_attributes.emplace_back(attribute);
    new_attribute.offset = m_stride;
    m_stride += attribute_stride;
}

auto Vertex_format::match(const Vertex_format& other) const -> bool
{
    if (m_attributes.size() != other.m_attributes.size()) {
        return false;
    }

    for (size_t i = 0; i < m_attributes.size(); ++i) {
        if (m_attributes[i] != other.m_attributes[i]) {
            return false;
        }
    }

    return true;
}

auto Vertex_format::has_attribute(
    const Vertex_attribute::Usage_type usage_type,
    const unsigned int                 index
) const -> bool
{
    for (const auto& i : m_attributes) {
        if ((i.usage.type == usage_type) && (i.usage.index == index)) {
            return true;
        }
    }

    return false;
}

auto Vertex_format::find_attribute_maybe(
    const Vertex_attribute::Usage_type usage_type,
    const unsigned int                 index
) const -> const Vertex_attribute*
{
    for (const auto& i : m_attributes) {
        if ((i.usage.type == usage_type) && (i.usage.index== index)) {
            return &(i);
        }
    }

    return nullptr;
}

auto Vertex_format::find_attribute(
    const Vertex_attribute::Usage_type usage_type,
    const unsigned int                 index
) const -> gsl::not_null<const Vertex_attribute*>
{
    for (const auto& i : m_attributes) {
        if ((i.usage.type == usage_type) && (i.usage.index == index)) {
            return &(i);
        }
    }

    ERHE_FATAL("vertex_attribute not found");
}

} // namespace erhe::graphics
