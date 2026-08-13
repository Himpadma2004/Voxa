#include "BootScreen.h"
#include "../ui/Theme.h"
#include "../audio/AudioManager.h"
#include <cmath>
#include <algorithm>

BootScreen::BootScreen()
{
}


void BootScreen::drawBackground(LGFX_Sprite& canvas, uint16_t w, uint16_t h)
{
    // Render a clean obsidian black to dark charcoal vertical gradient
    for (int y = 0; y < h; ++y)
    {
        float t = (float)y / (h - 1);
        uint8_t r = (uint8_t)((1.0f - t) * 4 + t * 14);
        uint8_t g = (uint8_t)((1.0f - t) * 4 + t * 10);
        uint8_t b = (uint8_t)((1.0f - t) * 6 + t * 16);
        uint16_t color = canvas.color565(r, g, b);
        canvas.drawFastHLine(0, y, w, color);
    }
}

void BootScreen::drawGlowCircle(LGFX_Sprite& canvas, float cx, float cy, float radius, 
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a, 
                                int layers, uint16_t h)
{
    // Draw concentric orange glow layers from outer to inner
    for (int i = layers; i >= 1; --i)
    {
        float r_curr = radius + i * 2.3f;
        uint8_t a_curr = std::max(4, (int)a / (i * 2));
        float alpha_f = a_curr / 255.0f;
        
        float t_bg = cy / (h - 1);
        t_bg = std::max(0.0f, std::min(1.0f, t_bg));
        float bg_r = (1.0f - t_bg) * 4.0f + t_bg * 14.0f;
        float bg_g = (1.0f - t_bg) * 4.0f + t_bg * 10.0f;
        float bg_b = (1.0f - t_bg) * 6.0f + t_bg * 16.0f;
        
        uint8_t r_blend = (uint8_t)((1.0f - alpha_f) * bg_r + alpha_f * r);
        uint8_t g_blend = (uint8_t)((1.0f - alpha_f) * bg_g + alpha_f * g);
        uint8_t b_blend = (uint8_t)((1.0f - alpha_f) * bg_b + alpha_f * b);
        
        canvas.fillCircle((int)cx, (int)cy, (int)r_curr, canvas.color565(r_blend, g_blend, b_blend));
    }
}

void BootScreen::drawWaves(LGFX_Sprite& canvas, float elapsed, uint16_t w, uint16_t h)
{
    // Render 5 waves in Electric Orange tint
    for (int i = 0; i < 5; ++i)
    {
        const float alpha_f = (48 - i * 7) / 255.0f;
        const float r_wave = 253;
        const float g_wave = 100 + i * 15;
        const float b_wave = 0;
        const float offset = elapsed * 42.0f + static_cast<float>(i) * 18.0f;
        
        for (int x = 0; x < w; ++x)
        {
            const float xf = static_cast<float>(x);
            const float y = h * 0.8f + std::sin((xf + offset) * 0.03f) * 10.0f + std::sin((xf - offset) * 0.02f) * 6.0f + i * 2.0f;
            
            float t_bg = y / (h - 1);
            t_bg = std::max(0.0f, std::min(1.0f, t_bg));
            float bg_r = (1.0f - t_bg) * 4.0f + t_bg * 14.0f;
            float bg_g = (1.0f - t_bg) * 4.0f + t_bg * 10.0f;
            float bg_b = (1.0f - t_bg) * 6.0f + t_bg * 16.0f;
            
            uint8_t r = (uint8_t)((1.0f - alpha_f) * bg_r + alpha_f * r_wave);
            uint8_t g = (uint8_t)((1.0f - alpha_f) * bg_g + alpha_f * g_wave);
            uint8_t b = (uint8_t)((1.0f - alpha_f) * bg_b + alpha_f * b_wave);
            
            float radius = 0.8f + i * 0.2f;
            int r_int = (int)(radius + 0.5f);
            uint16_t color = canvas.color565(r, g, b);
            
            if (r_int <= 0)
            {
                canvas.drawPixel((int)xf, (int)y, color);
            }
            else
            {
                canvas.fillCircle((int)xf, (int)y, r_int, color);
            }
        }
    }
}

void BootScreen::drawProgressBar(LGFX_Sprite& canvas, float progress, uint16_t w, uint16_t h)
{
    float barW = w * 0.5f;
    float barH = 6.0f;
    float barX = w * 0.25f;
    float barY = h * 0.72f;

    uint16_t trackColor = canvas.color565(35, 35, 45);
    uint16_t fillColor = VoxaTheme::getPrimary(); // Electric Orange

    // Draw track
    canvas.fillRoundRect((int)barX, (int)barY, (int)barW, (int)barH, (int)(barH * 0.5f), trackColor);

    // Draw filled portion
    float fillW = barW * progress;
    if (fillW >= barH)
    {
        canvas.fillRoundRect((int)barX, (int)barY, (int)fillW, (int)barH, (int)(barH * 0.5f), fillColor);
    }
    else if (fillW > 0)
    {
        canvas.fillRoundRect((int)barX, (int)barY, (int)barH, (int)barH, (int)(barH * 0.5f), fillColor);
    }
}

void BootScreen::show()
{
    uint16_t w = Display::width();
    uint16_t h = Display::height();

    // Initialize double-buffering canvas sprite
    LGFX_Sprite canvas(&Display::lcd);
    canvas.setPsram(true);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(w, h))
    {
        Serial.println("[BootScreen] Error creating sprite double-buffer!");
        return;
    }
    canvas.fillScreen(TFT_BLACK);

    // Trigger boot screen chime audio
    VOXA::AudioManager::instance().playBootChimeAsync();

    uint32_t startMs = millis();

    constexpr float durationSecs = 1.8f; // Fast, responsive boot animation

    while (true)
    {
        uint32_t nowMs = millis();
        float elapsed = (nowMs - startMs) / 1000.0f;
        if (elapsed >= durationSecs)
        {
            elapsed = durationSecs;
        }

        // 1. Draw vertical background gradient
        drawBackground(canvas, w, h);

        // 2. Draw Electric Orange glow circles
        drawGlowCircle(canvas, w * 0.84f, h * 0.7f,  40.0f, 253, 64, 0, 24, 6, h);
        drawGlowCircle(canvas, w * 0.12f, h * 0.72f, 30.0f, 253, 90, 0, 16, 5, h);
        drawGlowCircle(canvas, w * 0.5f,  h * 0.22f, 50.0f, 253, 64, 0, 15, 5, h);

        // 3. Draw animated waves
        drawWaves(canvas, elapsed, w, h);

        // 4. Draw texts with smooth anti-aliased fonts (like HomeScreen)
        float fadeAlpha = elapsed * 2.0f;
        if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;

        canvas.setTextDatum(textdatum_t::middle_center);
        
        // VOXA Title using FreeSansBold18pt7b for crisp modern typography
        canvas.setFont(&fonts::FreeSansBold18pt7b);
        canvas.setTextSize(1);
        canvas.setTextColor(VoxaTheme::getPrimary()); // Electric Orange
        canvas.drawString("VOXA", w * 0.5f, h * 0.28f);

        // Tagline using FreeSans9pt7b
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextColor(VoxaTheme::getTextSecondary());
        canvas.drawString("Take care of your moments", w * 0.5f, h * 0.46f);

        // Initializing Status text using FreeSans9pt7b
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextColor(VoxaTheme::getPrimaryLight());
        canvas.drawString("Initializing...", w * 0.5f, h * 0.60f);

        // 5. Draw progress bar
        float progress = elapsed / durationSecs;
        drawProgressBar(canvas, progress, w, h);

        // Push render buffer to screen
        canvas.pushSprite(0, 0);

        // Frame rate limiter (60 FPS)
        uint32_t frameMs = millis() - nowMs;
        if (frameMs < 16)
        {
            delay(16 - frameMs);
        }

        if (elapsed >= durationSecs)
        {
            break;
        }
    }

    // Clean up buffer without artificial delays so screen transitions immediately
    canvas.deleteSprite();
}