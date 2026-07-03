#pragma once

#include <string>

#include "../core/Widget.h"

namespace VOXA
{
    class Button : public Widget
    {
    public:
        Button(Rect bounds, std::string label, bool active);

        void render(Renderer& renderer) const override;

    private:
        std::string m_label;
        bool m_active;
    };
}
