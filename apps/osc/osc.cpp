#include "OpenSimCreator/Screens/MainUIScreen.hpp"
#include "OpenSimCreator/OpenSimCreatorApp.hpp"

#include "oscar/Platform/Config.hpp"
#include "oscar/Platform/Log.hpp"
#include "oscar/Tabs/Tab.hpp"
#include "oscar/Tabs/TabHost.hpp"
#include "oscar/Tabs/TabRegistry.hpp"
#include "oscar/Utils/CStringView.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace
{
    constexpr osc::CStringView c_Usage = "usage: osc [--help] [fd] MODEL.osim\n";

    constexpr osc::CStringView c_Help = R"(OPTIONS
    --help
        Show this help
)";
}

int main(int argc, char* argv[])
{
    std::vector<std::string_view> unnamedArgs;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view const arg{argv[i]};

        if (arg.empty())
        {
            // do nothing (this shouldn't happen)
        }
        else if (arg.front() != '-')
        {
            unnamedArgs.push_back(arg);
            break;
        }
        else if (arg == "--help")
        {
            std::cout << c_Usage << '\n' << c_Help << '\n';
            return EXIT_SUCCESS;
        }
    }

    // init top-level application state
    osc::OpenSimCreatorApp app;

    // init top-level screen (tab host)
    auto screen = std::make_unique<osc::MainUIScreen>();

    // load each unnamed arg as a file in the UI
    for (auto const& unnamedArg : unnamedArgs)
    {
        screen->open(unnamedArg);
    }

    // enter main application loop
    app.show(std::move(screen));

    return EXIT_SUCCESS;
}
