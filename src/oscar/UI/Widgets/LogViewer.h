#pragma once

#include <memory>

namespace osc
{
    class LogViewer final {
    public:
        LogViewer();
        LogViewer(const LogViewer&) = delete;
        LogViewer(LogViewer&&) noexcept;
        LogViewer& operator=(const LogViewer&) = delete;
        LogViewer& operator=(LogViewer&&) noexcept;
        ~LogViewer() noexcept;

        // assumes caller handles `ui::begin_panel(panel_name, nullptr, `ui::WindowFlag::MenuBar`)`
        void on_draw();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
