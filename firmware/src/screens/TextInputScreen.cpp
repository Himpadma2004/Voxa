#include "TextInputScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
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
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return s_backRoute;
        }

        ScreenId targetScreen = ScreenId::TextInput; // Remain on this virtual screen ID until Done/Back
        uint32_t lastMs = millis();

        // Key sizing parameters
        int kw = (w - 20) / 10;
        int kh = 26;
        int sp = 2;
        int startY = h - 4 * (kh + sp) - 10;

        // Mode row maps
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

            // Re-build key definitions dynamically based on shift / number mode
            std::vector<KeyDef> keys;
            
            // Row 1
            int r1_y = startY;
            int r1_count = 10;
            int r1_w = r1_count * kw + (r1_count - 1) * sp;
            int r1_sx = (w - r1_w) / 2;
            for (int i = 0; i < r1_count; ++i)
            {
                char c = s_isNumMode ? row1_num[i] : (s_isShift ? row1_ABC[i] : row1_abc[i]);
                std::string s(1, c);
                keys.push_back({s, r1_sx + i * (kw + sp), r1_y, kw, kh, s});
            }

            // Row 2
            int r2_y = startY + kh + sp;
            int r2_count = 9;
            int r2_w = r2_count * kw + (r2_count - 1) * sp;
            int r2_sx = (w - r2_w) / 2;
            for (int i = 0; i < r2_count; ++i)
            {
                char c = s_isNumMode ? row2_num[i] : (s_isShift ? row2_ABC[i] : row2_abc[i]);
                std::string s(1, c);
                keys.push_back({s, r2_sx + i * (kw + sp), r2_y, kw, kh, s});
            }

            // Row 3 (Shift, characters, Backspace)
            int r3_y = r2_y + kh + sp;
            int shiftW = kw * 1.4;
            int backspaceW = kw * 1.4;
            int charCount = 7;
            int r3_w = shiftW + backspaceW + charCount * kw + (charCount + 1) * sp;
            int r3_sx = (w - r3_w) / 2;
            
            keys.push_back({s_isNumMode ? "#+=" : (s_isShift ? "abc" : "ABC"), r3_sx, r3_y, shiftW, kh, "SHIFT"});

            for (int i = 0; i < charCount; ++i)
            {
                char c = s_isNumMode ? row3_num[i] : (s_isShift ? row3_ABC[i] : row3_abc[i]);
                std::string s(1, c);
                keys.push_back({s, r3_sx + shiftW + sp + i * (kw + sp), r3_y, kw, kh, s});
            }

            keys.push_back({"<-", r3_sx + shiftW + sp + charCount * (kw + sp), r3_y, backspaceW, kh, "BACKSPACE"});

            // Row 4 (Mode switch, Space, Done)
            int r4_y = r3_y + kh + sp;
            int modeW = kw * 2;
            int doneW = kw * 2.5;
            int spaceW = w - modeW - doneW - 4 * sp;
            int r4_sx = sp * 2;

            keys.push_back({s_isNumMode ? "abc" : "123", r4_sx, r4_y, modeW, kh, "MODE"});
            keys.push_back({"Space", r4_sx + modeW + sp, r4_y, spaceW, kh, " "});
            keys.push_back({"Done", r4_sx + modeW + spaceW + 2 * sp, r4_y, doneW, kh, "DONE"});

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    swipeBackCandidate = (tx < 50);

                    // Back button bounds at top-left
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        targetScreen = s_backRoute;
                    }

                    // Key presses
                    for (const auto& key : keys)
                    {
                        if (tx >= key.x && tx <= (key.x + key.w) &&
                            ty >= key.y && ty <= (key.y + key.h))
                        {
                            if (key.val == "SHIFT")
                            {
                                s_isShift = !s_isShift;
                            }
                            else if (key.val == "MODE")
                            {
                                s_isNumMode = !s_isNumMode;
                            }
                            else if (key.val == "BACKSPACE")
                            {
                                if (!s_buffer.empty())
                                {
                                    s_buffer.pop_back();
                                }
                            }
                            else if (key.val == "DONE")
                            {
                                targetScreen = s_backRoute;
                            }
                            else
                            {
                                s_buffer += key.val;
                                // Auto un-shift after one uppercase character
                                if (s_isShift)
                                {
                                    s_isShift = false;
                                }
                            }
                            break;
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    if (swipeBackCandidate && dx > 60)
                    {
                        targetScreen = s_backRoute;
                        swipeBackCandidate = false;
                    }
                }
            }
            else
            {
                m_wasTouched = false;
            }

            // 2. Render Screen
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, s_prompt, true, false, Icon::Plus, w, h);

            // Input Display Box
            canvas.drawRoundRect(10, 68, w - 20, 36, 6, VoxaTheme::getSurface());
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::FreeSans9pt7b);
            
            std::string displayText = s_buffer;
            
            // Add a blinking cursor
            if ((millis() / 500) % 2 == 0)
            {
                displayText += "|";
            }
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.drawString(displayText.c_str(), 18, 86);

            // Render Keyboard Keys
            for (const auto& key : keys)
            {
                uint16_t keyColor = VoxaTheme::getSurface();
                uint16_t textColor = VoxaTheme::getTextPrimary();

                if (key.val == "DONE")
                {
                    keyColor = VoxaTheme::getPrimary();
                    textColor = TFT_WHITE;
                }
                else if (key.val == "SHIFT" && s_isShift)
                {
                    keyColor = VoxaTheme::getPrimary();
                    textColor = TFT_WHITE;
                }

                canvas.fillRoundRect(key.x, key.y, key.w, key.h, 4, keyColor);
                canvas.drawRoundRect(key.x, key.y, key.w, key.h, 4, VoxaTheme::getSurface());
                
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(textColor);
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.drawString(key.label.c_str(), key.x + key.w / 2, key.y + key.h / 2);
            }

            // Push Sprite
            if (entryFrame < 10)
            {
                playSlideInFrame(canvas, getTransitionType(g_lastScreenId, ScreenId::TextInput), entryFrame, 10);
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
