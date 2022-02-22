#include "OpenSimHelpers.hpp"

#include "src/Utils/Algorithms.hpp"

#include <OpenSim/Simulation/Model/Model.h>

std::vector<OpenSim::AbstractSocket const*> osc::GetAllSockets(OpenSim::Component& c)
{
    std::vector<OpenSim::AbstractSocket const*> rv;

    for (std::string const& name : c.getSocketNames())
    {
        OpenSim::AbstractSocket const& sock = c.getSocket(name);
        rv.push_back(&sock);
    }

    return rv;
}

std::vector<OpenSim::AbstractSocket const*> osc::GetSocketsWithTypeName(OpenSim::Component& c, std::string_view typeName)
{
    std::vector<OpenSim::AbstractSocket const*> rv;

    for (std::string const& name : c.getSocketNames())
    {
        OpenSim::AbstractSocket const& sock = c.getSocket(name);
        if (sock.getConnecteeTypeName() == typeName)
        {
            rv.push_back(&sock);
        }
    }

    return rv;
}

std::vector<OpenSim::AbstractSocket const*> osc::GetPhysicalFrameSockets(OpenSim::Component& c)
{
    return GetSocketsWithTypeName(c, "PhysicalFrame");
}

OpenSim::Component const* osc::FindComponent(OpenSim::Component const& c, OpenSim::ComponentPath const& cp)
{
    static OpenSim::ComponentPath const g_EmptyPath;

    if (cp == g_EmptyPath)
    {
        return nullptr;
    }

    try
    {
        return &c.getComponent(cp);
    }
    catch (OpenSim::Exception const&)
    {
        return nullptr;
    }
}

OpenSim::Component* osc::FindComponentMut(OpenSim::Component& c, OpenSim::ComponentPath const& cp)
{
    return const_cast<OpenSim::Component*>(FindComponent(c, cp));
}

bool osc::ContainsComponent(OpenSim::Component const& root, OpenSim::ComponentPath const& cp)
{
    return FindComponent(root, cp);
}

bool osc::HasInputFileName(OpenSim::Model const& m)
{
    std::string const& name = m.getInputFileName();
    return !name.empty() && name != "Unassigned";
}

std::filesystem::path osc::TryFindInputFile(OpenSim::Model const& m)
{
    if (!HasInputFileName(m))
    {
        return {};
    }

    std::filesystem::path p{m.getInputFileName()};

    if (!std::filesystem::exists(p))
    {
        return {};
    }

    return p;
}

bool osc::ShouldShowInUI(OpenSim::Component const& c)
{
    if (Is<OpenSim::PathWrapPoint>(c))
    {
        return false;
    }
    else if (Is<OpenSim::Station>(c) && c.hasOwner() && DerivesFrom<OpenSim::PathPoint>(c.getOwner()))
    {
        return false;
    }
    else
    {
        return true;
    }
}
