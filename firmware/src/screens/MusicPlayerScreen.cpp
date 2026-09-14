#include "MusicPlayerScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "Transition.h"
#include "../services/ApiClient.h"
#include "../audio/AudioManager.h"
#include "../services/PowerManager.h"
#include "../services/ButtonService.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <algorithm>

namespace VOXA
{
    static const char* SEARCH_CHIPS[] = {
        "Coldplay", "Adele", "Ed Sheeran", "The Weeknd",
        "Imagine Dragons", "Arijit Singh", "Lo-Fi Beats", "Taylor Swift"
    };
    static const size_t NUM_SEARCH_CHIPS = 8;

    MusicPlayerScreen::MusicPlayerScreen()
        : m_currentTab(MusicTab::All)
        , m_isSearching(false)
        , m_currentTrackIndex(-1)
        , m_isPlaying(false)
        , m_showNowPlayingSheet(false)
        , m_scrollY(0.0f)
        , m_targetScrollY(0.0f)
        , m_scrollVelocity(0.0f)
        , m_dragStartY(0.0f)
        , m_lastDragY(0.0f)
        , m_isDragging(false)
        , m_wasTouched(false)
        , m_lastTouchSampleMs(0)
        , m_trackStartMs(0)
        , m_trackDurationMs(240000)
        , m_animTime(0.0f)
        , m_pressedItemIndex(-1)
        , m_pressedTabIdx(-1)
        , m_pressedSearchChipIdx(-1)
        , m_isBackPressed(false)
        , m_isRefreshPressed(false)
        , m_isPlayPressed(false)
        , m_isNextPressed(false)
        , m_isPrevPressed(false)
        , m_isSheetDismissPressed(false)
    {
        // Built-in curated full-length song library
        m_tracks = {
            { "s3_sunrise",         "Sunrise Melody",   "Voxa Cloud S3",  "Acoustic Chill",   180, "/api/music/stream/s3_sunrise" },
            { "s3_reminder",        "Reminder Alert",   "Voxa Cloud S3",  "Ambient Sound",    60,  "/api/music/stream/s3_reminder" },
            { "yt_yKNxeF4KMsY",     "Yellow",           "Coldplay",       "Alternative Rock", 273, "/api/music/stream/yt_yKNxeF4KMsY" },
            { "yt_4NRXx6U8ABQ",     "Blinding Lights",  "The Weeknd",     "Synthwave Pop",    200, "/api/music/stream/yt_4NRXx6U8ABQ" },
            { "yt_JGwWNGJdvx8",     "Shape of You",     "Ed Sheeran",     "Pop",              233, "/api/music/stream/yt_JGwWNGJdvx8" },
            { "yt_7wtfhZwyrcc",     "Believer",         "Imagine Dragons","Alt Rock",         217, "/api/music/stream/yt_7wtfhZwyrcc" },
            { "yt_H5v3kku4y6Q",     "As It Was",        "Harry Styles",   "Pop",              167, "/api/music/stream/yt_H5v3kku4y6Q" },
            { "yt_hT_nvWreIhg",     "Counting Stars",   "OneRepublic",    "Pop Rock",         257, "/api/music/stream/yt_hT_nvWreIhg" },
            { "yt_kTJczUoc26U",     "Stay",             "Justin Bieber",  "Pop",              141, "/api/music/stream/yt_kTJczUoc26U" },
            { "yt_TUVcZfQe-Kw",     "Levitating",       "Dua Lipa",       "Dance Pop",        203, "/api/music/stream/yt_TUVcZfQe-Kw" }
        };
    }

    void MusicPlayerScreen::fetchLibrary()
    {
        if (!WiFi.isConnected()) return;

        xTaskCreate(
            [](void* param) {
                MusicPlayerScreen* self = static_cast<MusicPlayerScreen*>(param);
                HTTPClient http;
                std::string url = apiClient.getBaseUrl() + "/api/music/library";
                http.begin(url.c_str());
                http.setTimeout(5000);

                int code = http.GET();
                if (code == HTTP_CODE_OK)
                {
                    String payload = http.getString();
                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, payload);
                    if (!err && doc["success"].as<bool>())
                    {
                        JsonArray arr = doc["tracks"].as<JsonArray>();
                        if (!arr.isNull() && arr.size() > 0)
                        {
                            std::vector<MusicTrack> fetched;
                            for (JsonObject obj : arr)
                            {
                                MusicTrack t;
                                t.id = obj["id"].as<std::string>();
                                t.title = obj["title"].as<std::string>();
                                t.artist = obj["artist"].as<std::string>();
                                t.genre = obj["genre"] | "Full Music";
                                t.durationSecs = obj["duration"] | 210;
                                t.streamUrl = obj["streamUrl"].as<std::string>();
                                fetched.push_back(t);
                            }
                            if (!fetched.empty())
                            {
                                self->m_tracks = fetched;
                                Serial.printf("[VoxaMusic] Library updated: %d full songs\n", (int)fetched.size());
                            }
                        }
                    }
                }
                http.end();
                vTaskDelete(NULL);
            },
            "MusicFetchTask",
            6144,
            this,
            1,
            NULL
        );
    }

    void MusicPlayerScreen::searchMusic(const std::string& query)
    {
        if (!WiFi.isConnected() || query.empty()) return;

        m_isSearching = true;

        struct SearchParam {
            MusicPlayerScreen* self;
            std::string q;
        };
        SearchParam* p = new SearchParam{this, query};

        xTaskCreate(
            [](void* param) {
                SearchParam* p = static_cast<SearchParam*>(param);
                MusicPlayerScreen* self = p->self;
                HTTPClient http;

                // URL encode query
                String encodedQ = "";
                for (char c : p->q)
                {
                    if (c == ' ') encodedQ += "+";
                    else if (isalnum(c) || c == '-' || c == '_' || c == '.') encodedQ += c;
                    else {
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                        encodedQ += buf;
                    }
                }

                std::string url = apiClient.getBaseUrl() + "/api/music/search?q=" + encodedQ.c_str();
                http.begin(url.c_str());
                http.setTimeout(12000);

                int code = http.GET();
                if (code == HTTP_CODE_OK)
                {
                    String payload = http.getString();
                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, payload);
                    if (!err && doc["success"].as<bool>())
                    {
                        JsonArray arr = doc["tracks"].as<JsonArray>();
                        if (!arr.isNull() && arr.size() > 0)
                        {
                            std::vector<MusicTrack> fetched;
                            for (JsonObject obj : arr)
                            {
                                MusicTrack t;
                                t.id = obj["id"].as<std::string>();
                                t.title = obj["title"].as<std::string>();
                                t.artist = obj["artist"].as<std::string>();
                                t.genre = obj["genre"] | "Full Song";
                                t.durationSecs = obj["duration"] | 210;
                                t.streamUrl = obj["streamUrl"].as<std::string>();
                                fetched.push_back(t);
                            }
                            if (!fetched.empty())
                            {
                                self->m_searchResults = fetched;
                                Serial.printf("[VoxaMusic] Search for '%s' returned %d songs\n", p->q.c_str(), (int)fetched.size());
                            }
                        }
                    }
                }
                http.end();
                self->m_isSearching = false;
                delete p;
                vTaskDelete(NULL);
            },
            "MusicSearchTask",
            8192,
            p,
            1,
            NULL
        );
    }

    void MusicPlayerScreen::playTrack(int index)
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (index < 0 || index >= (int)activeList.size()) return;

        m_currentTrackIndex = index;
        const auto& track = activeList[index];

        AudioManager::instance().playTapSoundAsync();

        std::string fullUrl = track.streamUrl;
        if (fullUrl.rfind("http", 0) != 0)
        {
            fullUrl = apiClient.getBaseUrl() + fullUrl;
        }

        Serial.printf("[VoxaMusic] Playing full track: '%s' (%ds) from %s\n", 
                      track.title.c_str(), track.durationSecs, fullUrl.c_str());

        AudioManager::instance().playUrlAsync(fullUrl);

        m_isPlaying = true;
        m_trackStartMs = millis();
        m_trackDurationMs = track.durationSecs * 1000;
        m_showNowPlayingSheet = true;
    }

    void MusicPlayerScreen::togglePlayPause()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (m_currentTrackIndex < 0)
        {
            if (!activeList.empty()) playTrack(0);
            return;
        }

        if (m_isPlaying)
        {
            AudioManager::instance().stop();
            m_isPlaying = false;
        }
        else
        {
            playTrack(m_currentTrackIndex);
        }
    }

    void MusicPlayerScreen::nextTrack()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (activeList.empty()) return;
        int nextIdx = (m_currentTrackIndex + 1) % (int)activeList.size();
        playTrack(nextIdx);
    }

    void MusicPlayerScreen::prevTrack()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (activeList.empty()) return;
        int prevIdx = (m_currentTrackIndex - 1 + (int)activeList.size()) % (int)activeList.size();
        playTrack(prevIdx);
    }

    ScreenId MusicPlayerScreen::show(Touch& touch)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        bool useSprite = canvas.createSprite(w, h);
        if (useSprite) canvas.fillScreen(0);
        LovyanGFX& target = useSprite ? (LovyanGFX&)canvas : (LovyanGFX&)Display::lcd;

        // Fetch fresh S3 & online tracks in background
        fetchLibrary();

        ScreenId targetScreen = ScreenId::Music;
        uint32_t lastMs = millis();
        int entryFrame = 0;

        while (targetScreen == ScreenId::Music)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;
            m_animTime += deltaSecs;

            PowerManager::instance().tick();

            if (ButtonService::isDirectRecordRequested())
            {
                targetScreen = ScreenId::Record;
                break;
            }

            // Determine active track list based on filter tab
            std::vector<MusicTrack> displayedTracks;
            if (m_currentTab == MusicTab::Search)
            {
                displayedTracks = m_searchResults;
            }
            else if (m_currentTab == MusicTab::CloudS3)
            {
                for (const auto& t : m_tracks)
                {
                    if (t.id.rfind("s3_", 0) == 0) displayedTracks.push_back(t);
                }
            }
            else if (m_currentTab == MusicTab::Hits)
            {
                for (const auto& t : m_tracks)
                {
                    if (t.id.rfind("s3_", 0) != 0) displayedTracks.push_back(t);
                }
            }
            else
            {
                displayedTracks = m_tracks;
            }

            // Touch Processing
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float extraSearchChipsH = (m_currentTab == MusicTab::Search) ? 75.0f : 0.0f;
            float contentHeight = 100.0f + extraSearchChipsH + displayedTracks.size() * 52.0f + 60.0f;
            float visibleHeight = h - 96.0f - (m_currentTrackIndex >= 0 ? 54.0f : 0.0f);
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            if (touched)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    m_dragStartY = ty;
                    m_lastDragY = ty;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    if (!m_showNowPlayingSheet)
                    {
                        // 1. Back button
                        if (std::sqrt((tx - 22.0f)*(tx - 22.0f) + (ty - 42.0f)*(ty - 42.0f)) <= 22.0f)
                        {
                            m_isBackPressed = true;
                        }
                        // 2. Refresh library button
                        else if (std::sqrt((tx - (w - 22.0f))*(tx - (w - 22.0f)) + (ty - 42.0f)*(ty - 42.0f)) <= 22.0f)
                        {
                            m_isRefreshPressed = true;
                        }
                        // 3. Tab Bar buttons (y: 62..86)
                        else if (ty >= 58.0f && ty <= 88.0f)
                        {
                            float tabW = (w - 16.0f) / 4.0f;
                            int tabIdx = (int)((tx - 8.0f) / tabW);
                            if (tabIdx >= 0 && tabIdx < 4)
                            {
                                m_pressedTabIdx = tabIdx;
                            }
                        }
                        // 4. Search Suggestion Chips (if on Search tab)
                        else if (m_currentTab == MusicTab::Search && ty >= 92.0f && ty <= 165.0f && m_scrollY < 30.0f)
                        {
                            float chipW = (w - 24.0f) / 2.0f;
                            for (size_t c = 0; c < NUM_SEARCH_CHIPS; ++c)
                            {
                                float cx = 8.0f + (c % 2) * (chipW + 8.0f);
                                float cy = 92.0f + (c / 2) * 32.0f - m_scrollY;
                                if (tx >= cx && tx <= cx + chipW && ty >= cy && ty <= cy + 28.0f)
                                {
                                    m_pressedSearchChipIdx = (int)c;
                                    break;
                                }
                            }
                        }
                        // 5. Mini Player pill bar at bottom
                        else if (m_currentTrackIndex >= 0 && ty >= h - 56.0f)
                        {
                            if (tx >= w - 46.0f)
                            {
                                togglePlayPause();
                            }
                            else
                            {
                                m_showNowPlayingSheet = true;
                            }
                        }
                        // 6. Track List items
                        else if (ty >= 92.0f && ty <= (h - (m_currentTrackIndex >= 0 ? 54.0f : 10.0f)))
                        {
                            float leftX = w * 0.04f;
                            float cardW = w * 0.92f;
                            float startListY = 94.0f + extraSearchChipsH;
                            for (size_t i = 0; i < displayedTracks.size(); ++i)
                            {
                                float itemY = startListY + i * 52.0f - m_scrollY;
                                if (tx >= leftX && tx <= (leftX + cardW) && ty >= itemY && ty <= (itemY + 48.0f))
                                {
                                    m_pressedItemIndex = (int)i;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        // In Now Playing Sheet
                        if (ty <= 55.0f)
                        {
                            m_isSheetDismissPressed = true;
                        }
                        // Play/Pause button
                        else if (std::sqrt((tx - w * 0.5f)*(tx - w * 0.5f) + (ty - 262.0f)*(ty - 262.0f)) <= 28.0f)
                        {
                            m_isPlayPressed = true;
                        }
                        // Prev button
                        else if (std::sqrt((tx - 44.0f)*(tx - 44.0f) + (ty - 262.0f)*(ty - 262.0f)) <= 22.0f)
                        {
                            m_isPrevPressed = true;
                        }
                        // Next button
                        else if (std::sqrt((tx - (w - 44.0f))*(tx - (w - 44.0f)) + (ty - 262.0f)*(ty - 262.0f)) <= 22.0f)
                        {
                            m_isNextPressed = true;
                        }
                    }
                }
                else
                {
                    // Touch Move / Drag
                    float dy = ty - m_dragStartY;
                    if (!m_showNowPlayingSheet && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_pressedItemIndex = -1;
                        m_pressedTabIdx = -1;
                        m_pressedSearchChipIdx = -1;
                        m_isBackPressed = false;
                        m_isRefreshPressed = false;
                    }

                    if (m_isDragging)
                    {
                        float dragDeltaY = ty - m_lastDragY;
                        m_targetScrollY -= dragDeltaY;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dragDeltaY / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                    m_lastDragY = ty;
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    if (!m_isDragging)
                    {
                        if (!m_showNowPlayingSheet)
                        {
                            if (m_isBackPressed)
                            {
                                targetScreen = ScreenId::Home;
                            }
                            else if (m_isRefreshPressed)
                            {
                                fetchLibrary();
                            }
                            else if (m_pressedTabIdx >= 0)
                            {
                                m_currentTab = static_cast<MusicTab>(m_pressedTabIdx);
                                m_scrollY = 0;
                                m_targetScrollY = 0;
                                AudioManager::instance().playTapSoundAsync();
                            }
                            else if (m_pressedSearchChipIdx >= 0 && m_pressedSearchChipIdx < (int)NUM_SEARCH_CHIPS)
                            {
                                AudioManager::instance().playTapSoundAsync();
                                searchMusic(SEARCH_CHIPS[m_pressedSearchChipIdx]);
                            }
                            else if (m_pressedItemIndex >= 0)
                            {
                                playTrack(m_pressedItemIndex);
                            }
                        }
                        else
                        {
                            if (m_isSheetDismissPressed)
                            {
                                m_showNowPlayingSheet = false;
                            }
                            else if (m_isPlayPressed)
                            {
                                togglePlayPause();
                            }
                            else if (m_isPrevPressed)
                            {
                                prevTrack();
                            }
                            else if (m_isNextPressed)
                            {
                                nextTrack();
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;
                    m_pressedTabIdx = -1;
                    m_pressedSearchChipIdx = -1;
                    m_isPlayPressed = false;
                    m_isPrevPressed = false;
                    m_isNextPressed = false;
                    m_isSheetDismissPressed = false;
                    m_pressedItemIndex = -1;
                    m_isDragging = false;
                }
            }

            // Scroll Physics
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f) m_scrollVelocity = 0.0f;
            }
            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;

            // ── RENDERING ────────────────────────────────────────────────────
            ScreenCommon::renderSurface(target, w, h);

            if (!m_showNowPlayingSheet)
            {
                // 1. Navigation Header
                ScreenCommon::renderHeader(target, "Voxa Music", true, true, Icon::Reset, w, h);

                // Back Button
                uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface();
                uint16_t backCol = m_isBackPressed ? 0xFFFF : VoxaTheme::getTextPrimary();
                ScreenCommon::renderCircularButton(target, 22.0f, 42.0f, Icon::Back, backFill, backCol, w, h);

                // Refresh Button
                uint16_t refFill = m_isRefreshPressed ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface();
                uint16_t refCol = m_isRefreshPressed ? 0xFFFF : VoxaTheme::getTextPrimary();
                ScreenCommon::renderCircularButton(target, w - 22.0f, 42.0f, Icon::Reset, refFill, refCol, w, h);

                // 2. Category Tab Filter Pills (All / Hits / Cloud S3 / Search)
                float tabY = 62.0f;
                float tabH = 26.0f;
                float tabW = (w - 16.0f) / 4.0f;
                const char* tabNames[] = { "All", "Hits", "Cloud", "Search" };

                for (int t = 0; t < 4; ++t)
                {
                    float txPos = 8.0f + t * tabW;
                    bool isSelected = (static_cast<int>(m_currentTab) == t);
                    uint16_t tFill = isSelected ? VoxaTheme::getSystemPurple() : VoxaTheme::getGlassSurface();
                    uint16_t tText = isSelected ? 0xFFFF : VoxaTheme::getTextSecondary();

                    target.fillRoundRect((int)txPos, (int)tabY, (int)(tabW - 4.0f), (int)tabH, 8, tFill);
                    target.drawRoundRect((int)txPos, (int)tabY, (int)(tabW - 4.0f), (int)tabH, 8, VoxaTheme::getGlassBorder());

                    target.setFont(&fonts::Font0);
                    target.setTextDatum(textdatum_t::middle_center);
                    target.setTextColor(tText);
                    target.drawString(tabNames[t], txPos + (tabW - 4.0f) * 0.5f, tabY + tabH * 0.5f);
                }

                // 3. Track List & Search Chips
                float leftX = w * 0.04f;
                float cardW = w * 0.92f;
                target.setClipRect(0, 92, w, h - 92 - (m_currentTrackIndex >= 0 ? 52 : 0));

                // If on Search Tab, draw Quick Search Chips
                if (m_currentTab == MusicTab::Search)
                {
                    float chipW = (w - 24.0f) / 2.0f;
                    for (size_t c = 0; c < NUM_SEARCH_CHIPS; ++c)
                    {
                        float cx = 8.0f + (c % 2) * (chipW + 8.0f);
                        float cy = 94.0f + (c / 2) * 32.0f - m_scrollY;
                        if (cy + 28.0f < 92.0f || cy > h) continue;

                        bool isP = (m_pressedSearchChipIdx == (int)c);
                        target.fillRoundRect((int)cx, (int)cy, (int)chipW, 28, 8, isP ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface());
                        target.drawRoundRect((int)cx, (int)cy, (int)chipW, 28, 8, VoxaTheme::getGlassBorder());

                        target.setFont(&fonts::Font0);
                        target.setTextDatum(textdatum_t::middle_center);
                        target.setTextColor(isP ? 0xFFFF : VoxaTheme::getTextPrimary());
                        target.drawString(SEARCH_CHIPS[c], cx + chipW * 0.5f, cy + 14.0f);
                    }

                    if (m_isSearching)
                    {
                        target.setFont(&fonts::FreeSans9pt7b);
                        target.setTextDatum(textdatum_t::middle_center);
                        target.setTextColor(VoxaTheme::getSystemPurple());
                        target.drawString("Searching full music catalog...", w * 0.5f, 240.0f - m_scrollY);
                    }
                }

                // Render song list cards
                float startListY = 94.0f + extraSearchChipsH;
                for (size_t i = 0; i < displayedTracks.size(); ++i)
                {
                    float itemY = startListY + i * 52.0f - m_scrollY;
                    if (itemY + 46.0f < 92.0f || itemY > h) continue;

                    bool isPressed = (m_pressedItemIndex == (int)i);
                    bool isCur = (m_currentTrackIndex == (int)i);

                    ScreenCommon::drawGlassCard(target, leftX, itemY, cardW, 46.0f, 12.0f, 
                                                isPressed, isCur ? VoxaTheme::getSystemPurple() : 0);

                    float cy = itemY + 23.0f;
                    float iconX = leftX + 8.0f;
                    float iconY = itemY + 8.0f;

                    // Squircle Icon Badge
                    uint16_t badgeCol = (displayedTracks[i].id.rfind("s3_", 0) == 0) 
                        ? VoxaTheme::getSystemBlue() 
                        : VoxaTheme::getSystemPurple();

                    target.fillRoundRect((int)iconX, (int)iconY, 30, 30, 8, badgeCol);
                    target.drawFastHLine((int)iconX + 6, (int)iconY + 1, 18, 0xFFFF);
                    ScreenCommon::drawIcon(target, isCur ? Icon::Play : Icon::Note, iconX + 5.0f, iconY + 5.0f, 20.0f, 0xFFFF);

                    // Track Title
                    target.setFont(&fonts::FreeSansBold9pt7b);
                    target.setTextDatum(textdatum_t::middle_left);
                    target.setTextColor(isPressed ? 0xFFFF : (isCur ? VoxaTheme::getSystemPurple() : VoxaTheme::getTextPrimary()));
                    std::string titleStr = displayedTracks[i].title;
                    if (titleStr.length() > 14) titleStr = titleStr.substr(0, 12) + "...";
                    target.drawString(titleStr.c_str(), leftX + 46.0f, cy - 8.0f);

                    // Track Artist & Formatted Full Duration (e.g. 4:33)
                    int dMin = displayedTracks[i].durationSecs / 60;
                    int dSec = displayedTracks[i].durationSecs % 60;
                    char durBuf[16];
                    snprintf(durBuf, sizeof(durBuf), "%d:%02d", dMin, dSec);

                    target.setFont(&fonts::FreeSans9pt7b);
                    target.setTextColor(isPressed ? 0xFFFF : VoxaTheme::getTextSecondary());
                    std::string sub = displayedTracks[i].artist + " * " + durBuf;
                    if (sub.length() > 18) sub = sub.substr(0, 16) + "...";
                    target.drawString(sub.c_str(), leftX + 46.0f, cy + 8.0f);

                    // Right Play arrow
                    float chevX = leftX + cardW - 18.0f;
                    ScreenCommon::drawIcon(target, Icon::ChevronRight, chevX - 5.0f, cy - 8.0f, 16.0f, VoxaTheme::getTextSecondary());
                }

                target.clearClipRect();

                // 4. Floating Bottom Mini-Player Bar
                if (m_currentTrackIndex >= 0 && m_currentTrackIndex < (int)displayedTracks.size())
                {
                    float barY = h - 50.0f;
                    ScreenCommon::drawGlassCard(target, leftX, barY, cardW, 44.0f, 14.0f, false, 0);

                    // Mini animated equalizer bars on left
                    for (int b = 0; b < 3; ++b)
                    {
                        float barH = m_isPlaying ? (6.0f + std::abs(std::sin(m_animTime * 4.0f + b * 1.5f)) * 12.0f) : 6.0f;
                        target.fillRoundRect((int)(leftX + 14.0f + b * 6.0f), (int)(barY + 22.0f - barH * 0.5f), 3, (int)barH, 1, VoxaTheme::getSystemPurple());
                    }

                    // Playing track title
                    target.setFont(&fonts::FreeSansBold9pt7b);
                    target.setTextDatum(textdatum_t::middle_left);
                    target.setTextColor(VoxaTheme::getTextPrimary());
                    std::string mTitle = displayedTracks[m_currentTrackIndex].title;
                    if (mTitle.length() > 13) mTitle = mTitle.substr(0, 11) + "...";
                    target.drawString(mTitle.c_str(), leftX + 38.0f, barY + 22.0f);

                    // Play/Pause button on right
                    float ppX = leftX + cardW - 32.0f;
                    target.fillCircle((int)ppX, (int)(barY + 22.0f), 14, VoxaTheme::getSystemPurple());
                    ScreenCommon::drawIcon(target, m_isPlaying ? Icon::Pause : Icon::Play, ppX - 8.0f, barY + 14.0f, 16.0f, 0xFFFF);
                }
            }
            else
            {
                // ── NOW PLAYING SHEET (iOS 26 Liquid Glass Music Player) ──────
                float sheetY = 16.0f;
                float sheetH = h - 22.0f;
                float cardW = w * 0.94f;
                float cardX = (w - cardW) * 0.5f;

                ScreenCommon::drawGlassCard(target, cardX, sheetY, cardW, sheetH, 20.0f, false, 0);

                // Top Dismiss Grabber
                float grabW = 36.0f;
                target.fillRoundRect((int)(w * 0.5f - grabW * 0.5f), (int)(sheetY + 8.0f), (int)grabW, 4, 2, VoxaTheme::getDivider());

                target.setFont(&fonts::Font0);
                target.setTextDatum(textdatum_t::middle_center);
                target.setTextColor(VoxaTheme::getTextSecondary());
                target.drawString("NOW PLAYING", w * 0.5f, sheetY + 24.0f);

                // Dynamic Audio Spectrum Visualizer (7 Dancing Chromatic Glass Bars)
                float specY = sheetY + 95.0f;
                float barSpacing = 16.0f;
                float startX = w * 0.5f - 3.0f * barSpacing;

                for (int b = 0; b < 7; ++b)
                {
                    float barVal = m_isPlaying 
                        ? (12.0f + std::abs(std::sin(m_animTime * 5.0f + b * 1.2f)) * 52.0f)
                        : 10.0f;
                    
                    uint16_t bCol = (b % 3 == 0) ? VoxaTheme::getSystemPurple() 
                                  : ((b % 3 == 1) ? VoxaTheme::getSystemBlue() : VoxaTheme::getPrimary());
                    
                    target.fillRoundRect((int)(startX + b * barSpacing - 4.0f), (int)(specY - barVal * 0.5f), 8, (int)barVal, 4, bCol);
                    target.drawFastHLine((int)(startX + b * barSpacing - 2.0f), (int)(specY - barVal * 0.5f + 1.0f), 4, 0xFFFF);
                }

                // Centered Track Info
                if (m_currentTrackIndex >= 0 && m_currentTrackIndex < (int)displayedTracks.size())
                {
                    const auto& curT = displayedTracks[m_currentTrackIndex];

                    target.setFont(&fonts::FreeSansBold12pt7b);
                    target.setTextDatum(textdatum_t::middle_center);
                    target.setTextColor(VoxaTheme::getTextPrimary());
                    target.drawString(curT.title.c_str(), w * 0.5f, sheetY + 158.0f);

                    target.setFont(&fonts::FreeSans9pt7b);
                    target.setTextColor(VoxaTheme::getTextSecondary());
                    target.drawString(curT.artist.c_str(), w * 0.5f, sheetY + 180.0f);

                    // Timeline Progress Bar
                    float barX = cardX + 18.0f;
                    float barW = cardW - 36.0f;
                    float progY = sheetY + 208.0f;

                    uint32_t elapsedMs = m_isPlaying ? (nowMs - m_trackStartMs) : 0;
                    float pct = std::min(1.0f, (float)elapsedMs / (float)m_trackDurationMs);
                    if (elapsedMs >= m_trackDurationMs && m_isPlaying)
                    {
                        nextTrack();
                    }

                    // Progress Track
                    target.fillRoundRect((int)barX, (int)progY, (int)barW, 4, 2, VoxaTheme::getDivider());
                    target.fillRoundRect((int)barX, (int)progY, (int)(barW * pct), 4, 2, VoxaTheme::getSystemPurple());
                    target.fillCircle((int)(barX + barW * pct), (int)(progY + 2.0f), 4, 0xFFFF);

                    // Full Song Timestamps (e.g. "1:45" / "4:33")
                    int elapsedMin = (elapsedMs / 1000) / 60;
                    int elapsedSec = (elapsedMs / 1000) % 60;
                    int totalMin = (m_trackDurationMs / 1000) / 60;
                    int totalSec = (m_trackDurationMs / 1000) % 60;

                    char timeBuf[16];
                    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", elapsedMin, elapsedSec);
                    target.setFont(&fonts::Font0);
                    target.setTextDatum(textdatum_t::top_left);
                    target.setTextColor(VoxaTheme::getTextSecondary());
                    target.drawString(timeBuf, barX, progY + 8.0f);

                    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", totalMin, totalSec);
                    target.setTextDatum(textdatum_t::top_right);
                    target.drawString(timeBuf, barX + barW, progY + 8.0f);
                }

                // Controls (Prev, Play/Pause, Next)
                float ctlY = sheetY + 248.0f;

                // Previous Button
                target.fillCircle(44, (int)ctlY, 18, VoxaTheme::getGlassSurface());
                target.drawCircle(44, (int)ctlY, 18, VoxaTheme::getGlassBorder());
                ScreenCommon::drawIcon(target, Icon::Back, 34.0f, ctlY - 10.0f, 20.0f, VoxaTheme::getTextPrimary());

                // Large Play/Pause Capsule
                uint16_t ppCol = VoxaTheme::getSystemPurple();
                target.fillCircle((int)(w * 0.5f), (int)ctlY, 24, ppCol);
                target.drawCircle((int)(w * 0.5f), (int)ctlY, 24, 0xFFFF);
                ScreenCommon::drawIcon(target, m_isPlaying ? Icon::Pause : Icon::Play, w * 0.5f - 10.0f, ctlY - 10.0f, 20.0f, 0xFFFF);

                // Next Button
                target.fillCircle(w - 44, (int)ctlY, 18, VoxaTheme::getGlassSurface());
                target.drawCircle(w - 44, (int)ctlY, 18, VoxaTheme::getGlassBorder());
                ScreenCommon::drawIcon(target, Icon::ChevronRight, w - 54.0f, ctlY - 10.0f, 20.0f, VoxaTheme::getTextPrimary());
            }

            if (entryFrame < 10)
            {
                entryFrame++;
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Music), entryFrame, 10);
            }

            if (useSprite) canvas.pushSprite(0, 0);
            delay(16);
        }

        if (useSprite) canvas.deleteSprite();
        return targetScreen;
    }
}
