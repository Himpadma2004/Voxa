#ifndef VOXA_MUSICPLAYERSCREEN_H
#define VOXA_MUSICPLAYERSCREEN_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <string>
#include <vector>
#include "../touch/Touch.h"
#include "ScreenCommon.h"

namespace VOXA
{
    struct MusicTrack
    {
        std::string id;
        std::string title;
        std::string artist;
        std::string genre;
        int durationSecs;
        std::string streamUrl;
    };

    enum class MusicTab
    {
        All = 0,
        Hits = 1,
        CloudS3 = 2,
        Search = 3
    };

    class MusicPlayerScreen
    {
    public:
        MusicPlayerScreen();
        ScreenId show(Touch& touch);

    private:
        void fetchLibrary();
        void searchMusic(const std::string& query);
        void playTrack(int index);
        void togglePlayPause();
        void nextTrack();
        void prevTrack();

        std::vector<MusicTrack> m_tracks;
        std::vector<MusicTrack> m_searchResults;
        MusicTab m_currentTab;
        bool m_isSearching;
        int m_currentTrackIndex;
        bool m_isPlaying;
        bool m_showNowPlayingSheet;

        float m_scrollY;
        float m_targetScrollY;
        float m_scrollVelocity;

        float m_dragStartY;
        float m_lastDragY;
        bool m_isDragging;
        bool m_wasTouched;
        uint32_t m_lastTouchSampleMs;

        uint32_t m_trackStartMs;
        uint32_t m_trackDurationMs;
        float m_animTime;

        int m_pressedItemIndex;
        int m_pressedTabIdx;
        int m_pressedSearchChipIdx;
        bool m_isBackPressed;
        bool m_isRefreshPressed;
        bool m_isPlayPressed;
        bool m_isNextPressed;
        bool m_isPrevPressed;
        bool m_isSheetDismissPressed;
    };
}

#endif // VOXA_MUSICPLAYERSCREEN_H
