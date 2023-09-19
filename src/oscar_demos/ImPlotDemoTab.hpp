#pragma once

#include <oscar/UI/Tabs/Tab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class TabHost; }

namespace osc
{
    class ImPlotDemoTab final : public Tab {
    public:
        static CStringView id() noexcept;

        explicit ImPlotDemoTab(ParentPtr<TabHost> const&);
        ImPlotDemoTab(ImPlotDemoTab const&) = delete;
        ImPlotDemoTab(ImPlotDemoTab&&) noexcept;
        ImPlotDemoTab& operator=(ImPlotDemoTab const&) = delete;
        ImPlotDemoTab& operator=(ImPlotDemoTab&&) noexcept;
        ~ImPlotDemoTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnMount() final;
        void implOnUnmount() final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
