#pragma once

#include <vector>

#include "../Screen.h"   // For ScreenId

namespace VOXA
{
    /// Stack-based screen navigation manager.
    ///
    /// Maintains a history stack so that goBack() always returns to the
    /// previous screen.  This is the single source of truth for which
    /// screen is currently displayed.
    class ScreenManager
    {
    public:
        explicit ScreenManager(ScreenId initialScreen);

        // ---------------------------------------------------------------
        // Navigation
        // ---------------------------------------------------------------

        /// Navigate to a screen and push the current screen onto the history stack.
        void navigateTo(ScreenId screen);

        /// Pop the current screen and return to the previous one.
        /// Does nothing if there is no history.
        void goBack();

        /// Replace the current screen without pushing history.
        /// Useful for redirects (e.g. boot → home).
        void replaceCurrent(ScreenId screen);

        /// Clear the entire history stack and set a new root screen.
        void resetTo(ScreenId screen);

        // ---------------------------------------------------------------
        // Queries
        // ---------------------------------------------------------------

        /// Returns the currently active screen id.
        [[nodiscard]] ScreenId current() const;

        /// Returns true if there is a previous screen to go back to.
        [[nodiscard]] bool canGoBack() const;

        /// Returns the number of screens in the history stack
        /// (not counting the current screen).
        [[nodiscard]] int historyDepth() const;

    private:
        ScreenId              m_current;
        std::vector<ScreenId> m_history;  ///< Previous screens, oldest at index 0
    };
}
