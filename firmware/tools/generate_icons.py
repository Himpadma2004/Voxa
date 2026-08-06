#!/usr/bin/env python3
"""
VOXA Icon Bitmap Generator
Generates crisp 20x20 monochrome bitmap arrays for ESP32 firmware.
Strategy: Draw each icon at 2x size (40x40) with anti-aliasing,
then scale down to 20x20 using LANCZOS for smooth edges, then threshold.
This mimics smartwatch icon generation pipelines.

Run: python tools/generate_icons.py
Output: src/ui/IconBitmaps.h
"""
from PIL import Image, ImageDraw
import math
import os

SIZE = 20          # Final icon size (pixels)
DRAW_SIZE = 40     # Drawing canvas size (2x for supersampling)
THRESHOLD = 128    # Binarize threshold (0-255)

OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "IconBitmaps.h")

# ── Helper ──────────────────────────────────────────────────────────────────
def make_canvas():
    return Image.new('L', (DRAW_SIZE, DRAW_SIZE), 255)

def downsample(img):
    """Supersampled downsample 40→20 with LANCZOS, then binarise."""
    small = img.resize((SIZE, SIZE), Image.LANCZOS)
    bw = small.point(lambda p: 0 if p < THRESHOLD else 255, '1')
    return bw

def img_to_c_array(img_bw, name):
    """Converts 1-bit PIL image to MSB-first, row-padded C uint8_t array."""
    pixels = list(img_bw.getdata()) if hasattr(img_bw, 'getdata') else list(img_bw.tobytes())
    # Flatten to list of pixel values
    try:
        pixels = list(img_bw.getdata())
    except Exception:
        pixels = list(img_bw.tobytes())
    w, h = img_bw.size
    bytes_per_row = (w + 7) // 8
    
    data = []
    for row in range(h):
        for byte_i in range(bytes_per_row):
            byte = 0
            for bit in range(8):
                px_x = byte_i * 8 + bit
                if px_x < w:
                    val = pixels[row * w + px_x]
                    # PIL mode '1': 0=black=ON, 255=white=OFF
                    if val == 0:
                        byte |= (1 << (7 - bit))
            data.append(byte)
    
    hex_vals = [f'0x{b:02X}' for b in data]
    grouped = [', '.join(hex_vals[i:i+10]) for i in range(0, len(hex_vals), 10)]
    body = ',\n    '.join(grouped)
    return (f"// {name}: {w}x{h}px, {bytes_per_row} bytes/row, {len(data)} total bytes\n"
            f"static const uint8_t {name}[{len(data)}] PROGMEM = {{\n"
            f"    {body}\n}};"), len(data)

# ── Icon Drawers (at 40x40) ───────────────────────────────────────────────
def draw_bell():
    """Minimal Bell: clean curved dome with rounded clapper"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    
    # Top ring
    d.ellipse([cx - 2, 2, cx + 2, 6], fill=0)
    # Bell dome & body using arc + polygon
    d.ellipse([cx - 12, 6, cx + 12, 24], fill=0)
    d.rectangle([cx - 12, 15, cx + 12, 27], fill=0)
    # Flared rim base
    d.rounded_rectangle([cx - 15, 27, cx + 15, 31], radius=2, fill=0)
    # Hanging clapper dot
    d.ellipse([cx - 3, 32, cx + 3, 37], fill=0)
    return img

def draw_lightbulb():
    """Modern Lightbulb: crisp bulb outline + center filament star cutout + screw base"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    
    # Outer bulb dome
    d.ellipse([cx - 12, 2, cx + 12, 24], fill=0)
    # Tapered neck block
    d.polygon([(cx - 8, 16), (cx + 8, 16), (cx + 5, 29), (cx - 5, 29)], fill=0)
    
    # Filament cutout inside bulb (white star glow cutout)
    d.line([cx, 8, cx, 18], fill=255, width=2)
    d.line([cx - 5, 13, cx + 5, 13], fill=255, width=2)
    
    # Screw base contacts
    d.rounded_rectangle([cx - 5, 29, cx + 5, 33], radius=1, fill=0)
    d.rounded_rectangle([cx - 3, 34, cx + 3, 37], radius=1, fill=0)
    return img

def draw_question():
    """Modern Question mark: clear bold curved hook + large centered dot"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    
    # Bold top arc loop using thick arc
    d.arc([cx - 11, 2, cx + 11, 20], start=180, end=380, fill=0, width=6)
    # Smooth curved diagonal stroke down to center
    d.line([cx + 7, 12, cx, 20], fill=0, width=6)
    # Vertical stem
    d.rounded_rectangle([cx - 3, 19, cx + 3, 26], radius=2, fill=0)
    # Crisp round dot
    d.ellipse([cx - 4, 30, cx + 4, 38], fill=0)
    return img




def draw_folder():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Tab
    d.rounded_rectangle([2, 8, 14, 16], radius=3, fill=0)
    # Body
    d.rounded_rectangle([2, 14, S-3, S-4], radius=4, fill=0)
    return img

def draw_gear():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    # Outer ring
    for r in range(13, 16):
        d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=0, width=1)
    # Fill the ring band
    d.ellipse([cx-15, cy-15, cx+15, cy+15], outline=0, width=4)
    # Center hub
    d.ellipse([cx-6, cy-6, cx+6, cy+6], fill=0)
    # 8 teeth (rounded rectangles at each angle)
    tooth_w = 6
    tooth_h = 10
    for i in range(8):
        angle = i * math.pi / 4
        tx = cx + int(15 * math.cos(angle))
        ty = cy + int(15 * math.sin(angle))
        d.ellipse([tx-5, ty-5, tx+5, ty+5], fill=0)
    return img

def draw_search():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Circle (bold annulus)
    d.ellipse([3, 3, 26, 26], outline=0, width=5)
    # Handle (thick line)
    d.line([22, 22, S-3, S-3], fill=0, width=6)
    return img

def draw_mic():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    # Mic capsule (rounded rectangle)
    d.rounded_rectangle([cx-7, 2, cx+7, 22], radius=6, fill=0)
    # Stand arc (bold)
    d.arc([cx-13, 12, cx+13, 32], start=0, end=180, fill=0, width=5)
    # Stand post
    d.rectangle([cx-2, 30, cx+2, 36], fill=0)
    # Base
    d.rounded_rectangle([cx-9, 35, cx+9, 39], radius=3, fill=0)
    return img

def draw_note():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Document body
    d.rounded_rectangle([4, 2, S-5, S-3], radius=3, fill=0)
    # Top-right fold ear (triangle cutout)
    d.polygon([(S-5-8, 2), (S-5, 2), (S-5, 10)], fill=255)
    # Fold crease line
    d.line([S-5-8, 2, S-5-8, 10, S-5, 10], fill=0, width=2)
    # Text lines (white cutouts)
    line_col = 255
    d.rectangle([9, 14, S-10, 16], fill=line_col)
    d.rectangle([9, 20, S-10, 22], fill=line_col)
    d.rectangle([9, 26, S-14, 28], fill=line_col)
    # Checkmark (tasks meaning)
    # (draw a tick in bottom-right)
    d.line([S-14, 30, S-11, 34, S-6, 26], fill=255, width=3)
    return img

def draw_chevron_right():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    cy = S // 2
    # Bold chevron >
    points_top = [(cx-4, 8), (cx+8, cy)]
    points_bottom = [(cx+8, cy), (cx-4, S-8)]
    d.line(points_top, fill=0, width=6)
    d.line(points_bottom, fill=0, width=6)
    return img

def draw_chevron_left():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    cy = S // 2
    # Bold chevron <
    points_top = [(cx+4, 8), (cx-8, cy)]
    points_bottom = [(cx-8, cy), (cx+4, S-8)]
    d.line(points_top, fill=0, width=6)
    d.line(points_bottom, fill=0, width=6)
    return img

def draw_wifi():
    """Ultra-clean premium Wi-Fi signal icon: 3 smooth solid arcs"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    cy_base = S - 3
    
    # Outer arc
    d.arc([cx - 17, cy_base - 31, cx + 17, cy_base + 3], start=215, end=325, fill=0, width=5)
    # Middle arc
    d.arc([cx - 11, cy_base - 21, cx + 11, cy_base + 1], start=215, end=325, fill=0, width=5)
    # Inner arc
    d.arc([cx - 5, cy_base - 11, cx + 5, cy_base - 1], start=215, end=325, fill=0, width=4)
    return img

def draw_bluetooth():
    """Ultra-crisp Nordic Bluetooth rune symbol with symmetric wings and top/bottom tails"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    
    # Spine line
    d.line([cx, 2, cx, S - 2], fill=0, width=4)
    # Upper & Lower diagonal wings forming the signature B rune
    # Top-left to mid-right, mid-right to mid-left (crossing center), mid-left to bot-right, bot-right to bottom-center
    d.line([(cx - 8, 10), (cx + 9, 18), (cx - 8, 28), (cx + 9, 36), (cx, 40)], fill=0, width=4)
    d.line([(cx, 0), (cx + 9, 8), (cx - 8, 18)], fill=0, width=4)
    return img




def draw_wifi_off():
    img = draw_wifi()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Red diagonal strike (represented as black in monochrome, rendered red in code)
    d.line([2, S-2, S-2, 2], fill=0, width=4)
    return img

def draw_battery():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Body
    d.rounded_rectangle([2, 10, S-6, S-10], radius=3, fill=0)
    # Terminal nub
    d.rectangle([S-6, 15, S-2, S-15], fill=0)
    # Inner (cutout for level indicator)
    d.rounded_rectangle([5, 13, S-10, S-13], radius=2, fill=255)
    # Fill bar (80% for icon)
    d.rounded_rectangle([5, 13, int((S-10)*0.80)+2, S-13], radius=2, fill=0)
    return img

def draw_cloud():
    """Cloud with circular sync arrows (for Sync & Backup icon) - like reference image"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    cy = S // 2
    # Outer circular arrows (ring arc with arrowheads)
    ring_r = 18
    # Top-right arc (going clockwise) ~270° to ~60°
    d.arc([cx-ring_r, cy-ring_r, cx+ring_r, cy+ring_r],
          start=250, end=80, fill=0, width=4)
    # Bottom-left arc (going clockwise) ~90° to ~240°
    d.arc([cx-ring_r, cy-ring_r, cx+ring_r, cy+ring_r],
          start=90, end=240, fill=0, width=4)
    # Arrowhead at top-right (at ~80°)
    a1 = math.radians(80)
    ax1 = int(cx + ring_r * math.cos(a1))
    ay1 = int(cy + ring_r * math.sin(a1))
    d.polygon([(ax1, ay1), (ax1-6, ay1-3), (ax1+1, ay1-7)], fill=0)
    # Arrowhead at bottom-left (at ~240°)
    a2 = math.radians(240)
    ax2 = int(cx + ring_r * math.cos(a2))
    ay2 = int(cy + ring_r * math.sin(a2))
    d.polygon([(ax2, ay2), (ax2+6, ay2+3), (ax2-1, ay2+7)], fill=0)
    # Cloud shape (smaller, centered)
    cloud_cx = cx
    cloud_cy = cy - 1
    # Cloud bumps
    d.ellipse([cloud_cx-10, cloud_cy-7, cloud_cx+2, cloud_cy+5], fill=0)
    d.ellipse([cloud_cx-3, cloud_cy-10, cloud_cx+9, cloud_cy+2], fill=0)
    d.ellipse([cloud_cx+2, cloud_cy-6, cloud_cx+12, cloud_cy+4], fill=0)
    # Cloud base
    d.rectangle([cloud_cx-9, cloud_cy-2, cloud_cx+11, cloud_cy+7], fill=0)
    return img

def draw_power():
    """Universal Power symbol: circle arc + top vertical line"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S // 2, S // 2
    
    # Outer arc (270 degrees open at top)
    d.arc([cx - 14, cy - 12, cx + 14, cy + 16], start=300, end=240, fill=0, width=5)
    # Vertical power bar in center top
    d.rounded_rectangle([cx - 2, 2, cx + 2, 18], radius=1, fill=0)
    return img

def draw_reset():
    """Factory Reset symbol: Warning Triangle with Exclamation mark"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    
    # Triangle boundary
    d.polygon([(cx, 2), (S - 3, S - 4), (3, S - 4)], fill=0)
    # Inner exclamation mark cutout (white cutout inside black triangle)
    d.rectangle([cx - 2, 12, cx + 2, 24], fill=255)
    d.ellipse([cx - 2, 28, cx + 2, 32], fill=255)
    return img

def draw_bluetooth():
    """Crisp Bluetooth rune symbol"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S // 2, S // 2
    
    # Spine vertical line
    d.line([cx, 4, cx, S - 4], fill=0, width=5)
    # Upper B wing
    d.line([cx, 4, cx + 10, 12, cx - 8, 22, cx + 10, 30, cx, S - 4], fill=0, width=4)
    return img

def draw_volume():
    """Speaker icon with sound wave lines"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S // 2, S // 2
    
    # Speaker cone
    d.polygon([(4, cy - 6), (12, cy - 6), (22, 6), (22, S - 6), (12, cy + 6), (4, cy + 6)], fill=0)
    # Sound arcs
    d.arc([cx + 2, cy - 10, cx + 14, cy + 10], start=300, end=60, fill=0, width=4)
    d.arc([cx + 6, cy - 16, cx + 20, cy + 16], start=300, end=60, fill=0, width=4)
    return img

def draw_sun():
    """Sun brightness symbol: central circle + 8 rays"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S // 2, S // 2
    
    r_sun = 9
    d.ellipse([cx - r_sun, cy - r_sun, cx + r_sun, cy + r_sun], fill=0)
    # 8 radiating rays
    for i in range(8):
        angle = math.radians(i * 45)
        x1 = int(cx + 12 * math.cos(angle))
        y1 = int(cy + 12 * math.sin(angle))
        x2 = int(cx + 17 * math.cos(angle))
        y2 = int(cy + 17 * math.sin(angle))
        d.line([x1, y1, x2, y2], fill=0, width=3)
    return img

def draw_moon():
    """Crescent moon night mode symbol"""
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S // 2, S // 2
    
    # Outer circle
    d.ellipse([cx - 15, cy - 15, cx + 15, cy + 15], fill=0)
    # Inner cutout to form crescent
    d.ellipse([cx - 8, cy - 18, cx + 22, cy + 12], fill=255)
    return img

def draw_rotate():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    # Arc (270 degrees)
    d.arc([cx-13, cy-13, cx+13, cy+13], start=60, end=330, fill=0, width=5)
    # Arrowhead at arc end
    end_angle = math.radians(60)
    ax = int(cx + 13 * math.cos(end_angle))
    ay = int(cy - 13 * math.sin(end_angle))
    # Arrow tip triangle
    d.polygon([(ax, ay), (ax+8, ay-4), (ax+5, ay+6)], fill=0)
    return img



def draw_play():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    # Bold play triangle
    d.polygon([(8, 6), (8, S-6), (S-5, cy)], fill=0)
    return img

def draw_pause():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cy = S // 2
    d.rounded_rectangle([6, 6, 14, S-6], radius=2, fill=0)
    d.rounded_rectangle([S-14, 6, S-6, S-6], radius=2, fill=0)
    return img

def draw_star():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    points = []
    for i in range(5):
        # Outer point
        a1 = math.radians(-90 + i * 72)
        points.append((cx + int(16 * math.cos(a1)), cy + int(16 * math.sin(a1))))
        # Inner point
        a2 = math.radians(-90 + i * 72 + 36)
        points.append((cx + int(7 * math.cos(a2)), cy + int(7 * math.sin(a2))))
    d.polygon(points, fill=0)
    return img

def draw_upload():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    # Arrow shaft
    d.rectangle([cx-3, 10, cx+3, S-6], fill=0)
    # Arrow triangle head
    d.polygon([(cx, 2), (cx-10, 14), (cx+10, 14)], fill=0)
    # Base line
    d.rounded_rectangle([4, S-6, S-5, S-2], radius=2, fill=0)
    return img

def draw_filter():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx = S // 2
    # Funnel triangle
    d.polygon([(3, 5), (S-4, 5), (cx, S//2+3)], fill=0)
    # Handle post
    d.rectangle([cx-3, S//2+2, cx+3, S-6], fill=0)
    return img

def draw_info():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    # Bold circle ring
    d.ellipse([2, 2, S-3, S-3], outline=0, width=4)
    # i dot
    d.ellipse([cx-3, 8, cx+3, 14], fill=0)
    # i stem
    d.rounded_rectangle([cx-3, 17, cx+3, 30], radius=2, fill=0)
    return img

def draw_storage():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # 3 stacked disk-like rounded rectangles
    for i in range(3):
        y = 3 + i * 13
        d.rounded_rectangle([3, y, S-4, y+8], radius=3, fill=0)
        # LED dot
        d.ellipse([S-9, y+2, S-5, y+6], fill=255)
    return img

def draw_calendar():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Body
    d.rounded_rectangle([2, 6, S-3, S-3], radius=3, fill=0)
    # Top header bar
    d.rounded_rectangle([2, 6, S-3, 14], radius=3, fill=0)
    # Hanger pins
    d.rounded_rectangle([8, 2, 12, 10], radius=2, fill=0)
    d.rounded_rectangle([S-13, 2, S-9, 10], radius=2, fill=0)
    # Day dots grid (white cutout)
    for row in range(2):
        for col in range(3):
            px = 6 + col * 10
            py = 17 + row * 9
            d.rectangle([px, py, px+5, py+5], fill=255)
    return img

def draw_chat():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    # Bubble body
    d.rounded_rectangle([2, 4, S-3, S-8], radius=5, fill=0)
    # Tail
    d.polygon([(6, S-8), (12, S-8), (5, S-2)], fill=0)
    # Text dots (white cutouts)
    for x in [9, 17, 25]:
        d.ellipse([x-2, 14, x+2, 18], fill=255)
    return img

def draw_spark():
    img = make_canvas()
    d = ImageDraw.Draw(img)
    S = DRAW_SIZE
    cx, cy = S//2, S//2
    # 4-pointed star / spark
    points = []
    for i in range(4):
        a_out = math.radians(i * 90 - 90)
        a_in1 = math.radians(i * 90 - 90 + 45)
        a_in2 = math.radians(i * 90 - 90 - 45)
        points.append((cx + int(18 * math.cos(a_out)), cy + int(18 * math.sin(a_out))))
        points.append((cx + int(5 * math.cos(a_in1)), cy + int(5 * math.sin(a_in1))))
    d.polygon(points, fill=0)
    return img

# ── Map icon name → draw function ────────────────────────────────────────
ICONS = {
    "ICON_BMP_BELL":          draw_bell,
    "ICON_BMP_LIGHTBULB":     draw_lightbulb,
    "ICON_BMP_QUESTION":      draw_question,
    "ICON_BMP_FOLDER":        draw_folder,
    "ICON_BMP_GEAR":          draw_gear,
    "ICON_BMP_SEARCH":        draw_search,
    "ICON_BMP_MIC":           draw_mic,
    "ICON_BMP_NOTE":          draw_note,
    "ICON_BMP_CHEVRON_RIGHT": draw_chevron_right,
    "ICON_BMP_CHEVRON_LEFT":  draw_chevron_left,
    "ICON_BMP_WIFI":          draw_wifi,
    "ICON_BMP_WIFI_OFF":      draw_wifi_off,
    "ICON_BMP_BATTERY":       draw_battery,
    "ICON_BMP_CLOUD":         draw_cloud,
    "ICON_BMP_ROTATE":        draw_rotate,
    "ICON_BMP_PLAY":          draw_play,
    "ICON_BMP_PAUSE":         draw_pause,
    "ICON_BMP_STAR":          draw_star,
    "ICON_BMP_UPLOAD":        draw_upload,
    "ICON_BMP_FILTER":        draw_filter,
    "ICON_BMP_INFO":          draw_info,
    "ICON_BMP_STORAGE":       draw_storage,
    "ICON_BMP_CALENDAR":      draw_calendar,
    "ICON_BMP_CHAT":          draw_chat,
    "ICON_BMP_SPARK":         draw_spark,
    "ICON_BMP_POWER":         draw_power,
    "ICON_BMP_RESET":         draw_reset,
    "ICON_BMP_BLUETOOTH":     draw_bluetooth,
    "ICON_BMP_VOLUME":        draw_volume,
    "ICON_BMP_SUN":           draw_sun,
    "ICON_BMP_MOON":          draw_moon,
}



# ── Main ─────────────────────────────────────────────────────────────────
def main():
    lines = [
        "// ════════════════════════════════════════════════════════════════════",
        "// IconBitmaps.h — Auto-generated by tools/generate_icons.py",
        "// DO NOT EDIT MANUALLY. Re-run generate_icons.py to regenerate.",
        "//",
        f"// Icon size: {SIZE}x{SIZE} px  |  Format: 1-bit mono MSB-first row-padded",
        "// Bytes per row: 3 (20 bits used, 4 bits padding per row)",
        "// Usage: canvas.drawBitmap(x, y, ICON_BMP_BELL, 20, 20, color);",
        "// ════════════════════════════════════════════════════════════════════",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"static constexpr int ICON_BMP_SIZE = {SIZE};",
        f"static constexpr int ICON_BMP_BYTES_PER_ROW = {(SIZE + 7) // 8};",
        f"static constexpr int ICON_BMP_TOTAL_BYTES = {SIZE * ((SIZE + 7) // 8)};",
        "",
    ]

    preview_dir = os.path.join(os.path.dirname(__file__), "icon_previews")
    os.makedirs(preview_dir, exist_ok=True)

    total = 0
    for name, draw_fn in ICONS.items():
        img_40 = draw_fn()
        img_20_bw = downsample(img_40)
        c_code, nbytes = img_to_c_array(img_20_bw, name)
        lines.append(c_code)
        lines.append("")
        total += nbytes
        # Save preview PNG (scaled up 8x for visibility)
        preview = img_20_bw.convert('RGB').resize((160, 160), Image.NEAREST)
        preview.save(os.path.join(preview_dir, f"{name}.png"))
        print(f"  [OK] {name:35s} {nbytes} bytes")

    lines.append(f"// Total icon data: {total} bytes in flash (PROGMEM)")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"\nGenerated: {OUTPUT_PATH}")
    print(f"Previews : {preview_dir}")
    print(f"Total    : {total} bytes in PROGMEM")

if __name__ == '__main__':
    print("VOXA Icon Bitmap Generator")
    print("=" * 40)
    main()
