#pragma once

#include "erhe/net/net_os.hpp"

namespace erhe::net
{

class Select_sockets
{
public:
    Select_sockets();

    static constexpr int flag_read   = (1u << 0u);
    static constexpr int flag_write  = (1u << 1u);
    static constexpr int flag_except = (1u << 2u);

    auto has_read  () const -> bool;
    auto has_write () const -> bool;
    auto has_except() const -> bool;
    auto has_read  (SOCKET socket) const -> bool;
    auto has_write (SOCKET socket) const -> bool;
    auto has_except(SOCKET socket) const -> bool;
    void set_read  (SOCKET socket);
    void set_write (SOCKET socket);
    void set_except(SOCKET socket);
    auto select    (int timeout_ms) -> int;

    unsigned int flags{0};
    int          nfds{0};
    FD_SET       read_fds;
    FD_SET       write_fds;
    FD_SET       except_fds;
};

}
