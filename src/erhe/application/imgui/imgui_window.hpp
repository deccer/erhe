#pragma once

#include <memory>
#include <string>
#include <string_view>

typedef int ImGuiWindowFlags;

namespace erhe::graphics
{
    class Texture;
}

namespace erhe::components
{
    class Components;
}

namespace erhe::application
{

class Commands;
#if defined(ERHE_GUI_LIBRARY_IMGUI)
class Imgui_renderer;
#endif
class Imgui_viewport;

/// <summary>
/// Wrapper for ImGui window.
/// </summary>
/// Each Imgui_window must be hosted in exactly one Imgui_viewport.
class Imgui_window
{
public:
    explicit Imgui_window(std::string_view title);
    virtual ~Imgui_window() noexcept;

    [[nodiscard]] auto is_visible     () const -> bool;
    [[nodiscard]] auto is_hovered     () const -> bool;
    [[nodiscard]] auto title          () const -> const std::string_view;
    [[nodiscard]] auto get_scale_value() const -> float;
    [[nodiscard]] auto show_in_menu   () const -> bool;
    auto begin            () -> bool;
    void end              ();
    void set_visibility   (bool visible);
    void show             ();
    void hide             ();
    void toggle_visibility();
    void image(
        const std::shared_ptr<erhe::graphics::Texture>& texture,
        const int                                       width,
        const int                                       height
    );

    auto get_viewport() const -> Imgui_viewport*;
    virtual void set_viewport(Imgui_viewport* imgui_viewport);

    virtual void imgui               () = 0;
    virtual void hidden              ();
    virtual auto get_window_type_hash() const -> uint32_t;
    virtual void on_begin            ();
    virtual void on_end              ();
    virtual auto flags               () -> ImGuiWindowFlags;
    virtual auto has_toolbar         () const -> bool;
    virtual void toolbar             (bool& hovered);
    virtual auto want_keyboard_events() const -> bool;
    virtual auto want_mouse_events   () const -> bool;

protected:
    Imgui_viewport*   m_imgui_viewport{nullptr};
    bool              m_is_visible    {true};
    bool              m_is_hovered    {false};
    bool              m_show_in_menu  {true};
    const std::string m_title;
    float             m_min_size[2]{120.0f, 120.0f};
    float             m_max_size[2]{99999.0f, 99999.0f};
};

} // namespace erhe::application
