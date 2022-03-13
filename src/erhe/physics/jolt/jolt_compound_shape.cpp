#include "erhe/physics/jolt/jolt_compound_shape.hpp"
#include "erhe/toolkit/verify.hpp"

#include <Jolt.h>
#include <Physics/Collision/Shape/StaticCompoundShape.h>

namespace erhe::physics
{

auto ICollision_shape::create_compound_shape(
    const Compound_shape_create_info& create_info
) -> ICollision_shape*
{
    return new Jolt_compound_shape(create_info);
}

auto ICollision_shape::create_compound_shape_shared(
    const Compound_shape_create_info& create_info
) -> std::shared_ptr<ICollision_shape>
{
    return std::make_shared<Jolt_compound_shape>(create_info);
}

Jolt_compound_shape::Jolt_compound_shape(
    const Compound_shape_create_info& create_info
)
{
	JPH::StaticCompoundShapeSettings shape_settings;
    for (const auto& entry : create_info.children)
    {
        auto collision_shape = dynamic_pointer_cast<Jolt_collision_shape>(entry.shape);
        const auto basis = glm::quat{entry.transform.basis};
        shape_settings.AddShape(
            to_jolt(entry.transform.origin),
            to_jolt(basis),
            collision_shape->get_jolt_shape()
        );
    }

	auto result = shape_settings.Create();
    ERHE_VERIFY(result.IsValid());
    m_jolt_shape = result.Get();
}


} // namespace erhe::physics