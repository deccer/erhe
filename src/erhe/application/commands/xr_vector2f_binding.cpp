#include "erhe/application/commands/xr_vector2f_binding.hpp"
#include "erhe/application/commands/command.hpp"
#include "erhe/application/commands/command_context.hpp"
#include "erhe/application/application_log.hpp"
#include "erhe/toolkit/verify.hpp"

namespace erhe::application {

Xr_vector2f_binding::Xr_vector2f_binding(
    Command* const                      command,
    erhe::xr::Xr_action_vector2f* const xr_action
)
    : Command_binding{command}
    , xr_action      {xr_action}
{
}

Xr_vector2f_binding::Xr_vector2f_binding() = default;

Xr_vector2f_binding::~Xr_vector2f_binding() noexcept = default;

[[nodiscard]] auto Xr_vector2f_binding::get_type() const -> Type
{
    return Type::Xr_vector2f;
}

auto Xr_vector2f_binding::on_value_changed(
    Input_arguments& input
) -> bool
{
    auto* const command = get_command();
    if (command->get_command_state() == State::Disabled) {
        return false;
    }

    command->try_ready();
    const bool consumed = command->try_call_with_input(input);
    if (consumed) {
        log_input_event_consumed->info(
            "{} consumed controller OpenXR vector2f input event",
            command->get_name()
        );
    }

    return consumed;
}

} // namespace erhe::application

