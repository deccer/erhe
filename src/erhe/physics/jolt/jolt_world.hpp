#pragma once

#include "erhe/physics/iworld.hpp"
#include "erhe/physics/imotion_state.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <memory>
#include <vector>

namespace erhe::physics
{

class ICollision_shape;
class Jolt_rigid_body;
class Jolt_constraint;
//class Jolt_debug_renderer;

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{

static constexpr uint8_t NON_MOVING    = 0u;
static constexpr uint8_t MOVING        = 1u;
static constexpr uint8_t NON_COLLIDING = 2u;
static constexpr uint8_t NUM_LAYERS    = 3u;

[[nodiscard]] auto get_layer(const Motion_mode motion_mode) -> uint8_t;

};

class Jolt_collision_filter
    : public JPH::ObjectVsBroadPhaseLayerFilter
    , public JPH::ObjectLayerPairFilter
{
    // Implements JPH::ObjectVsBroadPhaseLayerFilter
	auto ShouldCollide(
        JPH::ObjectLayer     inLayer1,
        JPH::BroadPhaseLayer inLayer2
    ) const -> bool override;

    // Implements JPH::ObjectLayerPairFilter
    auto ShouldCollide(
        JPH::ObjectLayer inLayer1,
        JPH::ObjectLayer inLayer2
    ) const -> bool override;
};

class Jolt_world
    : public IWorld
    , public JPH::BodyActivationListener
    , public JPH::ContactListener
{
public:
    Jolt_world();
    virtual ~Jolt_world() noexcept override;

    // Implements IWorld
    [[nodiscard]] auto is_physics_updates_enabled() const -> bool      override;
    [[nodiscard]] auto get_gravity               () const -> glm::vec3 override;
    void enable_physics_updates ()                         override;
    void disable_physics_updates()                         override;
    void update_fixed_step      (double dt)                override;
    void set_gravity            (const glm::vec3& gravity) override;
    void add_rigid_body         (IRigid_body* rigid_body)  override;
    void remove_rigid_body      (IRigid_body* rigid_body)  override;
    void add_constraint         (IConstraint* constraint)  override;
    void remove_constraint      (IConstraint* constraint)  override;
    void set_debug_drawer       (IDebug_draw* debug_draw)  override;
    void debug_draw             ()                         override;
    void sanity_check           ()                         override;

    // Implements BodyActivationListener
    void OnBodyActivated  (const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
    void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;

    // Implements ContactListener
    //auto OnContactValidate (const JPH::Body& inBody1, const JPH::Body& inBody2, const CollideShapeResult &inCollisionResult) -> JPH::ValidateResult override
    //{
    //    return ValidateResult::AcceptAllContactsForThisBodyPair;
    //}

    void OnContactAdded    (const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
    void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
    void OnContactRemoved  (const JPH::SubShapeIDPair& inSubShapePair) override;

    // Public API
    [[nodiscard]] auto get_physics_system() -> JPH::PhysicsSystem&;

private:
    class Initialize_first
    {
    public:
        Initialize_first();
    };
    Initialize_first m_initialize_first;

    bool                                           m_physics_enabled{false};
    glm::vec3                                      m_gravity        {0.0f};

    static constexpr unsigned int cMaxBodies             = 1024 * 32;
    static constexpr unsigned int cNumBodyMutexes        = 0;
    static constexpr unsigned int cMaxBodyPairs          = 1024 * 8;
    static constexpr unsigned int cMaxContactConstraints = 1024;

    const Jolt_collision_filter                    m_collision_filter;

    JPH::TempAllocatorImpl                         m_temp_allocator;
    JPH::JobSystemThreadPool                       m_job_system;
    std::unique_ptr<JPH::BroadPhaseLayerInterface> m_broad_phase_layer_interface;
    JPH::PhysicsSystem                             m_physics_system;
    //std::unique_ptr<Jolt_debug_renderer>           m_debug_renderer;

    std::vector<Jolt_rigid_body*>                  m_rigid_bodies;
    std::vector<Jolt_constraint*>                  m_constraints;

    std::vector<std::shared_ptr<ICollision_shape>> m_collision_shapes;

};

} // namespace erhe::physics
