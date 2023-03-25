#include "erhe/graphics/buffer_transfer_queue.hpp"
#include "erhe/gl/enum_bit_mask_operators.hpp"
#include "erhe/graphics/buffer.hpp"
#include "erhe/graphics/graphics_log.hpp"
#include "erhe/graphics/scoped_buffer_mapping.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#include <fmt/format.h>

namespace erhe::graphics
{

Buffer_transfer_queue::Buffer_transfer_queue()
{
}

Buffer_transfer_queue::~Buffer_transfer_queue() noexcept
{
    flush();
}

void Buffer_transfer_queue::enqueue(
    Buffer&                buffer,
    const std::size_t      offset,
    std::vector<uint8_t>&& data
)
{
    const std::lock_guard<std::mutex> lock{m_mutex};

    log_buffer->trace(
        "queued buffer {} transfer offset = {} size = {}",
        buffer.gl_name(),
        offset,
        data.size()
    );
    m_queued.emplace_back(buffer, offset, std::move(data));
}

void Buffer_transfer_queue::flush()
{
    ERHE_PROFILE_FUNCTION

    const std::lock_guard<std::mutex> lock{m_mutex};

    for (const auto& entry : m_queued) {
        log_buffer->trace(
            "buffer upload {} transfer offset = {} size = {}",
            entry.target.gl_name(),
            entry.target_offset,
            entry.data.size()
        );
        Scoped_buffer_mapping<uint8_t> scoped_mapping{
            entry.target,
            entry.target_offset,
            entry.data.size(),
            gl::Map_buffer_access_mask::map_invalidate_range_bit |
            gl::Map_buffer_access_mask::map_write_bit
        };
        auto& destination = scoped_mapping.span();
        memcpy(destination.data(), entry.data.data(), entry.data.size());
    }
    m_queued.clear();
}

} // namespace erhe::graphics
