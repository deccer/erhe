#include "scene/scene_commands.hpp"

#include "editor_scenes.hpp"
#include "operations/compound_operation.hpp"
#include "operations/insert_operation.hpp"
#include "operations/node_operation.hpp"
#include "operations/operation_stack.hpp"
#include "rendertarget_mesh.hpp"
#include "rendertarget_imgui_viewport.hpp"
#include "scene/node_raytrace.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_window.hpp"
#include "scene/viewport_windows.hpp"
#include "tools/selection_tool.hpp"

#include "erhe/application/commands/commands.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/toolkit/profile.hpp"

namespace editor
{

using erhe::scene::Item_flags;

#pragma region Command

Create_new_camera_command::Create_new_camera_command()
    : Command{"scene.create_new_camera"}
{
}

auto Create_new_camera_command::try_call() -> bool
{
    return g_scene_commands->create_new_camera().operator bool();
}

Create_new_empty_node_command::Create_new_empty_node_command()
    : Command{"scene.create_new_empty_node"}
{
}

auto Create_new_empty_node_command::try_call() -> bool
{
    return g_scene_commands->create_new_empty_node().operator bool();
}

Create_new_light_command::Create_new_light_command()
    : Command{"scene.create_new_light"}
{
}

auto Create_new_light_command::try_call() -> bool
{
    return g_scene_commands->create_new_light().operator bool();
}
#pragma endregion Command

Scene_commands* g_scene_commands{nullptr};

Scene_commands::Scene_commands()
    : erhe::components::Component{c_type_name}
{
}

Scene_commands::~Scene_commands() noexcept
{
    ERHE_VERIFY(g_scene_commands == this);
    g_scene_commands = nullptr;
}

void Scene_commands::declare_required_components()
{
    require<erhe::application::Commands>();
}

void Scene_commands::initialize_component()
{
    ERHE_PROFILE_FUNCTION
    ERHE_VERIFY(g_scene_commands == nullptr);

    auto& commands = *erhe::application::g_commands;
    commands.register_command   (&m_create_new_camera_command);
    commands.register_command   (&m_create_new_empty_node_command);
    commands.register_command   (&m_create_new_light_command);
    commands.bind_command_to_key(&m_create_new_camera_command,     erhe::toolkit::Key_f2, true);
    commands.bind_command_to_key(&m_create_new_empty_node_command, erhe::toolkit::Key_f3, true);
    commands.bind_command_to_key(&m_create_new_light_command,      erhe::toolkit::Key_f4, true);

    g_scene_commands = this;
}

auto Scene_commands::get_scene_root(erhe::scene::Node* parent) const -> Scene_root*
{
    if (parent != nullptr) {
        return reinterpret_cast<Scene_root*>(parent->get_item_host());
    }

    const auto  first_selected_node  = g_selection_tool->get_first_selected_node();
    const auto  first_selected_scene = g_selection_tool->get_first_selected_scene();
    const auto& viewport_window      = g_viewport_windows->last_window();

    erhe::scene::Scene_host* scene_host = first_selected_node
        ? first_selected_node->get_item_host()
        : first_selected_scene
            ? first_selected_scene->get_root_node()->get_item_host()
            : viewport_window
                ? viewport_window->get_scene_root().get()
                : nullptr;
    if (scene_host == nullptr) {
        return nullptr;
    }

    Scene_root* scene_root = reinterpret_cast<Scene_root*>(scene_host);
    return scene_root;
}

auto Scene_commands::create_new_camera(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Camera>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node = std::make_shared<erhe::scene::Node>("new camera node");
    auto new_camera = std::make_shared<erhe::scene::Camera>("new camera");
    new_node->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
    new_camera->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    g_operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Node_insert_remove_operation>(
                        Node_insert_remove_operation::Parameters{
                            .node   = new_node,
                            .parent = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode   = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_camera, new_node)
                }
            }
        )
    );

    return new_camera;
}

auto Scene_commands::create_new_empty_node(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Node>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_empty_node = std::make_shared<erhe::scene::Node>("new empty node");
    new_empty_node->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
    g_operation_stack->push(
        std::make_shared<Node_insert_remove_operation>(
            Node_insert_remove_operation::Parameters{
                .node   = new_empty_node,
                .parent = (parent != nullptr)
                    ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                    : scene_root->get_hosted_scene()->get_root_node(),
                .mode   = Scene_item_operation::Mode::insert
            }
        )
    );

    return new_empty_node;
}

auto Scene_commands::create_new_light(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Light>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node = std::make_shared<erhe::scene::Node>("new light node");
    auto new_light = std::make_shared<erhe::scene::Light>("new light");
    new_node->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    new_light->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    new_light->layer_id = scene_root->layers().light()->id;
    g_operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Node_insert_remove_operation>(
                        Node_insert_remove_operation::Parameters{
                            .node   = new_node,
                            .parent = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode   = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_light, new_node)
                }
            }
        )
    );

    return new_light;
}

auto Scene_commands::create_new_rendertarget(
    erhe::scene::Node* parent
) -> std::shared_ptr<Rendertarget_mesh>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node = std::make_shared<erhe::scene::Node>("new light node");
    auto new_mesh = std::make_shared<Rendertarget_mesh>(
        2048,
        2048,
        600.0f
    );
    new_mesh->mesh_data.layer_id = scene_root->layers().rendertarget()->id;
    new_mesh->enable_flag_bits(
        erhe::scene::Item_flags::rendertarget |
        erhe::scene::Item_flags::visible      |
        erhe::scene::Item_flags::translucent  |
        erhe::scene::Item_flags::show_in_ui
    );

    new_node = std::make_shared<erhe::scene::Node>("Hud RT node");
    new_node->set_parent_from_node(
        erhe::toolkit::mat4_rotate_xz_180
    );
    new_node->set_parent(scene_root->scene().get_root_node());
    new_node->attach(new_mesh);
    new_node->enable_flag_bits(
        erhe::scene::Item_flags::rendertarget |
        erhe::scene::Item_flags::visible      |
        erhe::scene::Item_flags::show_in_ui
    );
    auto node_raytrace = new_mesh->get_node_raytrace();
    if (node_raytrace) {
        new_node->attach(node_raytrace);
    }

    auto rendertarget_imgui_viewport = std::make_shared<Rendertarget_imgui_viewport>(
        new_mesh.get(),
        "Rendertarget Viewport"
    );

    rendertarget_imgui_viewport->set_menu_visible(true);
    erhe::application::g_imgui_windows->queue(
        [rendertarget_imgui_viewport]()
        {
            erhe::application::g_imgui_windows->register_imgui_viewport(rendertarget_imgui_viewport);
        }
    );

    new_mesh->mesh_data.layer_id = scene_root->layers().rendertarget()->id;
    g_operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Node_insert_remove_operation>(
                        Node_insert_remove_operation::Parameters{
                            .node   = new_node,
                            .parent = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode   = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_mesh, new_node)
                }
            }
        )
    );

    return new_mesh;
}

} // namespace editor
