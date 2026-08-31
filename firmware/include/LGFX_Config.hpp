#pragma once

// #define LGFX_USE_V1

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;

public:
    LGFX()
    {
        {
            auto cfg = _bus.config();

            cfg.spi_host = SPI2_HOST;

            cfg.spi_mode = 0;

            cfg.freq_write = 27000000;

            cfg.freq_read = 16000000;

            cfg.spi_3wire = false;

            cfg.use_lock = true;

            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = 12;

            cfg.pin_mosi = 11;

            cfg.pin_miso = -1;

            cfg.pin_dc = 9;

            _bus.config(cfg);

            _panel.setBus(&_bus);
        }

        {
            auto cfg = _panel.config();

            cfg.pin_cs = 10;

            cfg.pin_rst = 14;

            cfg.pin_busy = -1;

            cfg.memory_width = 240;

            cfg.memory_height = 320;

            cfg.panel_width = 240;

            cfg.panel_height = 320;

            cfg.offset_x = 0;

            cfg.offset_y = 0;

            cfg.offset_rotation = 0;

            cfg.dummy_read_pixel = 8;

            cfg.dummy_read_bits = 1;

            cfg.readable = false;

            cfg.invert = true;

            cfg.rgb_order = false;

            cfg.dlen_16bit = false;

            cfg.bus_shared = true;

            _panel.config(cfg);
        }

        {
            auto cfg = _light.config();
            cfg.pin_bl = 3;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            cfg.invert = false;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        setPanel(&_panel);
    }
};