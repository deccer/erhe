#pragma once

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4702) // unreachable code
#   pragma warning(disable : 4714) // marked as __forceinline not inlined
#endif

#include "erhe/raytrace/iscene.hpp"

#include <bvh/v2/bvh.h>
#include <bvh/v2/sphere.h>

#include <string>
#include <vector>

namespace erhe::raytrace
{

class Bvh_geometry;
class Bvh_instance;
class IGeometry;

class Bvh_scene
    : public IScene
{
public:
    explicit Bvh_scene(const std::string_view debug_label);
    ~Bvh_scene() noexcept override;

    // Implements IScene
    void attach   (IGeometry* geometry) override;
    void attach   (IInstance* instance) override;
    void detach   (IGeometry* geometry) override;
    void detach   (IInstance* geometry) override;
    void commit   () override;
    void intersect(Ray& ray, Hit& hit) override;
    [[nodiscard]] auto debug_label() const -> std::string_view override;

    // Bvh_scene public API
    void intersect_instance(Ray& ray, Hit& hit, Bvh_instance* instance);
    //// void collect_spheres   (
    ////     std::vector<bvh::Sphere<float>>& spheres,
    ////     std::vector<Bvh_instance*>&      instances,
    ////     Bvh_instance*                    instance
    //// );

private:
    std::vector<Bvh_geometry*> m_geometries;
    std::vector<Bvh_instance*> m_instances;
    std::string                m_debug_label;

    //// std::vector<bvh::Sphere<float>> m_collected_spheres;   // flat
    std::vector<Bvh_instance*>   m_collected_instances; // flat
    bvh::v2::BBox<float, 3>      m_global_bbox;
    bvh::v2::Bvh<
         bvh::v2::Node<float, 3>
    >                            m_bvh;
};

}

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif
