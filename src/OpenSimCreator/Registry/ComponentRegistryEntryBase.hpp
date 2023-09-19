#pragma once

#include <OpenSim/Common/Component.h>
#include <oscar/Utils/CStringView.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace osc
{
    class ComponentRegistryEntryBase {
    public:
        ComponentRegistryEntryBase(
            std::string_view name_,
            std::string_view description_,
            std::shared_ptr<OpenSim::Component const>
        );

        CStringView name() const noexcept
        {
            return m_Name;
        }

        CStringView description() const noexcept
        {
            return m_Description;
        }

        OpenSim::Component const& prototype() const noexcept
        {
            return *m_Prototype;
        }

        std::unique_ptr<OpenSim::Component> instantiate() const;
    private:
        std::string m_Name;
        std::string m_Description;
        std::shared_ptr<OpenSim::Component const> m_Prototype;
    };
}
