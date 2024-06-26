#pragma once

#include <oscar/Maths/Vec2.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/Widgets/IPopup.h>

#include <optional>
#include <string>
#include <string_view>

namespace osc { struct Rect; }

namespace osc
{
    // base class for implementing a standard UI popup (that blocks the whole screen
    // apart from the popup content)
    class StandardPopup : public IPopup {
    protected:
        StandardPopup(const StandardPopup&) = default;
        StandardPopup(StandardPopup&&) noexcept = default;
        StandardPopup& operator=(const StandardPopup&) = default;
        StandardPopup& operator=(StandardPopup&&) noexcept = default;
    public:
        virtual ~StandardPopup() noexcept = default;

        explicit StandardPopup(
            std::string_view popupName
        );

        StandardPopup(
            std::string_view popupName,
            Vec2 dimensions,
            ImGuiWindowFlags
        );

    protected:
        bool isPopupOpenedThisFrame() const;
        void request_close();
        bool isModal() const;
        void setModal(bool);
        void setRect(const Rect&);
        void set_dimensions(Vec2);
        void set_position(std::optional<Vec2>);

    private:
        // this standard implementation supplies these
        bool implIsOpen() const final;
        void implOpen() final;
        void implClose() final;
        bool implBeginPopup() final;
        void implOnDraw() final;
        void implEndPopup() final;

        // derivers can/must provide these
        virtual void implBeforeImguiBeginPopup() {}
        virtual void implAfterImguiBeginPopup() {}
        virtual void implDrawContent() = 0;
        virtual void implOnClose() {}

        std::string m_PopupName;
        Vec2i m_Dimensions;
        std::optional<Vec2i> m_MaybePosition;
        ImGuiWindowFlags m_PopupFlags;
        bool m_ShouldOpen;
        bool m_ShouldClose;
        bool m_JustOpened;
        bool m_IsOpen;
        bool m_IsModal;
    };
}
