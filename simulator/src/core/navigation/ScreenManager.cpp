#include "ScreenManager.h"

namespace VOXA
{
    ScreenManager::ScreenManager(ScreenId initialScreen)
        : m_current(initialScreen)
    {
    }

    void ScreenManager::navigateTo(ScreenId screen)
    {
        if (screen == m_current) return;
        m_history.push_back(m_current);
        m_current = screen;
    }

    void ScreenManager::goBack()
    {
        if (m_history.empty()) return;
        m_current = m_history.back();
        m_history.pop_back();
    }

    void ScreenManager::replaceCurrent(ScreenId screen)
    {
        m_current = screen;
    }

    void ScreenManager::resetTo(ScreenId screen)
    {
        m_history.clear();
        m_current = screen;
    }

    ScreenId ScreenManager::current() const
    {
        return m_current;
    }

    bool ScreenManager::canGoBack() const
    {
        return !m_history.empty();
    }

    int ScreenManager::historyDepth() const
    {
        return static_cast<int>(m_history.size());
    }
}
