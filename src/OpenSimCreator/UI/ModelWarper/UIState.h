#pragma once

#include <OpenSimCreator/Documents/Model/IConstModelStatePair.h>
#include <OpenSimCreator/Documents/ModelWarper/CachedModelWarper.h>
#include <OpenSimCreator/Documents/ModelWarper/ModelWarpDocument.h>
#include <OpenSimCreator/Documents/ModelWarper/ValidationCheckResult.h>
#include <OpenSimCreator/Documents/ModelWarper/ValidationCheckState.h>
#include <OpenSimCreator/Documents/ModelWarper/WarpDetail.h>

#include <oscar/Maths/PolarPerspectiveCamera.h>
#include <oscar/UI/Tabs/ITabHost.h>
#include <oscar/Utils/ParentPtr.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace OpenSim { class Mesh; }
namespace OpenSim { class Model; }
namespace OpenSim { class PhysicalOffsetFrame; }
namespace osc { template<typename> class ParentPtr; }
namespace osc { class ITabHost; }

namespace osc::mow
{
    class UIState final {
    public:
        explicit UIState(ParentPtr<ITabHost> const& tabHost) :
            m_TabHost{tabHost}
        {}

        const OpenSim::Model& model() const { return m_Document->model(); }
        const IConstModelStatePair& modelstate() const { return m_Document->modelstate(); }

        std::vector<WarpDetail> details(const OpenSim::Mesh& mesh) const { return m_Document->details(mesh); }
        std::vector<ValidationCheckResult> validate(const OpenSim::Mesh& mesh) const { return m_Document->validate(mesh); }
        ValidationCheckState state(const OpenSim::Mesh& mesh) const { return m_Document->state(mesh); }

        std::vector<WarpDetail> details(const OpenSim::PhysicalOffsetFrame& pof) const { return m_Document->details(pof); }
        std::vector<ValidationCheckResult> validate(const OpenSim::PhysicalOffsetFrame& pof) const { return m_Document->validate(pof); }
        ValidationCheckState state(const OpenSim::PhysicalOffsetFrame& pof) const { return m_Document->state(pof); }

        float getWarpBlendingFactor() const { return m_Document->getWarpBlendingFactor(); }
        void setWarpBlendingFactor(float v) { m_Document->setWarpBlendingFactor(v); }

        bool isCameraLinked() const { return m_LinkCameras; }
        void setCameraLinked(bool v) { m_LinkCameras = v; }
        bool isOnlyCameraRotationLinked() const { return m_OnlyLinkRotation; }
        void setOnlyCameraRotationLinked(bool v) { m_OnlyLinkRotation = v; }
        const PolarPerspectiveCamera& getLinkedCamera() const { return m_LinkedCamera; }
        void setLinkedCamera(const PolarPerspectiveCamera& camera) { m_LinkedCamera = camera; }

        ValidationCheckState state() const { return m_Document->state(); }
        bool canWarpModel() const { return state() != ValidationCheckState::Error; }
        std::shared_ptr<IConstModelStatePair const> tryGetWarpedModel()
        {
            if (canWarpModel()) {
                return m_ModelWarper.warp(*m_Document);
            }
            else {
                return nullptr;
            }
        }

        void actionOpenOsimOrPromptUser(
            std::optional<std::filesystem::path> maybeOsimPath = std::nullopt
        );

        void actionWarpModelAndOpenInModelEditor();
    private:
        ParentPtr<ITabHost> m_TabHost;
        std::shared_ptr<ModelWarpDocument> m_Document = std::make_shared<ModelWarpDocument>();
        CachedModelWarper m_ModelWarper;

        bool m_LinkCameras = true;
        bool m_OnlyLinkRotation = false;
        PolarPerspectiveCamera m_LinkedCamera;
    };
}
