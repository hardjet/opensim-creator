#pragma once

#include <oscar/UI/Tabs/ITab.h>
#include <oscar/Utils/CStringView.h>
#include <oscar/Utils/UID.h>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { struct Screenshot; }
namespace osc { class ITabHost; }

namespace osc
{
    class ScreenshotTab final : public ITab {
    public:
        ScreenshotTab(const ParentPtr<ITabHost>&, Screenshot&&);
        ScreenshotTab(const ScreenshotTab&) = delete;
        ScreenshotTab(ScreenshotTab&&) noexcept;
        ScreenshotTab& operator=(const ScreenshotTab&) = delete;
        ScreenshotTab& operator=(ScreenshotTab&&) noexcept;
        ~ScreenshotTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnDrawMainMenu() final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
