#pragma once

#include "erhe/graphics/pipeline.hpp"
#include "erhe/primitive/enums.hpp"

#include <functional>

namespace erhe::renderer
{

class Pipeline_renderpass
{
public:
    void reset();

    erhe::graphics::Pipeline pipeline;
    std::function<void()>    begin;
    std::function<void()>    end;
};

} // namespace erhe::renderer