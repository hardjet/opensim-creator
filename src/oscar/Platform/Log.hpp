#pragma once

#include "oscar/Utils/CircularBuffer.hpp"
#include "oscar/Utils/CStringView.hpp"
#include "oscar/Utils/SynchronizedValue.hpp"

#include <nonstd/span.hpp>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// log: logging implementation
//
// this implementation takes heavy inspiration from `spdlog`

namespace osc::log
{
    enum class Level : int32_t {
        FIRST = 0,
        trace = FIRST,
        debug,
        info,
        warn,
        err,
        critical,
        off,
        NUM_LEVELS,
    };

    CStringView toCStringView(Level);

    // a log message
    //
    // to prevent needless runtime allocs, this does not own its data. See below if you need an
    // owning version
    struct LogMessage final {

        LogMessage() = default;

        LogMessage(std::string_view loggerName_,
                   std::string_view payload_,
                   Level level_) :
            loggerName{loggerName_},
            time{std::chrono::system_clock::now()},
            payload{payload_},
            level{level_}
        {
        }

        std::string_view loggerName;
        std::chrono::system_clock::time_point time;
        std::string_view payload;
        Level level;
    };

    // a log message that owns all its data
    //
    // useful if you need to persist a log message somewhere
    struct OwnedLogMessage final {

        OwnedLogMessage() = default;

        OwnedLogMessage(log::LogMessage const& msg) :
            loggerName{msg.loggerName},
            time{msg.time},
            payload{msg.payload},
            level{msg.level}
        {
        }

        std::string loggerName;
        std::chrono::system_clock::time_point time;
        std::string payload;
        Level level;
    };

    class Sink {
    protected:
        Sink() = default;
        Sink(Sink const&) = delete;
        Sink(Sink&&) noexcept = delete;
        Sink& operator=(Sink const&) = delete;
        Sink& operator=(Sink&&) noexcept = delete;
    public:
        virtual ~Sink() noexcept = default;

        void log(LogMessage const& logMessage)
        {
            implLog(logMessage);
        }

        void set_level(Level level) noexcept
        {
            m_SinkLevel = level;
        }

        [[nodiscard]] Level level() const noexcept
        {
            return m_SinkLevel;
        }

        [[nodiscard]] bool should_log(Level level) const noexcept
        {
            return level >= m_SinkLevel;
        }

    private:
        virtual void implLog(LogMessage const&) = 0;

        Level m_SinkLevel = Level::info;
    };

    class Logger final {
    public:
        Logger(std::string _name) :
            m_Name{std::move(_name)},
            m_Sinks()
        {
        }

        Logger(std::string _name, std::shared_ptr<Sink> _sink) :
            m_Name{std::move(_name)},
            m_Sinks{_sink}
        {
        }

        template<typename... Args>
        void log(Level msgLvl, char const* fmt, ...)
        {
            if (msgLvl < level)
            {
                return;
            }

            // create the log message
            thread_local std::vector<char> buf(2048);
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
            LogMessage msg{m_Name, std::string_view{buf.data(), n}, msgLvl};

            // sink it
            for (auto& sink : m_Sinks)
            {
                if (sink->should_log(msg.level))
                {
                    sink->log(msg);
                }
            }
        }

        template<typename... Args>
        void trace(char const* fmt, Args const&... args)
        {
            log(Level::trace, fmt, args...);
        }

        template<typename... Args>
        void debug(char const* fmt, Args const&... args)
        {
            log(Level::debug, fmt, args...);
        }

        template<typename... Args>
        void info(char const* fmt, Args const&... args)
        {
            log(Level::info, fmt, args...);
        }

        template<typename... Args>
        void warn(char const* fmt, Args const&... args)
        {
            log(Level::warn, fmt, args...);
        }

        template<typename... Args>
        void error(char const* fmt, Args const&... args)
        {
            log(Level::err, fmt, args...);
        }

        template<typename... Args>
        void critical(char const* fmt, Args const&... args)
        {
            log(Level::critical, fmt, args...);
        }

        [[nodiscard]] std::vector<std::shared_ptr<Sink>> const& sinks() const noexcept
        {
            return m_Sinks;
        }

        [[nodiscard]] std::vector<std::shared_ptr<Sink>>& sinks() noexcept
        {
            return m_Sinks;
        }

    private:
        std::string m_Name;
        std::vector<std::shared_ptr<Sink>> m_Sinks;
        Level level = Level::trace;
    };

    // global logging API
    std::shared_ptr<Logger> defaultLogger() noexcept;
    Logger* defaultLoggerRaw() noexcept;

    template<typename... Args>
    inline void log(Level level, char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->log(level, fmt, args...);
    }

    template<typename... Args>
    inline void trace(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->trace(fmt, args...);
    }

    template<typename... Args>
    inline void debug(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->debug(fmt, args...);
    }

    template<typename... Args>
    void info(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->info(fmt, args...);
    }

    template<typename... Args>
    void warn(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->warn(fmt, args...);
    }

    template<typename... Args>
    void error(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->error(fmt, args...);
    }

    template<typename... Args>
    void critical(char const* fmt, Args const&... args)
    {
        defaultLoggerRaw()->critical(fmt, args...);
    }

    constexpr static size_t c_MaxLogTracebackMessages = 256;

    [[nodiscard]] Level getTracebackLevel();
    void setTracebackLevel(Level);
    [[nodiscard]] SynchronizedValue<CircularBuffer<OwnedLogMessage, c_MaxLogTracebackMessages>>& getTracebackLog();
}
