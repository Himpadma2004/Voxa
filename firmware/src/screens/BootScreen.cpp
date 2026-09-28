#include "BootScreen.h"
#include "../ui/Theme.h"
#include "../audio/AudioManager.h"
#include <cmath>
#include <algorithm>

BootScreen::BootScreen() {}

// ─── Utility ──────────────────────────────────────────────────────────────────
static inline float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
static inline float easeOut3(float t) { float f = 1.0f - t; return 1.0f - f * f * f; }

// ─── Draw ⊙ Icon ──────────────────────────────────────────────────────────────
static void drawIcon(LGFX_Sprite& canvas, float cx, float cy, float alpha)
{
    if (alpha <= 0.01f) return;

    // Soft halo bloom around outer ring (r=14..18)
    for (int r = 18; r >= 13; --r)
    {
        float t = (float)(18 - r) / 5.0f;
        uint8_t haloVal = (uint8_t)(32.0f * t * alpha);
        if (haloVal > 0)
        {
            uint16_t haloCol = canvas.color565(haloVal, haloVal, haloVal);
            canvas.drawCircle((int)cx, (int)cy, r, haloCol);
        }
    }

    uint8_t a = (uint8_t)(255 * alpha);
    uint16_t ringCol = canvas.color565(a, a, a);

    // Thick outer ring: diameter ~24px (radii 10, 11, 12)
    canvas.drawCircle((int)cx, (int)cy, 10, ringCol);
    canvas.drawCircle((int)cx, (int)cy, 11, ringCol);
    canvas.drawCircle((int)cx, (int)cy, 12, ringCol);

    // Filled center dot: radius 3px
    canvas.fillCircle((int)cx, (int)cy, 3, ringCol);
}

// ─── Draw "voxa" Text ─────────────────────────────────────────────────────────
static void drawLogoText(LGFX_Sprite& canvas, float textX, float cy, float alpha)
{
    if (alpha <= 0.01f) return;

    uint8_t a = (uint8_t)(255 * alpha);
    uint16_t textCol = canvas.color565(a, a, a);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.setTextColor(textCol);
    canvas.drawString("voxa", (int)textX, (int)cy);
}

// ─── Draw "SILICON MICROKERNEL" Subtitle ───────────────────────────────────────
static void drawSubtitle(LGFX_Sprite& canvas, float cx, float cy, float alpha)
{
    if (alpha <= 0.01f) return;

    // Warm champagne/gold tint: RGB(175, 158, 135)
    uint8_t r = (uint8_t)(175 * alpha);
    uint8_t g = (uint8_t)(158 * alpha);
    uint8_t b = (uint8_t)(135 * alpha);
    uint16_t subCol = canvas.color565(r, g, b);

    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(subCol);
    canvas.drawString("New Tomorrow", (int)cx, (int)cy);
}

// ─── Draw Horizontal Lens Flare ──────────────────────────────────────────────
static void drawLensFlare(LGFX_Sprite& canvas, float cx, float cy, float intensity, uint16_t w, uint16_t h)
{
    if (intensity <= 0.01f) return;

    // 1. Subtle horizon reflection glow below flare line
    int startY = (int)cy + 2;
    int endY = std::min((int)h, (int)cy + 45);
    for (int y = startY; y < endY; ++y)
    {
        float dy = (float)(y - cy);
        float alphaY = (1.0f - dy / 45.0f);
        alphaY = alphaY * alphaY;
        uint8_t baseB = (uint8_t)(22.0f * alphaY * intensity);
        if (baseB < 1) continue;

        uint16_t gradCol = canvas.color565(baseB, (baseB * 9) / 10, (baseB * 8) / 10);
        int span = (int)(w * 0.40f * alphaY);
        canvas.drawFastHLine((int)cx - span, y, span * 2, gradCol);
    }

    // 2. Horizontal streak beam across nearly the full screen width
    int maxDx = (int)(w * 0.47f * intensity);
    for (int dx = 1; dx <= maxDx; ++dx)
    {
        float t = (float)dx / (float)maxDx;
        float falloff = (1.0f - t);
        falloff = falloff * std::sqrt(falloff); // 1.5 power smooth falloff
        uint8_t bright = (uint8_t)(255.0f * falloff * intensity);
        if (bright < 3) break;

        uint16_t coreCol = canvas.color565(bright, bright, bright);
        int pxL = (int)cx - dx;
        int pxR = (int)cx + dx;
        int py = (int)cy;

        if (pxL >= 0) canvas.drawPixel(pxL, py, coreCol);
        if (pxR < w)  canvas.drawPixel(pxR, py, coreCol);

        // +-1px vertical blur
        uint8_t b1 = (uint8_t)(bright * 0.45f);
        if (b1 > 2)
        {
            uint16_t col1 = canvas.color565(b1, b1, b1);
            if (pxL >= 0) { canvas.drawPixel(pxL, py - 1, col1); canvas.drawPixel(pxL, py + 1, col1); }
            if (pxR < w)  { canvas.drawPixel(pxR, py - 1, col1); canvas.drawPixel(pxR, py + 1, col1); }
        }

        // +-2px faint vertical blur
        uint8_t b2 = (uint8_t)(bright * 0.18f);
        if (b2 > 2)
        {
            uint16_t col2 = canvas.color565(b2, b2, b2);
            if (pxL >= 0) { canvas.drawPixel(pxL, py - 2, col2); canvas.drawPixel(pxL, py + 2, col2); }
            if (pxR < w)  { canvas.drawPixel(pxR, py - 2, col2); canvas.drawPixel(pxR, py + 2, col2); }
        }
    }

    // 3. Central flare bloom star / orb
    for (int dy = -12; dy <= 12; ++dy)
    {
        int y = (int)cy + dy;
        if (y < 0 || y >= h) continue;
        float ny = (float)dy / 12.0f;

        for (int dx = -24; dx <= 24; ++dx)
        {
            int x = (int)cx + dx;
            if (x < 0 || x >= w) continue;
            float nx = (float)dx / 24.0f;

            float dist = std::sqrt(nx * nx + ny * ny);
            if (dist <= 1.0f)
            {
                float f = (1.0f - dist);
                f = f * f;
                uint8_t b = (uint8_t)(255.0f * f * intensity);
                if (b > 3)
                {
                    uint16_t col = canvas.color565(b, (b * 98) / 100, (b * 95) / 100);
                    canvas.drawPixel(x, y, col);
                }
            }
        }
    }

    // 4. Intense white center hot core
    uint8_t coreB = (uint8_t)(255 * intensity);
    uint16_t white = canvas.color565(coreB, coreB, coreB);
    canvas.fillCircle((int)cx, (int)cy, 3, white);
    canvas.drawPixel((int)cx - 4, (int)cy, white);
    canvas.drawPixel((int)cx + 4, (int)cy, white);
}

// ─── Main show() ──────────────────────────────────────────────────────────────
void BootScreen::show()
{
    uint16_t w = Display::width();
    uint16_t h = Display::height();

    LGFX_Sprite canvas(&Display::lcd);
    canvas.setPsram(true);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(w, h))
    {
        Serial.println("[BootScreen] Sprite alloc failed!");
        return;
    }

    uint32_t startMs = millis();
    constexpr float TOTAL = 4.5f;

    // Centered layout calculations:
    // Screen is 240 x 320
    float cx = w * 0.5f;
    float logoY = h * 0.38f;      // ~122
    float subY  = h * 0.48f;      // ~154
    float flareY = h * 0.60f;     // ~192

    // Icon + "voxa" text centering:
    // Icon diameter ~24px (r=12), gap ~10px, "voxa" ~79px -> total width ~113px
    // Centered around cx=120:
    float iconCx = cx - 44.0f;    // ~76
    float textX  = cx - 22.0f;    // ~98

    while (true)
    {
        uint32_t nowMs = millis();
        float elapsed  = std::min((float)(nowMs - startMs) / 1000.0f, TOTAL);

        // Fill pure black background
        canvas.fillScreen(0x0000);

        // Global fade-out in final 0.5s
        float globalAlpha = 1.0f;
        if (elapsed > 4.0f)
        {
            globalAlpha = clamp01((TOTAL - elapsed) / 0.5f);
        }

        // Phase 1 (0 – 0.6s): Flare blooms open from center
        float flareIntensity = clamp01(elapsed / 0.55f);
        flareIntensity = easeOut3(flareIntensity);

        // Subtle gentle breathing shimmer during hold
        if (elapsed > 1.2f && elapsed <= 4.0f)
        {
            flareIntensity *= (1.0f + 0.03f * std::sin((elapsed - 1.2f) * 3.5f));
        }

        // Phase 2 (0.45s – 1.4s): Logo & Subtitle fade in smoothly
        float textAlpha = clamp01((elapsed - 0.45f) / 0.90f);
        textAlpha = easeOut3(textAlpha);

        // Apply global fade-out to all elements
        float effectiveFlare = flareIntensity * globalAlpha;
        float effectiveLogo  = textAlpha * globalAlpha;

        // Draw Flare line & glow
        drawLensFlare(canvas, cx, flareY, effectiveFlare, w, h);

        // // Draw ⊙ Icon
        // drawIcon(canvas, iconCx, logoY, effectiveLogo);

        // Draw "voxa" text (large bold FreeSansBold18pt7b)
        drawLogoText(canvas, textX, logoY, effectiveLogo);

        // Draw "SILICON MICROKERNEL" subtitle (spaced FreeSans9pt7b)
        drawSubtitle(canvas, cx, subY, effectiveLogo);

        // Push frame to LCD
        canvas.pushSprite(0, 0);

        uint32_t frameMs = millis() - nowMs;
        if (frameMs < 16) delay(16 - frameMs);
        if (elapsed >= TOTAL) break;
    }

    canvas.deleteSprite();
}

// Legacy stubs
void BootScreen::drawBackground(LGFX_Sprite& canvas, uint16_t w, uint16_t h) {}
void BootScreen::drawGlowCircle(LGFX_Sprite& canvas, float cx, float cy, float radius,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                int layers, uint16_t h_) {}
void BootScreen::drawWaves(LGFX_Sprite& canvas, float elapsed, uint16_t w, uint16_t h) {}
void BootScreen::drawProgressBar(LGFX_Sprite& canvas, float progress, uint16_t w, uint16_t h) {}