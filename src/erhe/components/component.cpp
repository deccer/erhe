#include "erhe/components/components.hpp"
#include "erhe/components/components_log.hpp"
#include "erhe/toolkit/verify.hpp"
#include "erhe/toolkit/profile.hpp"

namespace erhe::components
{

auto c_str(const Component_state state) -> const char*
{
    switch (state) {
        // using enum Component_state;
        case Component_state::Constructed:                           return "Constructed";
        case Component_state::Declaring_initialization_requirements: return "Declaring_initialization_requirements";
        case Component_state::Initialization_requirements_declared:  return "Initialization_requirements_declared";
        case Component_state::Initializing:                          return "Initializing";
        case Component_state::Initialized:                           return "Initialized";
        case Component_state::Post_initializing:                     return "Post_initializing";
        case Component_state::Ready:                                 return "Ready";
        case Component_state::Deinitializing:                        return "Deinitializing";
        case Component_state::Deinitialized:                         return "Deinitialized";
        default: return "?";
    }
}

Component::Component(const std::string_view name)
    : m_name{name}
{
    ERHE_PROFILE_FUNCTION
}

Component::~Component() noexcept
{
    unregister();
}

auto Component::processing_requires_main_thread() const -> bool
{
    return false;
}

auto Component::name() const -> std::string_view
{
    return m_name;
}

auto Component::is_registered() const -> bool
{
    return m_components != nullptr;
}

void Component::register_as_component(Components* components)
{
    m_components = components;
}

void Component::unregister()
{
    m_components = nullptr;
}

auto Component::dependencies() -> const Component_vector&
{
    return m_dependencies;
}

void Component::depends_on(Component* dependency)
{
    ERHE_VERIFY(dependency);

    if (!dependency->is_registered()) {
        log_components->error(
            "Component {} dependency {} has not been registered as a Component",
            name(),
            dependency->name()
        );
        ERHE_FATAL("Dependency has not been registered");
    }
    m_dependencies.push_back(dependency);
}

void Component::is_depended_by(Component* component)
{
    // WARNING - not multithreading safe
    ERHE_VERIFY(component != nullptr);
    m_depended_by.push_back(component);
}

auto Component::get_depended_by() const -> const Component_vector&
{
    return m_depended_by;
}

void Component::set_state(Component_state state)
{
    switch (m_state) {
        case Component_state::Constructed: {
            ERHE_VERIFY(state == Component_state::Declaring_initialization_requirements);
            m_state = state;
            break;
        }
        case Component_state::Declaring_initialization_requirements: {
            ERHE_VERIFY(state == Component_state::Initialization_requirements_declared);
            m_state = state;
            break;
        }
        case Component_state::Initialization_requirements_declared: {
            ERHE_VERIFY(state == Component_state::Initializing);
            m_state = state;
            break;
        }
        case Component_state::Initializing: {
            ERHE_VERIFY(state == Component_state::Initialized);
            m_state = state;
            break;
        }
        case Component_state::Initialized: {
            ERHE_VERIFY(state == Component_state::Post_initializing);
            m_state = state;
            break;
        }
        case Component_state::Post_initializing: {
            ERHE_VERIFY(state == Component_state::Ready);
            m_state = state;
            break;
        }
        case Component_state::Ready: {
            ERHE_VERIFY(state == Component_state::Deinitializing);
            m_state = state;
            break;
        }
        case Component_state::Deinitializing: {
            ERHE_VERIFY(state == Component_state::Deinitialized);
            m_state = state;
            m_initialized_dependencies.clear();
            break;
        }
        default: {
            ERHE_FATAL("invalid state transition");
            break;
        }
    }
}

auto Component::get_state() const -> Component_state
{
    return m_state;
}

auto Component::is_ready_to_initialize(
    const bool in_worker_thread,
    const bool parallel
) const -> bool
{
    if (m_state != Component_state::Initialization_requirements_declared) {
        log_components->trace(
            "{} is not ready to initialize: state {} is not Connected",
            name(),
            c_str(m_state)
        );
        return false;
    }

    const bool requires_main = processing_requires_main_thread();
    const bool is_ready =
        m_dependencies.empty() &&
        (
            !parallel ||
            //!requires_main ||
            (in_worker_thread != requires_main)
        );
    if (!is_ready && !m_dependencies.empty()) {
        log_components->trace(
            "dependencies:",
            name(),
            requires_main,
            in_worker_thread ? "worker-thread" : "main-thread"
        );
        for (const auto& component : m_dependencies) {
            log_components->trace(
                "    {}: {}",
                component->name(),
                c_str(component->get_state())
            );
        }
    }
    log_components->trace(
        "{} {} {} {}",
        name(),
        is_ready         ? "is-ready" : "is-waiting",
        requires_main    ? "init-in-main" : "init-in-worker",
        in_worker_thread ? "thread-is-worker" : "thread-is-main"
    );
    return is_ready;
}

auto Component::is_ready_to_deinitialize() const -> bool
{
    if (m_state != Component_state::Ready) {
        log_components->trace(
            "{} is not ready to initialize: state {} is not Ready",
            name(),
            c_str(m_state)
        );
        return false;
    }

    return true;
}

void Component::component_initialized(Component* component)
{
    Component* dependency = nullptr;
    for (const auto& i : m_dependencies) {
        if (i == component) {
            dependency = i;
            break;
        }
    }
    if (dependency == nullptr) {
        return;
    }

    const auto remove_it = std::remove_if(
        m_dependencies.begin(),
        m_dependencies.end(),
        [component](auto entry)
        {
            return entry == component;
        }
    );

    ERHE_VERIFY(remove_it != m_dependencies.end());
    m_initialized_dependencies.push_back(dependency);
    m_dependencies.erase(
        remove_it,
        m_dependencies.end()
    );
}

} // namespace erhe::components
