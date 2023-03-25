#include "scene/frame_controller.hpp"
#include "editor_log.hpp"

#include "erhe/application/controller.hpp"
#include "erhe/scene/node.hpp"
#include "erhe/toolkit/bit_helpers.hpp"
#include "erhe/toolkit/math_util.hpp"
#include "erhe/toolkit/verify.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace editor
{

using glm::mat4;
using glm::vec3;
using glm::vec4;

Frame_controller::Frame_controller()
    : Node_attachment{"frame controller"}
{
    reset();
    rotate_x      .set_damp     (0.700f);
    rotate_y      .set_damp     (0.700f);
    rotate_z      .set_damp     (0.700f);
    rotate_x      .set_max_delta(0.02f);
    rotate_y      .set_max_delta(0.02f);
    rotate_z      .set_max_delta(0.02f);
    translate_x   .set_damp     (0.92f);
    translate_y   .set_damp     (0.92f);
    translate_z   .set_damp     (0.92f);
    translate_x   .set_max_delta(0.004f);
    translate_y   .set_max_delta(0.004f);
    translate_z   .set_max_delta(0.004f);
    speed_modifier.set_max_value(3.0f);
    speed_modifier.set_damp     (0.92f);
    speed_modifier.set_max_delta(0.5f);

    update();
}

auto Frame_controller::get_controller(
    const Control control
) -> erhe::application::Controller&
{
    switch (control) {
        case Control::translate_x: return translate_x;
        case Control::translate_y: return translate_y;
        case Control::translate_z: return translate_z;
        case Control::rotate_x   : return rotate_x;
        case Control::rotate_y   : return rotate_y;
        case Control::rotate_z   : return rotate_z;
        default: {
            ERHE_FATAL("bad control %04x", static_cast<unsigned int>(control));
        }
    }
}

void Frame_controller::set_position(const vec3 position)
{
    m_position = position;
    update();
}

void Frame_controller::set_elevation(const float value)
{
    m_elevation = value;
    update();
}

void Frame_controller::set_heading(const float value)
{
    m_heading = value;
    m_heading_matrix = erhe::toolkit::create_rotation(
        m_heading,
        erhe::toolkit::vector_types<float>::vec3_unit_y()
    );
    update();
}

auto Frame_controller::position() const -> vec3
{
    return m_position;
}

auto Frame_controller::elevation() const -> float
{
    return m_elevation;
}

auto Frame_controller::heading() const -> float
{
    return m_heading;
}

auto Frame_controller::static_type() -> uint64_t
{
    return erhe::scene::Item_type::node_attachment | erhe::scene::Item_type::frame_controller;
}

auto Frame_controller::static_type_name() -> const char*
{
    return "Frame_controller";
}

auto Frame_controller::get_type() const -> uint64_t
{
    return static_type();
}

auto Frame_controller::type_name() const -> const char*
{
    return static_type_name();
}

void Frame_controller::handle_node_transform_update()
{
    if (m_transform_update) {
        return;
    }

    auto* node = get_node();
    if (node == nullptr) {
        return;
    }
    const vec4 position  = node->position_in_world();
    const vec4 direction = node->direction_in_world();

    m_position = position;
    float heading  {0.0f};
    float elevation{0.0f};
    erhe::toolkit::cartesian_to_heading_elevation(direction, elevation, heading);
    m_elevation = elevation;
    m_heading   = heading;

    m_heading_matrix = erhe::toolkit::create_rotation(
        m_heading,
        erhe::toolkit::vector_types<float>::vec3_unit_y()
    );

    update();
}

void Frame_controller::reset()
{
    translate_x.reset();
    translate_y.reset();
    translate_z.reset();
    rotate_x.reset();
    rotate_y.reset();
    rotate_z.reset();
}

void Frame_controller::update()
{
    auto* node = get_node();
    if (node == nullptr) {
        return;
    }

    const mat4 elevation_matrix = erhe::toolkit::create_rotation(
        m_elevation,
        erhe::toolkit::vector_types<float>::vec3_unit_x()
    );
    m_rotation_matrix = m_heading_matrix * elevation_matrix;

    mat4 parent_from_local = m_rotation_matrix;
    //mat4 parent_to_local = transpose(local_to_parent);

    // HACK
    // if (m_position.y < 0.03f)
    // {
    //     m_position.y = 0.03f;
    // }

    // Put translation to column 3
    parent_from_local[3] = vec4{m_position, 1.0f};

    // Put inverse translation to column 3
    /*parentToLocal._03 = parentToLocal._00 * -positionInParent.X + parentToLocal._01 * -positionInParent.Y + parentToLocal._02 * - positionInParent.Z;
   parentToLocal._13 = parentToLocal._10 * -positionInParent.X + parentToLocal._11 * -positionInParent.Y + parentToLocal._12 * - positionInParent.Z;
   parentToLocal._23 = parentToLocal._20 * -positionInParent.X + parentToLocal._21 * -positionInParent.Y + parentToLocal._22 * - positionInParent.Z;
   parentToLocal._33 = 1.0f;
   */
    //m_local_from_parent = inverse(m_parent_from_local);
    m_transform_update = true;
    //node->set_parent_from_node(parent_from_local);
    node->set_world_from_node(parent_from_local);
    m_transform_update = false;

    //vec4 position  = m_node->world_from_local.matrix() * vec4{0.0f, 0.0f, 0.0f, 1.0f};
    //vec4 direction = m_node->world_from_local.matrix() * vec4{0.0f, 0.0f, 1.0f, 0.0f};
    //float elevation;
    //float heading;
    //cartesian_to_heading_elevation(direction, elevation, heading);
    //log_render.info("elevation = {:.2f}, heading = {:.2f}\n", elevation / glm::pi<float>(), heading / glm::pi<float>());

    //Frame.LocalToParent.Set(localToParent, parentToLocal);
}

auto Frame_controller::right() const -> vec3
{
    return vec3{m_heading_matrix[0]};
}

auto Frame_controller::up() const -> vec3
{
    return vec3{m_heading_matrix[1]};
}

auto Frame_controller::back() const -> vec3
{
    return vec3{m_heading_matrix[2]};
}

void Frame_controller::update_fixed_step()
{
    translate_x.update();
    translate_y.update();
    translate_z.update();
    rotate_x.update();
    rotate_y.update();
    rotate_z.update();
    speed_modifier.update();

    const float speed = 0.8f + speed_modifier.current_value();

    if (translate_x.current_value() != 0.0f) {
        m_position += right() * translate_x.current_value() * speed;
    }

    if (translate_y.current_value() != 0.0f) {
        m_position += up() * translate_y.current_value() * speed;
    }

    if (translate_z.current_value() != 0.0f) {
        m_position += back() * translate_z.current_value() * speed;
    }

    if (
        (rotate_x.current_value() != 0.0f) ||
        (rotate_y.current_value() != 0.0f)
    ) {
        m_heading += rotate_y.current_value();
        m_elevation += rotate_x.current_value();
        const mat4 elevation_matrix = erhe::toolkit::create_rotation(
            m_elevation,
            erhe::toolkit::vector_types<float>::vec3_unit_x()
        );
        m_heading_matrix = erhe::toolkit::create_rotation(
            m_heading,
            erhe::toolkit::vector_types<float>::vec3_unit_y()
        );
        m_rotation_matrix = m_heading_matrix * elevation_matrix;
    }

    update();
#if 0
   m_parent_from_local = m_rotation_matrix;
   //Matrix4 parentToLocal;

   //Matrix4.Transpose(localToParent, out parentToLocal);

   // HACK
   if (m_position.y < 0.03f)
      m_position.y = 0.03f;

   /*  Put translation to column 3  */
   m_parent_from_local[3] = vec4{m_position, 1.0f};

#    if 0
   localToParent._03 = positionInParent.X;
   localToParent._13 = positionInParent.Y;
   localToParent._23 = positionInParent.Z;
   localToParent._33 = 1.0f;

   /*  Put inverse translation to column 3 */
   parentToLocal._03 = parentToLocal._00 * -positionInParent.X + parentToLocal._01 * -positionInParent.Y + parentToLocal._02 * - positionInParent.Z;
   parentToLocal._13 = parentToLocal._10 * -positionInParent.X + parentToLocal._11 * -positionInParent.Y + parentToLocal._12 * - positionInParent.Z;
   parentToLocal._23 = parentToLocal._20 * -positionInParent.X + parentToLocal._21 * -positionInParent.Y + parentToLocal._22 * - positionInParent.Z;
   parentToLocal._33 = 1.0f;

   Frame.LocalToParent.Set(localToParent, parentToLocal);
#    endif

   m_local_from_parent = inverse(m_parent_from_local);
#endif
}

auto is_frame_controller(
    const erhe::scene::Item* const item
) -> bool
{
    if (item == nullptr)
    {
        return false;
    }
    using namespace erhe::toolkit;
    return test_all_rhs_bits_set(
        item->get_type(),
        erhe::scene::Item_type::frame_controller
    );
}

auto is_frame_controller(
    const std::shared_ptr<erhe::scene::Item>& item
) -> bool
{
    return is_frame_controller(item.get());
}

auto as_frame_controller(
    erhe::scene::Item* item
) -> Frame_controller*
{
    if (item == nullptr) {
        return nullptr;
    }
    using namespace erhe::toolkit;
    if (
        !test_all_rhs_bits_set(
            item->get_type(),
            erhe::scene::Item_type::frame_controller
        )
    ) {
        return nullptr;
    }
    return static_cast<Frame_controller*>(item);
}

auto as_frame_controller(
    const std::shared_ptr<erhe::scene::Item>& item
) -> std::shared_ptr<Frame_controller>
{
    if (!item) {
        return {};
    }
    using namespace erhe::toolkit;
    if (
        !test_all_rhs_bits_set(
            item->get_type(),
            erhe::scene::Item_type::frame_controller
        )
    ) {
        return {};
    }
    return std::static_pointer_cast<Frame_controller>(item);
}

auto get_frame_controller(
    const erhe::scene::Node* node
) -> std::shared_ptr<Frame_controller>
{
    for (const auto& attachment : node->attachments()) {
        auto frame_controller = as_frame_controller(attachment);
        if (frame_controller) {
            return frame_controller;
        }
    }
    return {};
}

} // namespace editor
