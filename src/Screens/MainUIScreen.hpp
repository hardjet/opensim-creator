#pragma once

#include "src/Platform/Screen.hpp"

#include <SDL_events.h>

#include <filesystem>

namespace osc
{
    class MainUIScreen final : public Screen {
    public:
        MainUIScreen();
        MainUIScreen(std::filesystem::path);
        MainUIScreen(MainUIScreen const&) = delete;
        MainUIScreen(MainUIScreen&&) noexcept;
        MainUIScreen& operator=(MainUIScreen const&) = delete;
        MainUIScreen& operator=(MainUIScreen&&) noexcept;
        ~MainUIScreen() noexcept override;

        void onMount() override;
        void onUnmount() override;
        void onEvent(SDL_Event const&) override;
        void onTick() override;
        void onDraw() override;

    private:
        class Impl;
        Impl* m_Impl;
    };
}
