#include "BootScreen.h"
#include <cmath>
#include <algorithm>

BootScreen::BootScreen()
{
}

void BootScreen::drawBackground(LGFX_Sprite& canvas, uint16_t w, uint16_t h)
{
    // Render a linear vertical gradient from #08080C (top) to #120E1C (bottom)
    for (int y = 0; y < h; ++y)
    {
        float t = (float)y / (h - 1);
        uint8_t r = (uint8_t)((1.0f - t) * 8 + t * 18);
        uint8_t g = (uint8_t)((1.0f - t) * 8 + t * 14);
        uint8_t b = (uint8_t)((1.0f - t) * 12 + t * 28);
        uint16_t color = canvas.color565(r, g, b);
        canvas.drawFastHLine(0, y, w, color);
    }
}

void BootScreen::drawGlowCircle(LGFX_Sprite& canvas, float cx, float cy, float radius, 
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a, 
                                int layers, uint16_t h)
{
    // Draw concentric glow layers from outer to inner
    for (int i = layers; i >= 1; --i)
    {
        float r_curr = radius + i * 2.3f;
        uint8_t a_curr = std::max(4, (int)a / (i * 2));
        float alpha_f = a_curr / 255.0f;
        
        // Sample the background color at centerY to perform alpha pre-blending
        float t_bg = cy / (h - 1);
        t_bg = std::max(0.0f, std::min(1.0f, t_bg));
        float bg_r = (1.0f - t_bg) * 8.0f + t_bg * 18.0f;
        float bg_g = (1.0f - t_bg) * 8.0f + t_bg * 14.0f;
        float bg_b = (1.0f - t_bg) * 12.0f + t_bg * 28.0f;
        
        uint8_t r_blend = (uint8_t)((1.0f - alpha_f) * bg_r + alpha_f * r);
        uint8_t g_blend = (uint8_t)((1.0f - alpha_f) * bg_g + alpha_f * g);
        uint8_t b_blend = (uint8_t)((1.0f - alpha_f) * bg_b + alpha_f * b);
        
        canvas.fillCircle((int)cx, (int)cy, (int)r_curr, canvas.color565(r_blend, g_blend, b_blend));
    }
}

void BootScreen::drawWaves(LGFX_Sprite& canvas, float elapsed, uint16_t w, uint16_t h)
{
    // Render 5 waves with dynamic sine wave offset based on elapsed time
    for (int i = 0; i < 5; ++i)
    {
        const float alpha_f = (52 - i * 7) / 255.0f;
        const float r_wave = 124 + i * 12;
        const float g_wave = 92 + i * 10;
        const float b_wave = 255;
        const float offset = elapsed * 42.0f + static_cast<float>(i) * 18.0f;
        
        for (int x = 0; x < w; ++x)
        {
            const float xf = static_cast<float>(x);
            const float y = h * 0.8f + std::sin((xf + offset) * 0.03f) * 10.0f + std::sin((xf - offset) * 0.02f) * 6.0f + i * 2.0f;
            
            // Get local background color at the current y coordinate for alpha pre-blending
            float t_bg = y / (h - 1);
            t_bg = std::max(0.0f, std::min(1.0f, t_bg));
            float bg_r = (1.0f - t_bg) * 8.0f + t_bg * 18.0f;
            float bg_g = (1.0f - t_bg) * 8.0f + t_bg * 14.0f;
            float bg_b = (1.0f - t_bg) * 12.0f + t_bg * 28.0f;
            
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
    float barW = w * 0.4f;
    float barH = 5.0f;
    float barX = w * 0.3f;
    float barY = h * 0.6f;

    uint16_t trackColor = canvas.color565(48, 48, 60);
    uint16_t fillColor = canvas.color565(124, 92, 255);

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
    canvas.setColorDepth(16);
    if (!canvas.createSprite(w, h))
    {
        Serial.println("[BootScreen] Error creating sprite double-buffer!");
        return;
    }

    uint32_t startMs = millis();
    constexpr float durationSecs = 2.5f;

    while (true)
    {
        uint32_t nowMs = millis();
        float elapsed = (nowMs - startMs) / 1000.0f;
        if (elapsed >= durationSecs)
        {
            elapsed = durationSecs;
        }

        // 1. Draw vertical gradient background
        drawBackground(canvas, w, h);

        // 2. Draw glow circles
        drawGlowCircle(canvas, w * 0.84f, h * 0.7f,  40.0f, 124, 92,  255, 18, 6, h);
        drawGlowCircle(canvas, w * 0.12f, h * 0.72f, 30.0f, 92,  60,  240, 12, 5, h);
        drawGlowCircle(canvas, w * 0.5f,  h * 0.22f, 50.0f, 124, 92,  255, 10, 5, h);

        // 3. Draw animated waves
        drawWaves(canvas, elapsed, w, h);

        // 4. Draw texts with fade-in alpha
        float fadeAlpha = elapsed * 1.5f;
        if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;

        canvas.setTextDatum(textdatum_t::middle_center);
        
        // VOXA Title Y = 25% of height
        float t_title = 0.25f;
        uint8_t bg_r_t = (uint8_t)((1.0f - t_title) * 8 + t_title * 18);
        uint8_t bg_g_t = (uint8_t)((1.0f - t_title) * 8 + t_title * 14);
        uint8_t bg_b_t = (uint8_t)((1.0f - t_title) * 12 + t_title * 28);
        
        uint8_t title_r = (uint8_t)((1.0f - fadeAlpha) * bg_r_t + fadeAlpha * 210);
        uint8_t title_g = (uint8_t)((1.0f - fadeAlpha) * bg_g_t + fadeAlpha * 190);
        uint8_t title_b = (uint8_t)((1.0f - fadeAlpha) * bg_b_t + fadeAlpha * 255);
        canvas.setTextColor(canvas.color565(title_r, title_g, title_b));
        canvas.setTextSize(4);
        canvas.drawString("VOXA", w * 0.5f, h * 0.25f);

        // Tagline Y = 40% of height
        float t_tag = 0.40f;
        uint8_t bg_r_tag = (uint8_t)((1.0f - t_tag) * 8 + t_tag * 18);
        uint8_t bg_g_tag = (uint8_t)((1.0f - t_tag) * 8 + t_tag * 14);
        uint8_t bg_b_tag = (uint8_t)((1.0f - t_tag) * 12 + t_tag * 28);
        
        uint8_t tag_r = (uint8_t)((1.0f - fadeAlpha) * bg_r_tag + fadeAlpha * 225);
        uint8_t tag_g = (uint8_t)((1.0f - fadeAlpha) * bg_g_tag + fadeAlpha * 225);
        uint8_t tag_b = (uint8_t)((1.0f - fadeAlpha) * bg_b_tag + fadeAlpha * 235);
        canvas.setTextColor(canvas.color565(tag_r, tag_g, tag_b));
        canvas.setTextSize(1);
        canvas.drawString("Take care of your moments", w * 0.5f, h * 0.40f);

        // Initializing Status text Y = 52% of height
        float t_stat = 0.52f;
        uint8_t bg_r_stat = (uint8_t)((1.0f - t_stat) * 8 + t_stat * 18);
        uint8_t bg_g_stat = (uint8_t)((1.0f - t_stat) * 8 + t_stat * 14);
        uint8_t bg_b_stat = (uint8_t)((1.0f - t_stat) * 12 + t_stat * 28);
        
        uint8_t stat_r = (uint8_t)((1.0f - fadeAlpha) * bg_r_stat + fadeAlpha * 210);
        uint8_t stat_g = (uint8_t)((1.0f - fadeAlpha) * bg_g_stat + fadeAlpha * 210);
        uint8_t stat_b = (uint8_t)((1.0f - fadeAlpha) * bg_b_stat + fadeAlpha * 220);
        canvas.setTextColor(canvas.color565(stat_r, stat_g, stat_b));
        canvas.drawString("Initializing...", w * 0.5f, h * 0.52f);

        // 5. Draw progress bar Y = 60% of height
        float progress = elapsed / durationSecs;
        drawProgressBar(canvas, progress, w, h);

        // Push render buffer to screen
        canvas.pushSprite(0, 0);

        // Frame rate limiter (roughly 60 FPS)
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

    delay(400);
}