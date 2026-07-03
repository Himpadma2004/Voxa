#include "ProgressBar.h"

#include <algorithm>

#include "../graphics/Renderer.h"

namespace VOXA
{
    ProgressBar::ProgressBar(Rect bounds, float progress, SDL_Color track, SDL_Color fill)
        : Widget(bounds)
        , m_progress(std::clamp(progress, 0.0f, 1.0f))
        , m_track(track)
        , m_fill(fill)
    {
    }

    void ProgressBar::setProgress(float progress)
    {
        m_progress = std::clamp(progress, 0.0f, 1.0f);
    }

    void ProgressBar::render(Renderer& renderer) const
    {
        renderer.drawProgressBar(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_progress, m_track, m_fill);
    }
}
