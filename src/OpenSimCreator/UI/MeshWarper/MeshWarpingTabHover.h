#pragma once

#include <OpenSimCreator/Documents/MeshWarper/TPSDocumentElementID.h>
#include <OpenSimCreator/Documents/MeshWarper/TPSDocumentInputIdentifier.h>

#include <oscar/Maths/Vec3.h>

#include <optional>

namespace osc
{
    // a mouse hovertest result
    class MeshWarpingTabHover final {
    public:
        explicit MeshWarpingTabHover(
            TPSDocumentInputIdentifier input_,
            const Vec3& worldspaceLocation_) :
            m_Input{input_},
            m_WorldspaceLocation{worldspaceLocation_}
        {}

        explicit MeshWarpingTabHover(
            TPSDocumentElementID sceneElementID_,
            const Vec3& worldspaceLocation_) :

            m_MaybeSceneElementID{std::move(sceneElementID_)},
            m_Input{m_MaybeSceneElementID->input},
            m_WorldspaceLocation{worldspaceLocation_}
        {
        }

        TPSDocumentInputIdentifier getInput() const
        {
            return m_Input;
        }

        std::optional<TPSDocumentElementID> getSceneElementID() const
        {
            return m_MaybeSceneElementID;
        }

        bool isHoveringASceneElement() const
        {
            return m_MaybeSceneElementID.has_value();
        }

        bool isHovering(const TPSDocumentElementID& el) const
        {
            return m_MaybeSceneElementID == el;
        }

        const Vec3& getWorldspaceLocation() const
        {
            return m_WorldspaceLocation;
        }

    private:
        std::optional<TPSDocumentElementID> m_MaybeSceneElementID;
        TPSDocumentInputIdentifier m_Input;
        Vec3 m_WorldspaceLocation;
    };
}
