#pragma once

#include "oscar/Platform/LogSink.hpp"
#include "oscar/Platform/LogMessageView.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace osc
{
    class Logger final {
    public:
        Logger(std::string _name) :
            m_Name{std::move(_name)},
            m_Sinks()
        {
        }

        Logger(std::string _name, std::shared_ptr<LogSink> _sink) :
            m_Name{std::move(_name)},
            m_Sinks{_sink}
        {
        }

        template<typename... Args>
        void log(LogLevel msgLvl, char const* fmt, ...)
        {
            if (msgLvl < level)
            {
                return;
            }

            // create a not-allocated view of the log message that
            // the sinks _may_ consume
            std::vector<char> buf(2048);
            size_t n = 0;
            {
                va_list args;
                va_start(args, fmt);
                int rv = std::vsnprintf(buf.data(), buf.size(), fmt, args);
                va_end(args);

                if (rv <= 0)
                {
                    return;
                }

                n = std::min(static_cast<size_t>(rv), buf.size()-1);
            }
            LogMessageView view{m_Name, std::string_view{buf.data(), n}, msgLvl};

            // sink it
            for (auto& sink : m_Sinks)
            {
                if (sink->should_log(view.level))
                {
                    sink->log(view);
                }
            }
        }

        template<typename... Args>
        void trace(char const* fmt, Args const&... args)
        {
            log(LogLevel::trace, fmt, args...);
        }

        template<typename... Args>
        void debug(char const* fmt, Args const&... args)
        {
            log(LogLevel::debug, fmt, args...);
        }

        template<typename... Args>
        void info(char const* fmt, Args const&... args)
        {
            log(LogLevel::info, fmt, args...);
        }

        template<typename... Args>
        void warn(char const* fmt, Args const&... args)
        {
            log(LogLevel::warn, fmt, args...);
        }

        template<typename... Args>
        void error(char const* fmt, Args const&... args)
        {
            log(LogLevel::err, fmt, args...);
        }

        template<typename... Args>
        void critical(char const* fmt, Args const&... args)
        {
            log(LogLevel::critical, fmt, args...);
        }

        [[nodiscard]] std::vector<std::shared_ptr<LogSink>> const& sinks() const noexcept
        {
            return m_Sinks;
        }

        [[nodiscard]] std::vector<std::shared_ptr<LogSink>>& sinks() noexcept
        {
            return m_Sinks;
        }

    private:
        std::string m_Name;
        std::vector<std::shared_ptr<LogSink>> m_Sinks;
        LogLevel level = LogLevel::trace;
    };
}