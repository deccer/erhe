#pragma once

#include "erhe/toolkit/verify.hpp"
#include "erhe/toolkit/xxhash.hpp"

#include "etl/vector.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

namespace erhe::components
{

class Component;
class Components;

#if 1
using Component_vector = std::vector<Component*>;
#else
constexpr int max_component_count = 100;
using Component_vector = etl::vector<Component*, max_component_count>
#endif

class Time_context
{
public:
    double   dt          {0.0};
    double   time        {0.0};
    uint64_t frame_number{0};
};

class IUpdate_fixed_step
{
public:
    virtual void update_fixed_step(const Time_context&) = 0;
};

class IUpdate_once_per_frame
{
public:
    virtual void update_once_per_frame(const Time_context&) = 0;
};

enum class Component_state : unsigned int
{
    Constructed,
    Declaring_initialization_requirements,
    Initialization_requirements_declared,
    Initializing,
    Initialized,
    Post_initializing,
    Ready,
    Deinitializing,
    Deinitialized
};

auto c_str(const Component_state state) -> const char*;

class Component
{
protected:
    Component     (const Component&) = delete;
    Component     (Component&&)      = delete;
    void operator=(const Component&) = delete;
    void operator=(Component&&)      = delete;

    explicit Component(const std::string_view name);
    virtual ~Component() noexcept;

public:
    // Will fail hard with ERHE_VERIFY() if component has not yet been initialized
    template<typename T>
    [[nodiscard]] auto get() const -> T*;

    // Will not trigger failure if component was not yet initialized
    template<typename T>
    [[nodiscard]] auto try_get() const -> T*;

    template<typename T>
    auto require() -> T*;

    template<typename T>
    auto optional() -> T*;

    // Public interface
    [[nodiscard]] virtual auto get_type_hash                  () const -> uint32_t = 0;
    [[nodiscard]] virtual auto processing_requires_main_thread() const -> bool;
    virtual void declare_required_components() {}
    virtual void initialize_component       () {}
    virtual void deinitialize_component     () {}
    virtual void post_initialize            () {}
    virtual void on_thread_exit             () {}
    virtual void on_thread_enter            () {}

    // Public non-virtual API
    [[nodiscard]] auto name                    () const -> std::string_view;
    [[nodiscard]] auto get_state               () const -> Component_state;
    [[nodiscard]] auto is_registered           () const -> bool;
    [[nodiscard]] auto is_ready_to_initialize  (bool in_worker_thread, bool parallel) const -> bool;
    [[nodiscard]] auto is_ready_to_deinitialize() const -> bool;
    [[nodiscard]] auto dependencies            () -> const Component_vector&;

    void register_as_component(Components* components);
    void component_initialized(Component* component);
    void depends_on           (Component* dependency);
    void is_depended_by       (Component* component); // WARNING - not multithreading safe
    void set_state            (Component_state state);

    auto get_depended_by() const -> const Component_vector&;

protected:
    Components* m_components{nullptr};

private:
    void unregister();

    std::string_view m_name;
    Component_state  m_state{Component_state::Constructed};
    Component_vector m_dependencies;
    Component_vector m_initialized_dependencies;
    Component_vector m_depended_by;
};

/// Components is a collection of Components.
/// Typically you have only one instance of Components in your application.
///
/// Usage:
///  * In each Component declare_required_components(), declare dependencies to other Components
///    with require<T>()
///  * Register all components, in any order, with Components::add()
///  * Once all components have been registered, call Components::initialize_components().
///    This will call Component::initialize() for each component, in order which respects
///    all declared dependencies. If there are circular dependencies, initialize_components()
///    will abort.
class Component;
class IUpdate_fixed_step;
class IUpdate_once_per_frame;
class Time_context;

class IExecution_queue
{
public:
    virtual ~IExecution_queue() noexcept;
    virtual void enqueue(std::function<void()>) = 0;
    virtual void wait   () = 0;
};

class Components final
{
public:
    Components    ();
    ~Components   () noexcept;
    Components    (const Components&) = delete;
    void operator=(const Components&) = delete;
    Components    (Components&&)      = delete;
    void operator=(Components&&)      = delete;

    [[nodiscard]] auto is_component_initialization_complete() -> bool;
    [[nodiscard]] auto get_component_to_deinitialize       () -> Component*;

    auto add                                   (Component* component) -> Component&;
    void show_dependencies                     () const;
    void cleanup_components                    ();
    void launch_component_initialization       (bool parallel);
    void wait_component_initialization_complete();
    void on_thread_exit                        ();
    void on_thread_enter                       ();
    void update_fixed_step                     (const Time_context& time_context);
    void update_once_per_frame                 (const Time_context& time_context);

    template<typename T>
    [[nodiscard]] auto get() const -> T*;

    void collect_uninitialized_depended_by(Component* component, std::set<Component*>& result);

private:
    [[nodiscard]] auto get_component_to_initialize(bool in_worker_thread) -> Component*;
    void queue_all_components_to_be_processed();
    void initialize_component                (bool in_worker_thread);
    void deinitialize_component              (Component* component);
    void post_initialize_components          ();

    std::mutex                        m_mutex;
    Component_vector                  m_components;
    Component_vector                  m_initialization_order;
    std::set<IUpdate_fixed_step    *> m_fixed_step_updates;
    std::set<IUpdate_once_per_frame*> m_once_per_frame_updates;
    bool                              m_parallel_initialization{false};
    bool                              m_is_ready               {false};
    std::condition_variable           m_component_processed;
    std::set<Component*>              m_components_to_process;
    std::unique_ptr<IExecution_queue> m_execution_queue;
    std::size_t                       m_initialize_component_count_worker_thread{0};
    std::size_t                       m_initialize_component_count_main_thread  {0};
    std::atomic<int>                  m_count_initialized_in_worker_thread      {0};
};

template<typename T>
[[nodiscard]] auto Component::get() const -> T*
{
    T* component = try_get<T>();
    if (!component) {
        auto get_result = m_components->get<T>();
        if (get_result) {
            ERHE_FATAL("component was not declared required");
        } else {
            ERHE_FATAL("component was not registered to Components");
        }
    }

    return component;
}

template<typename T>
[[nodiscard]] auto Component::try_get() const -> T*
{
    switch (m_state) {
        case Component_state::Initializing: {
            for (const auto& component : m_initialized_dependencies) {
                if (component->get_type_hash() == T::c_type_hash) {
                    return static_cast<T*>(component);
                }
            }
            return {};
        }

        case Component_state::Post_initializing: // fall-through
        case Component_state::Ready: {
            ERHE_VERIFY(m_components != nullptr);
            auto get_result = m_components->get<T>();
            ERHE_VERIFY(
                (!get_result) ||
                (get_result->get_state() == Component_state::Initialized) ||
                (get_result->get_state() == Component_state::Ready)
            );
            return get_result;
        }
        default: {
            ERHE_FATAL("invalid get");
        }
    }
    // unreachable
}

template<typename T>
auto Component::require() -> T*
{
    ERHE_VERIFY(get_state() == Component_state::Declaring_initialization_requirements);

    T* const component = (m_components == nullptr)
        ? nullptr
        : m_components->get<T>();
    if (component) {
        depends_on(component);
    }
    return component;
}

template<typename T>
auto Component::optional() -> T*
{
    ERHE_VERIFY(get_state() == Component_state::Declaring_initialization_requirements);

    T* const component = (m_components == nullptr)
        ? nullptr
        : m_components->get<T>();
    if (component) {
        depends_on(component);
    }
    return component;
}

template<typename T>
[[nodiscard]] auto Components::get() const -> T*
{
    for (Component* component : m_components) {
        if (component->get_type_hash() == T::c_type_hash) {
            return static_cast<T*>(component);
        }
    }
    return {};
}

} // namespace erhe::components
