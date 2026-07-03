#pragma once

#include <string>

namespace VOXA
{
    class Renderer;

    class StatusBar
    {
    public:
        StatusBar(std::string title, std::string batteryLabel);

        void render(Renderer& renderer) const;

    private:
        std::string m_title;
        std::string m_batteryLabel;
    };
}
