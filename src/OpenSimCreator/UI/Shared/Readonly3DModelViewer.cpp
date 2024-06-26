#include "Readonly3DModelViewer.h"

#include <OpenSimCreator/Graphics/CachedModelRenderer.h>
#include <OpenSimCreator/Graphics/ModelRendererParams.h>
#include <OpenSimCreator/UI/Shared/BasicWidgets.h>

#include <oscar/Graphics/Scene/SceneCache.h>
#include <oscar/Graphics/Scene/SceneCollision.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Platform/App.h>
#include <oscar/UI/IconCache.h>
#include <oscar/UI/ImGuiHelpers.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/Widgets/GuiRuler.h>
#include <oscar/UI/Widgets/IconWithoutMenu.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace osc;

namespace
{
    std::string GetSettingsKeyPrefixForPanel(std::string_view panelName)
    {
        std::stringstream ss;
        ss << "panels/" << panelName << '/';
        return std::move(ss).str();
    }
}

class osc::Readonly3DModelViewer::Impl final {
public:
    explicit Impl(std::string_view parentPanelName_, Readonly3DModelViewerFlags flags_) :
        m_ParentPanelName{parentPanelName_},
        m_Flags{flags_}
    {
        UpdModelRendererParamsFrom(
            App::config(),
            GetSettingsKeyPrefixForPanel(parentPanelName_),
            m_Params
        );
    }

    bool isLeftClicked() const
    {
        return m_MaybeLastHittest && m_MaybeLastHittest->is_left_click_released_without_dragging;
    }

    bool isRightClicked() const
    {
        return m_MaybeLastHittest && m_MaybeLastHittest->is_right_click_released_without_dragging;
    }

    bool isMousedOver() const
    {
        return m_MaybeLastHittest && m_MaybeLastHittest->is_hovered;
    }

    std::optional<SceneCollision> onDraw(IConstModelStatePair const& rs)
    {
        // if this is the first frame being rendered, auto-focus the scene
        if (!m_MaybeLastHittest)
        {
            m_CachedModelRenderer.autoFocusCamera(
                rs,
                m_Params,
                aspect_ratio(ui::GetContentRegionAvail())
            );
        }

        // inputs: process inputs, if hovering
        if (m_MaybeLastHittest && m_MaybeLastHittest->is_hovered)
        {
            ui::UpdatePolarCameraFromInputs(
                m_Params.camera,
                m_MaybeLastHittest->item_rect,
                m_CachedModelRenderer.bounds()
            );
        }

        // render scene to texture
        m_CachedModelRenderer.onDraw(
            rs,
            m_Params,
            ui::GetContentRegionAvail(),
            App::get().anti_aliasing_level()
        );

        // blit texture as a ui::Image
        ui::Image(
            m_CachedModelRenderer.updRenderTexture(),
            ui::GetContentRegionAvail()
        );

        // update current+retained hittest
        ui::HittestResult const hittest = ui::HittestLastItem();
        m_MaybeLastHittest = hittest;

        // if allowed, hittest the scene
        std::optional<SceneCollision> hittestResult;
        if (!(m_Flags & Readonly3DModelViewerFlags::NoSceneHittest) && hittest.is_hovered && !ui::IsDraggingWithAnyMouseButtonDown())
        {
            hittestResult = m_CachedModelRenderer.getClosestCollision(
                m_Params,
                ui::GetMousePos(),
                hittest.item_rect
            );
        }

        // draw 2D ImGui overlays
        auto renderParamsBefore = m_Params;
        bool const edited = DrawViewerImGuiOverlays(
            m_Params,
            m_CachedModelRenderer.getDrawlist(),
            m_CachedModelRenderer.bounds(),
            hittest.item_rect,
            *m_IconCache,
            [this]() { return drawRulerButton(); }
        );
        if (edited)
        {
            auto const& renderParamsAfter = m_Params;
            SaveModelRendererParamsDifference(
                renderParamsBefore,
                renderParamsAfter,
                GetSettingsKeyPrefixForPanel(m_ParentPanelName),
                App::upd().upd_config()
            );
        }

        // handle ruler and return value
        if (m_Ruler.isMeasuring())
        {
            m_Ruler.onDraw(m_Params.camera, hittest.item_rect, hittestResult);
            return std::nullopt;  // disable hittest while measuring
        }
        else
        {
            return hittestResult;
        }
    }

    std::optional<Rect> getScreenRect() const
    {
        return m_MaybeLastHittest ?
            std::optional<Rect>{m_MaybeLastHittest->item_rect} :
            std::nullopt;
    }

    PolarPerspectiveCamera const& getCamera() const
    {
        return m_Params.camera;
    }

    void setCamera(PolarPerspectiveCamera const& camera)
    {
        m_Params.camera = camera;
    }

private:
    bool drawRulerButton()
    {
        IconWithoutMenu rulerButton
        {
            m_IconCache->getIcon("ruler"),
            "Ruler",
            "Roughly measure something in the scene",
        };

        bool rv = false;
        if (rulerButton.onDraw())
        {
            m_Ruler.toggleMeasuring();
            rv = true;
        }
        return rv;
    }

    // used for saving per-panel data to the application config
    std::string m_ParentPanelName;

    // runtime modification flags
    Readonly3DModelViewerFlags m_Flags = Readonly3DModelViewerFlags::None;

    // rendering-related data
    ModelRendererParams m_Params;
    CachedModelRenderer m_CachedModelRenderer{
        App::singleton<SceneCache>(App::resource_loader()),
    };

    // only available after rendering the first frame
    std::optional<ui::HittestResult> m_MaybeLastHittest;

    // overlay-related data
    std::shared_ptr<IconCache> m_IconCache = App::singleton<IconCache>(
        App::resource_loader().with_prefix("icons/"),
        ui::GetTextLineHeight()/128.0f
    );
    GuiRuler m_Ruler;
};


// public API (PIMPL)

osc::Readonly3DModelViewer::Readonly3DModelViewer(std::string_view parentPanelName_, Readonly3DModelViewerFlags flags_) :
    m_Impl{std::make_unique<Impl>(parentPanelName_, flags_)}
{
}
osc::Readonly3DModelViewer::Readonly3DModelViewer(Readonly3DModelViewer&&) noexcept = default;
osc::Readonly3DModelViewer& osc::Readonly3DModelViewer::operator=(Readonly3DModelViewer&&) noexcept = default;
osc::Readonly3DModelViewer::~Readonly3DModelViewer() noexcept = default;

bool osc::Readonly3DModelViewer::isLeftClicked() const
{
    return m_Impl->isLeftClicked();
}

bool osc::Readonly3DModelViewer::isRightClicked() const
{
    return m_Impl->isRightClicked();
}

bool osc::Readonly3DModelViewer::isMousedOver() const
{
    return m_Impl->isMousedOver();
}

std::optional<SceneCollision> osc::Readonly3DModelViewer::onDraw(IConstModelStatePair const& rs)
{
    return m_Impl->onDraw(rs);
}

std::optional<Rect> osc::Readonly3DModelViewer::getScreenRect() const
{
    return m_Impl->getScreenRect();
}

PolarPerspectiveCamera const& osc::Readonly3DModelViewer::getCamera() const
{
    return m_Impl->getCamera();
}

void osc::Readonly3DModelViewer::setCamera(PolarPerspectiveCamera const& camera)
{
    m_Impl->setCamera(camera);
}
