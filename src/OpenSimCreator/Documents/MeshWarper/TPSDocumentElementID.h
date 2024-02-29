#pragma once

#include <OpenSimCreator/Documents/MeshWarper/TPSDocumentElementType.h>
#include <OpenSimCreator/Documents/MeshWarper/TPSDocumentInputIdentifier.h>

#include <oscar/Utils/HashHelpers.h>
#include <oscar/Utils/UID.h>

#include <cstddef>
#include <functional>

namespace osc
{
    // an associative identifier that points to a specific part of a TPS document
    //
    // (handy for selection logic etc.)
    struct TPSDocumentElementID final {

        friend bool operator==(TPSDocumentElementID const&, TPSDocumentElementID const&) = default;

        UID uid;
        TPSDocumentElementType type;
        TPSDocumentInputIdentifier input = TPSDocumentInputIdentifier::Source;
    };
}

template<>
struct std::hash<osc::TPSDocumentElementID> final {
    size_t operator()(osc::TPSDocumentElementID const& el) const
    {
        return osc::HashOf(el.uid, el.type, el.input);
    }
};