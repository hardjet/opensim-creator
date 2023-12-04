#include "MIObject.hpp"

#include <oscar/Maths/MathHelpers.hpp>

#include <algorithm>

void osc::mi::MIObject::applyRotation(
    IObjectFinder const& lookup,
    Vec3 const& eulerAngles,
    Vec3 const& rotationCenter)
{
    Transform t = getXForm(lookup);
    ApplyWorldspaceRotation(t, eulerAngles, rotationCenter);
    setXform(lookup, t);
}

bool osc::mi::MIObject::isCrossReferencing(
    UID id,
    CrossrefDirection direction) const
{
    auto const crossRefs = implGetCrossReferences();
    return std::any_of(
        crossRefs.begin(),
        crossRefs.end(),
        [id, direction](CrossrefDescriptor const& desc)
        {
            return desc.getConnecteeID() == id && (desc.getDirection() & direction);
        }
    );
}