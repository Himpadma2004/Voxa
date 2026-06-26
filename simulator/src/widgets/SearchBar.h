#pragma once

#include <string>

#include "../core/Widget.h"

namespace VOXA
{
    class SearchBar : public Widget
    {
    public:
        SearchBar(Rect bounds, std::string placeholder);

        void render(Renderer& renderer) const override;

    private:
        std::string m_placeholder;
    };
}
