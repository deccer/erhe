#pragma once
// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "erhe/geometry/geometry_log.hpp"
#include "erhe/toolkit/verify.hpp"

#include <glm/glm.hpp>

#include <sstream>

namespace erhe::geometry
{

template<typename T>
class Pair_entries
{
public:
    class Entry
    {
    public:
        Entry(T primary, T secondary)
            : primary  {primary}
            , secondary{secondary}
        {
        }
        void swap(T lhs, T rhs)
        {
            if (primary == lhs) {
                primary = rhs;
            } else if (primary == rhs) {
                primary = lhs;
            }
            if (secondary == lhs) {
                secondary = rhs;
            } else if (secondary == rhs) {
                secondary = lhs;
            }
        }

        T primary;
        T secondary;
    };

    auto find_primary(const T primary) const -> Entry*
    {
        for (auto& i : entries) {
            if (i.primary == primary) {
                return &i;
            }
        }
        return nullptr;
    }
    auto find_secondary(const T secondary) -> Entry*
    {
        for (auto& i : entries) {
            if (i.secondary == secondary) {
                return &i;
            }
        }
        return nullptr;
    }

    void insert(const T primary, const T secondary)
    {
        entries.emplace_back(primary, secondary);
    }

    auto size() const -> size_t
    {
        return entries.size();
    }

    void swap(T lhs, T rhs)
    {
        for (auto& entry : entries) {
            entry.swap(lhs, rhs);
        }
    }
    std::vector<Entry> entries;
};

template<typename T>
class Remapper
{
public:
    explicit Remapper(const T size)
        : old_size{size}
        , new_size{size}
        , new_end {size}
    {
        old_from_new.resize(size);
        new_from_old.resize(size);
        old_used.resize(size);

        for (T new_id = 0; new_id < size; ++new_id) {
            old_from_new[new_id] = new_id;
        }

        std::fill(old_used.begin(), old_used.end(), false);
    }

    void create_new_from_old_mapping()
    {
        for (T new_id = 0; new_id < new_size; ++new_id) {
            const T old_id = old_from_new[new_id];
            new_from_old[old_id] = new_id;
        }
    }

    void dump()
    {
        bool error = false;
        {
            std::stringstream ss;
            for (T old_id = 0; old_id < old_size; ++old_id) {
                const T new_id = new_from_old[old_id];
                ss << fmt::format("{:2}", new_id);
                if (is_bijection && (old_id != std::numeric_limits<T>::max()) && (old_from_new[new_id] != old_id)) {
                    error = true;
                    ss << "!";
                } else {
                    ss << " ";
                }
            }
            ss << "  < new from old\n";
            for (T old_id = 0; old_id < old_size; ++old_id) {
                ss << fmt::format("{:2} ", old_id);
            }
            ss << "  < old\n";
            ss << "\n";
            ss << "    \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/\n";
            ss << "    /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\\n";
            ss << "\n";

            for (T new_id = 0; new_id < old_size; ++new_id) {
                ss << fmt::format("{:2} ", new_id);
            }
            ss << "  < new\n";
            for (T new_id = 0; new_id < new_size; ++new_id) {
                const T old_id = old_from_new[new_id];
                ss << fmt::format("{:2}", old_id);
                if (is_bijection && (old_id != std::numeric_limits<T>::max()) && (new_from_old[old_id] != new_id)) {
                    error = true;
                    ss << "!";
                } else {
                    ss << " ";
                }
            }
            ss << "  < old from new";
            log_weld->trace("---------------------------------\n{}", ss.str());
            if (error) {
                log_weld->error("Errors detected");
            }
        }

        {
            std::stringstream ss;
            for (T id : eliminate) {
                ss << fmt::format(" {}", id);
            }
            log_weld->trace("Eliminate list: {}", ss.str());
        }

        {
            std::stringstream ss;
            for (T new_id = new_end; new_id < old_size; ++new_id) {
                const T old = old_id(new_id);
                ss << fmt::format(" new {} old {}", new_id, old);
            }
            log_weld->trace("Drop list: {}", ss.str());
        }
    }

    auto old_id(const T new_id) const -> T
    {
        return old_from_new[new_id];
    }

    auto new_id(const T old_id) const -> T
    {
        return new_from_old[old_id];
    }

    void swap(const T secondary_new_id, const T keep_new_id)
    {
        ERHE_VERIFY(secondary_new_id != keep_new_id);
        const T secondary_old_id = old_from_new[secondary_new_id];
        const T keep_old_id      = old_from_new[keep_new_id];
        SPDLOG_LOGGER_TRACE(
            log_weld,
            "New {:2} old {:2} is being removed - swapping with new {:2} old {:2}",
            secondary_new_id, secondary_old_id,
            keep_new_id, keep_old_id
        );
        std::swap(
            old_from_new[secondary_new_id],
            old_from_new[keep_new_id]
        );
        std::swap(
            new_from_old[secondary_old_id],
            new_from_old[keep_old_id]
        );

        for (size_t i = 0; i < merge.size(); ++i) {
            merge.entries[i].swap(keep_new_id, secondary_new_id);
        }
        for (size_t i = 0; i < eliminate.size(); ++i) {
            if (eliminate[i] == keep_new_id) {
                eliminate[i] = secondary_new_id;
            } else if (eliminate[i] == secondary_new_id) {
                eliminate[i] = keep_new_id;
            }
        }
    }

    auto get_next_end(const bool check_used = false) -> T
    {
        for (;;) {
            ERHE_VERIFY(new_end > 0);
            --new_end;
            if (check_used && !old_used[old_from_new[new_end]]) {
                continue;
            }
            return new_end;
        }
    }

    void reorder_to_drop_merge_duplicates_and_elimitated()
    {
        {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
            std::stringstream ss;
            for (auto entry : merge.entries) {
                ss << fmt::format(" {}->{}", entry.secondary, entry.primary);
            }
            SPDLOG_LOGGER_TRACE(log_weld, "Merge list:\n{}", ss.str());
#endif
        }

        {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
            std::stringstream ss;
#endif
            for (size_t i = 0, end = merge.size(); i < end; ++i) {
                const auto& entry = merge.entries[i];
                const T secondary_new_id = entry.secondary;
                if (secondary_new_id >= new_end) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                    ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, secondary_new_id);
#endif
                    continue;
                }
                const T keep_new_id = get_next_end();
                if (secondary_new_id == keep_new_id) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                    ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, secondary_new_id);
#endif
                    continue;
                }
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, keep_new_id);
#endif
                swap(secondary_new_id, keep_new_id);
            }
            SPDLOG_LOGGER_TRACE(log_weld, "Dropped due to merge:{}", ss.str());
        }

        {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
            std::stringstream ss;
#endif
            for (size_t i = 0, end = eliminate.size(); i < end; ++i) {
                T secondary_new_id = eliminate[i];
                if (secondary_new_id >= new_end) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                    ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, secondary_new_id);
#endif
                    continue;
                }
                T keep_new_id = get_next_end();
                if (secondary_new_id == keep_new_id) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                    ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, secondary_new_id);
#endif
                    continue;
                }
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
                ss << fmt::format(" {:2} -> {:2} ", secondary_new_id, keep_new_id);
#endif
                swap(secondary_new_id, keep_new_id);
            }
            SPDLOG_LOGGER_TRACE(log_weld, "Dropped due to eliminate:{}", ss.str());
        }
    }

    void reorder_to_drop_unused()
    {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
        std::stringstream ss;
#endif
        new_end = 0;
        for (T new_id = 0, end = new_size; new_id < end; ++new_id) {
            const T old_id = old_from_new[new_id];
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
            ss << fmt::format(
                "new {:2} old {:2} : {}\n",
                new_id,
                old_id,
                (
                    old_used[old_id]
                        ? "keep old"
                        : "drop old"
                )
            );
#endif
            if (old_used[old_id]) {
                new_end = new_id + 1;
            }
        }
        SPDLOG_LOGGER_TRACE(log_weld, "Usage:\n{}", ss.str());

        for (T new_id = 0, end = new_size; new_id < end; ++new_id) {
            const T old_id = old_from_new[new_id];
            if (!old_used[old_id]) {
                T secondary_new_id = new_id;
                if (secondary_new_id >= new_end) {
                    SPDLOG_LOGGER_TRACE(log_weld, "Dropping unused, end {:2} new {:2} old {:2}", new_end, secondary_new_id, old_id);
                    continue;
                }
                T keep_new_id = get_next_end(true);
                if (secondary_new_id == keep_new_id) {
                    SPDLOG_LOGGER_TRACE(log_weld, "Dropping unused, end {:2} new {:2} old {:2}", new_end, secondary_new_id, old_id);
                    continue;
                }
                SPDLOG_LOGGER_TRACE(
                    log_weld,
                    "Dropping unused, end {:2} new {:2} old {:2} -> new {:2} old {:2}",
                    new_end,
                    secondary_new_id,
                    old_id,
                    keep_new_id,
                    old_from_new[keep_new_id]
                );
                swap(secondary_new_id, keep_new_id);
            }
        }
        //log_weld.trace("\n");
    }

    // For each new (secondary) T that is mapped to be merged with primary T,
    // execute the callback function. The callback function will be passed
    //  - primary (kept merge T) new and old ids
    //  - secondary (dropped merge T) new and old ids
    void for_each_primary_new(
        const T primary_new_id,
        std::function<void(
            const T /*primary_new_id*/,   const T /*primary_old_id*/,
            const T /*secondary_new_id*/, const T /*secondary_old_id*/)
        > callback
    )
    {
        if (!callback) {
            return;
        }
        for (std::size_t i = 0, end = merge.size(); i < end; ++i) {
            const auto& entry = merge.entries[i];
            if (entry.primary != primary_new_id) {
                continue;
            }
            ERHE_VERIFY(primary_new_id == entry.primary);
            const T primary_old_id   = old_from_new[primary_new_id];
            const T secondary_new_id = entry.secondary;
            const T secondary_old_id = old_from_new[secondary_new_id];
            callback(primary_new_id, primary_old_id, secondary_new_id, secondary_old_id);
        }
    }

    void merge_pass(
        std::function<
            void(
                T primary_new_id,
                T primary_old_id,
                T secondary_new_id,
                T secondary_old_id
            )
        > swap_callback
    )
    {
        if (!swap_callback) {
            return;
        }
        for (std::size_t i = 0, end = merge.size(); i < end; ++i) {
            const auto& entry = merge.entries[i];
            const T primary_new_id   = entry.primary;
            const T primary_old_id   = old_from_new[primary_new_id];
            const T secondary_new_id = entry.secondary;
            const T secondary_old_id = old_from_new[secondary_new_id];
            swap_callback(primary_new_id, primary_old_id, secondary_new_id, secondary_old_id);
        }
    }

    void update_secondary_new_from_old()
    {
        for (std::size_t i = 0, end = merge.size(); i < end; ++i) {
            const auto& entry = merge.entries[i];
            const T primary_new_id   = entry.primary;
            const T secondary_new_id = entry.secondary;
            const T secondary_old_id = old_from_new[secondary_new_id];
            new_from_old[secondary_old_id] = primary_new_id;
        }
        is_bijection = false;
    }

    void trim(std::function<void(const T new_id, const T old_id)> remove_callback)
    {
        dump();

        if (remove_callback) {
            std::vector<T> failed;
            for (T new_id = new_end; new_id < old_size; ++new_id) {
                const T old = old_id(new_id);
                log_weld->trace("Removing new {} old {}", new_id, old);
                bool ok{true};
                for (T keep_id : new_from_old) { // old may may not be mapped to new which is being deleted
                    if (keep_id >= new_end) {
                        continue; // removed -> ok
                    }
                    if (new_id == keep_id) {
                        failed.push_back(new_id);
                        ok = false;
                    }
                }
                if (ok) {
                    remove_callback(new_id, old_id(new_id));
                }
            }
            if (!failed.empty()) {
                std::stringstream ss;
                ss << "Failed: ";
                bool empty{true};

                for (T fail : failed) {
                    if (!empty) {
                        ss << ", ";
                    }
                    ss << fail;
                    empty = false;
                }
                log_weld->error("{}", ss.str());
            }

        }
        trim();
    }

    void trim()
    {
        //for (T new_id = merge_end; new_id < old_size; ++new_id) {
        //    SPDLOG_LOGGER_TRACE(log_weld, "Removing new {} old {}", new_id, old_id(new_id));
        //    for (T keep_id : new_from_old) { // old may may not be mapped to new which is being deleted
        //        ERHE_VERIFY(new_id != keep_id);
        //    }
        //}
        SPDLOG_LOGGER_TRACE(log_weld, "is_bijection {} -> false", is_bijection);
        SPDLOG_LOGGER_TRACE(log_weld, "new_size {} -> {}", new_size, new_end);
        is_bijection = false;
        new_size = new_end;
    }

    void use_old(const T old_id)
    {
        old_used[old_id] = true;
    }

    T                 old_size{0};
    T                 new_size{0};
    T                 new_end{0};
    bool              is_bijection{true};
    std::vector<bool> old_used;
    std::vector<T>    old_from_new;
    std::vector<T>    new_from_old;
    Pair_entries<T>   merge;
    std::vector<T>    eliminate; // TODO must be set
};

}
