#include "TextInputScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../audio/AudioManager.h"
#include "Transition.h"
#include <vector>
#include <cmath>

namespace VOXA
{
    std::string TextInputScreen::s_prompt = "";
    ScreenId    TextInputScreen::s_backRoute = ScreenId::Home;
    bool        TextInputScreen::s_isPassword = false;
    std::string TextInputScreen::s_buffer = "";
    bool        TextInputScreen::s_isShift = false;
    bool        TextInputScreen::s_isNumMode = false;

    struct KeyDef
    {
        std::string label;
        int x, y, w, h;
        std::string val;
        enum class Type { Letter, Shift, Backspace, Mode, Space, Done } type;
    };

    void TextInputScreen::prepare(const std::string& prompt, ScreenId backRoute, bool isPassword)
    {
        s_prompt = prompt;
        s_backRoute = backRoute;
        s_isPassword = isPassword;
        s_buffer = "";
        s_isShift = false;
        s_isNumMode = false;
    }

    std::string TextInputScreen::getResult()
    {
        return s_buffer;
    }

    ScreenId TextInputScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;
        int pressedKeyIdx = -1;
        bool isBackPressed = false;
        bool isClearPressed = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return s_backRoute;
        }

        ScreenId targetScreen = ScreenId::TextInput;
        uint32_t lastMs = millis();

        // ── Keyboard Layout Configuration ──────────────────────────
        const int kh = 34;
        const int sp = 3;
        const int startY = h - 4 * (kh + sp) - 8; // approx 168px

        // Key rows maps
        const char* row1_abc = "qwertyuiop";
        const char* row1_ABC = "QWERTYUIOP";
        const char* row1_num = "1234567890";

        const char* row2_abc = "asdfghjkl";
        const char* row2_ABC = "ASDFGHJKL";
        const char* row2_num = "-/:;()$&@";

        const char* row3_abc = "zxcvbnm";
        const char* row3_ABC = "ZXCVBNM";
        const char* row3_num = ".,?!'\"_";

        while (targetScreen == ScreenId::TextInput)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Re-build keyboard keys
            std::vector<KeyDef> keys;

            // ── Row 1 (10 keys) ──
            int r1_y = startY;
            int r1_kw = 20;
            int r1_totalW = 10 * r1_kw + 9 * sp;
            int r1_sx = (w - r1_totalW) / 2;
            for (int i = 0; i < 10; ++i)
            {
                char c = s_isNumMode ? row1_num[i] : (s_isShift ? row1_ABC[i] : row1_abc[i]);
                std::string s(1, c);
                std::string disp(1, s_isNumMode ? row1_num[i] : row1_ABC[i]); // Always show clean uppercase on keycaps
                keys.push_back({disp, r1_sx + i * (r1_kw + sp), r1_y, r1_kw, kh, s, KeyDef::Type::Letter});
            }

            // ── Row 2 (9 keys) ──
            int r2_y = startY + kh + sp;
            int r2_kw = 20;
            int r2_totalW = 9 * r2_kw + 8 * sp;
            int r2_sx = (w - r2_totalW) / 2;
            for (int i = 0; i < 9; ++i)
            {
                char c = s_isNumMode ? row2_num[i] : (s_isShift ? row2_ABC[i] : row2_abc[i]);
                std::string s(1, c);
                std::string disp(1, s_isNumMode ? row2_num[i] : row2_ABC[i]);
                keys.push_back({disp, r2_sx + i * (r2_kw + sp), r2_y, r2_kw, kh, s, KeyDef::Type::Letter});
            }

            // ── Row 3 (Shift + 7 keys + Backspace) ──
            int r3_y = r2_y + kh + sp;
            int shiftW = 28;
            int backspaceW = 28;
            int r3_kw = 20;
            int r3_totalW = shiftW + backspaceW + 7 * r3_kw + 8 * sp;
            int r3_sx = (w - r3_totalW) / 2;

            // Shift
            keys.push_back({s_isNumMode ? "#+=" : "^", r3_sx, r3_y, shiftW, kh, "SHIFT", KeyDef::Type::Shift});

            // 7 keys
            for (int i = 0; i < 7; ++i)
            {
                char c = s_isNumMode ? row3_num[i] : (s_isShift ? row3_ABC[i] : row3_abc[i]);
                std::string s(1, c);
                std::string disp(1, s_isNumMode ? row3_num[i] : row3_ABC[i]);
                keys.push_back({disp, r3_sx + shiftW + sp + i * (r3_kw + sp), r3_y, r3_kw, kh, s, KeyDef::Type::Letter});
            }

            // Backspace
            keys.push_back({"<-", r3_sx + shiftW + sp + 7 * (r3_kw + sp), r3_y, backspaceW, kh, "BACKSPACE", KeyDef::Type::Backspace});

            // ── Row 4 (123 + Space + Done) ──
            int r4_y = r3_y + kh + sp;
            int modeW = 38;
            int doneW = 46;
            int spaceW = w - (r3_sx * 2) - modeW - doneW - 2 * sp;
            int r4_sx = r3_sx;

            keys.push_back({s_isNumMode ? "ABC" : "123", r4_sx, r4_y, modeW, kh, "MODE", KeyDef::Type::Mode});
            keys.push_back({"Space", r4_sx + modeW + sp, r4_y, spaceW, kh, " ", KeyDef::Type::Space});
            keys.push_back({"Done", r4_sx + modeW + spaceW + 2 * sp, r4_y, doneW, kh, "DONE", KeyDef::Type::Done});

            // ── 1. Process Touch ────────────────────────────────────
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 40);

                    // Top Back Chevron (X: 0..60, Y: 0..45)
                    if (tx <= 60 && ty <= 45)
                    {
                        isBackPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Input Box Clear Button (X: 200..235, Y: 50..95)
                    if (!s_buffer.empty() && tx >= 200 && tx <= 235 && ty >= 50 && ty <= 95)
                    {
                        isClearPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Check Keys
                    for (size_t i = 0; i < keys.size(); ++i)
                    {
                        const auto& key = keys[i];
                        if (tx >= key.x && tx <= (key.x + key.w) &&
                            ty >= key.y && ty <= (key.y + key.h))
                        {
                            pressedKeyIdx = (int)i;
                            AudioManager::instance().playTapSoundAsync();
                            break;
                        }
                    }
                }
                else
                {
                    // Swipe back check
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = s_backRoute;
                        swipeBackCandidate = false;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    if (isBackPressed)
                    {
                        isBackPressed = false;
                        targetScreen = s_backRoute;
                    }

                    if (isClearPressed)
                    {
                        isClearPressed = false;
                        s_buffer.clear();
                    }

                    if (pressedKeyIdx >= 0 && pressedKeyIdx < (int)keys.size())
                    {
                        const auto& key = keys[pressedKeyIdx];
                        if (key.type == KeyDef::Type::Shift)
                        {
                            s_isShift = !s_isShift;
                        }
                        else if (key.type == KeyDef::Type::Mode)
                        {
                            s_isNumMode = !s_isNumMode;
                        }
                        else if (key.type == KeyDef::Type::Backspace)
                        {
                            if (!s_buffer.empty())
                            {
                                s_buffer.pop_back();
                            }
                        }
                        else if (key.type == KeyDef::Type::Done)
                        {
                            AudioManager::instance().playTone(1200, 80);
                            targetScreen = s_backRoute;
                        }
                        else
                        {
                            s_buffer += key.val;
                            if (s_isShift)
                            {
                                s_isShift = false;
                            }
                        }
                    }
                    pressedKeyIdx = -1;
                }
            }

            // ── 2. Render Screen ────────────────────────────────────
            canvas.fillScreen(TFT_BLACK);

            // ── Header Navigation ──
            // Back chevron `<`
            uint16_t chevronCol = isBackPressed ? canvas.color565(245, 175, 50) : canvas.color565(180, 195, 215);
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(chevronCol);
            canvas.drawString("<", 14.0f, 12.0f);

            // Centered prompt
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::top_center);
            canvas.setTextColor(canvas.color565(130, 145, 165));
            std::string headerTitle = s_prompt.empty() ? "KEYBOARD" : s_prompt;
            for (auto& c : headerTitle) c = toupper((unsigned char)c);
            canvas.drawString(headerTitle.c_str(), w * 0.5f, 15.0f);

            // ── Input Display Card Box (X: 10, Y: 42, W: 220, H: 46) ──
            int ibX = 10;
            int ibY = 40;
            int ibW = w - 20;
            int ibH = 46;
            canvas.fillRoundRect(ibX, ibY, ibW, ibH, 8, canvas.color565(16, 18, 24));
            canvas.drawRoundRect(ibX, ibY, ibW, ibH, 8, canvas.color565(36, 42, 54));

            // Format displayed text
            std::string dispStr = "";
            if (s_isPassword)
            {
                dispStr.assign(s_buffer.length(), '*');
            }
            else
            {
                dispStr = s_buffer;
            }

            // Blinking cursor
            bool showCursor = ((millis() / 500) % 2 == 0);
            if (showCursor)
            {
                dispStr += "|";
            }

            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setTextColor(0xFFFF);
            canvas.drawString(dispStr.c_str(), ibX + 12, ibY + ibH / 2);

            // Clear button if text exists
            if (!s_buffer.empty())
            {
                uint16_t clearBg = isClearPressed ? canvas.color565(50, 55, 70) : canvas.color565(28, 32, 42);
                canvas.fillCircle(ibX + ibW - 18, ibY + ibH / 2, 9, clearBg);
                canvas.setFont(&fonts::Font0);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(canvas.color565(160, 175, 195));
                canvas.drawString("X", ibX + ibW - 18, ibY + ibH / 2);
            }

            // ── Render Keyboard Keys ────────────────────────────────
            for (size_t i = 0; i < keys.size(); ++i)
            {
                const auto& key = keys[i];
                bool isPressed = (pressedKeyIdx == (int)i);

                if (key.type == KeyDef::Type::Space)
                {
                    // Space: Solid White rounded key
                    uint16_t bg = isPressed ? canvas.color565(200, 200, 200) : 0xFFFF;
                    canvas.fillRoundRect(key.x, key.y, key.w, key.h, 6, bg);

                    canvas.setFont(&fonts::Font0);
                    canvas.setTextDatum(textdatum_t::middle_center);
                    canvas.setTextColor(canvas.color565(35, 40, 50)); // Dark charcoal
                    canvas.drawString("Space", key.x + key.w / 2, key.y + key.h / 2);
                }
                else if (key.type == KeyDef::Type::Done)
                {
                    // Done: Warm golden / amber orange rounded key
                    uint16_t bg = isPressed ? canvas.color565(210, 140, 30) : canvas.color565(245, 175, 50);
                    canvas.fillRoundRect(key.x, key.y, key.w, key.h, 6, bg);

                    canvas.setFont(&fonts::Font0);
                    canvas.setTextDatum(textdatum_t::middle_center);
                    canvas.setTextColor(canvas.color565(35, 30, 20)); // Dark charcoal / black
                    canvas.drawString("Done", key.x + key.w / 2, key.y + key.h / 2);
                }
                else if (key.type == KeyDef::Type::Shift)
                {
                    // Shift Key
                    uint16_t bg = isPressed ? canvas.color565(38, 44, 58) : (s_isShift ? canvas.color565(42, 50, 68) : canvas.color565(22, 26, 36));
                    uint16_t border = s_isShift ? canvas.color565(245, 175, 50) : canvas.color565(34, 40, 54);
                    canvas.fillRoundRect(key.x, key.y, key.w, key.h, 6, bg);
                    canvas.drawRoundRect(key.x, key.y, key.w, key.h, 6, border);

                    // Draw Shift Up Arrow
                    int cx = key.x + key.w / 2;
                    int cy = key.y + key.h / 2;
                    uint16_t arrowCol = s_isShift ? canvas.color565(245, 175, 50) : 0xFFFF;
                    
                    if (s_isNumMode)
                    {
                        canvas.setFont(&fonts::Font0);
                        canvas.setTextDatum(textdatum_t::middle_center);
                        canvas.setTextColor(arrowCol);
                        canvas.drawString("#+=", cx, cy);
                    }
                    else
                    {
                        // Draw crisp arrow triangle + stem
                        canvas.fillTriangle(cx, cy - 6, cx - 5, cy, cx + 5, cy, arrowCol);
                        canvas.fillRect(cx - 2, cy, 5, 6, arrowCol);
                    }
                }
                else if (key.type == KeyDef::Type::Backspace)
                {
                    // Backspace Key
                    uint16_t bg = isPressed ? canvas.color565(38, 44, 58) : canvas.color565(22, 26, 36);
                    canvas.fillRoundRect(key.x, key.y, key.w, key.h, 6, bg);
                    canvas.drawRoundRect(key.x, key.y, key.w, key.h, 6, canvas.color565(34, 40, 54));

                    // Draw crisp backspace icon (pointing left with an X)
                    int cx = key.x + key.w / 2;
                    int cy = key.y + key.h / 2;
                    uint16_t bsCol = 0xFFFF;
                    
                    canvas.fillTriangle(cx - 7, cy, cx - 2, cy - 5, cx - 2, cy + 5, bsCol);
                    canvas.fillRect(cx - 2, cy - 5, 9, 11, bsCol);
                    // Inner dark cross
                    canvas.drawLine(cx, cy - 3, cx + 5, cy + 3, bg);
                    canvas.drawLine(cx, cy + 3, cx + 5, cy - 3, bg);
                }
                else
                {
                    // Standard Letter / Mode Key (Dark Slate Tile)
                    uint16_t bg = isPressed ? canvas.color565(38, 44, 58) : canvas.color565(22, 26, 36);
                    canvas.fillRoundRect(key.x, key.y, key.w, key.h, 6, bg);
                    canvas.drawRoundRect(key.x, key.y, key.w, key.h, 6, canvas.color565(34, 40, 54));

                    canvas.setTextDatum(textdatum_t::middle_center);
                    canvas.setTextColor(0xFFFF); // Clean high-contrast white

                    if (key.type == KeyDef::Type::Mode)
                    {
                        canvas.setFont(&fonts::Font0);
                    }
                    else
                    {
                        canvas.setFont(&fonts::Font0);
                    }
                    canvas.drawString(key.label.c_str(), key.x + key.w / 2, key.y + key.h / 2);
                }
            }

            // Push Sprite
            if (entryFrame < 6)
            {
                playSlideInFrame(canvas, getTransitionType(g_lastScreenId, ScreenId::TextInput), entryFrame, 6);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }
            delay(16);
        }

        canvas.deleteSprite();
        return targetScreen;
    }
}
