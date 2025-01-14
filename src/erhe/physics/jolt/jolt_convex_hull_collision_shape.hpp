#pragma once

#include "erhe/physics/jolt/jolt_collision_shape.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include <glm/glm.hpp>

#include <memory>

namespace erhe::physics
{

class Jolt_convex_hull_collision_shape
    : public Jolt_collision_shape
{
public:
    Jolt_convex_hull_collision_shape(
        const float* points,
        int          point_count,
        int          stride
    );

    auto get_shape_settings() -> JPH::ShapeSettings& override
    {
        return *m_shape_settings.GetPtr();
    }

private:
    JPH::Ref<JPH::ConvexHullShapeSettings> m_shape_settings;
};

} // namespace erhe::physics
