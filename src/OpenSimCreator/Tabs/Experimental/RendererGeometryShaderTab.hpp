#pragma once

#include <oscar/UI/Tabs/Tab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>

#include <SDL_events.h>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class TabHost; }

namespace osc
{
    class RendererGeometryShaderTab final : public Tab {
    public:
        static CStringView id() noexcept;

        explicit RendererGeometryShaderTab(ParentPtr<TabHost> const&);
        RendererGeometryShaderTab(RendererGeometryShaderTab const&) = delete;
        RendererGeometryShaderTab(RendererGeometryShaderTab&&) noexcept;
        RendererGeometryShaderTab& operator=(RendererGeometryShaderTab const&) = delete;
        RendererGeometryShaderTab& operator=(RendererGeometryShaderTab&&) noexcept;
        ~RendererGeometryShaderTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnMount() final;
        void implOnUnmount() final;
        bool implOnEvent(SDL_Event const&) final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
