#pragma once

#include "../core/Widget.h"

namespace VOXA
{
    class ProgressBar : public Widget
    {
    public:
        ProgressBar(Rect bounds, float progress, SDL_Color track, SDL_Color fill);

        void setProgress(float progress);
        void render(Renderer& renderer) const override;

    private:
        float m_progress;
        SDL_Color m_track;
        SDL_Color m_fill;
    };
}
