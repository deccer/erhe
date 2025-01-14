#pragma once

#include "erhe/gl/wrapper_enums.hpp"

#include <string>

namespace erhe::graphics
{

class Fragment_output
{
public:
    std::string                     name;
    gl::Fragment_shader_output_type type{gl::Fragment_shader_output_type::float_vec4};
    unsigned int                    location;
};

} // namespace erhe::graphics
