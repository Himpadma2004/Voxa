// Standalone SD card hardware isolation test for VOXA ESP32-S3
// Three different software configurations, selectable via PlatformIO environment build flags:
// - TEST_VARIANT_IDF: ESP-IDF native esp_vfs_fat_sdspi_mount()
// - TEST_VARIANT_SDFAT: SdFat library at low frequency (100 kHz)
// - TEST_VARIANT_BITBANG: Hand-written bit-banged software SPI sequence with generous delays

#include <Arduino.h>
#include <SPI.h>

#define SD_CS   1
#define SD_MOSI 2
#define SD_MISO 13
#define SD_SCK  21

int totalAttempts = 0;
int successfulAttempts = 0;

// ============================================================================
// VARIANT 1: ESP-IDF Native esp_vfs_fat_sdspi_mount()
// ============================================================================
#if defined(TEST_VARIANT_IDF)
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

sdmmc_card_t *card = nullptr;
const char *mount_point = "/sdcard";

bool run_idf_test()
{
    Serial.println("[IDF] Setting up SPI bus for ESP-IDF...");
    
    // Explicitly configure pins & pull-ups
    gpio_reset_pin((gpio_num_t)SD_CS);
    gpio_reset_pin((gpio_num_t)SD_MOSI);
    gpio_reset_pin((gpio_num_t)SD_MISO);
    gpio_reset_pin((gpio_num_t)SD_SCK);

    gpio_pulldown_dis((gpio_num_t)SD_MISO);
    gpio_pullup_en((gpio_num_t)SD_MISO);
    pinMode(SD_MISO, INPUT_PULLUP);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(20);

    // Initialize SPI bus
    spi_bus_config_t bus_config;
    memset(&bus_config, 0, sizeof(bus_config));
    bus_config.mosi_io_num = SD_MOSI;
    bus_config.miso_io_num = SD_MISO;
    bus_config.sclk_io_num = SD_SCK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 4000;

    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        Serial.printf("[IDF] Failed to initialize SPI bus: 0x%X\n", err);
        return false;
    }

    // Host config
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    host.max_freq_khz = SDMMC_FREQ_PROBING; // Force probe speed (400 kHz) for safety

    // Device slot config
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)SD_CS;
    slot_config.host_id = SPI3_HOST;

    // Mount config
    esp_vfs_fat_mount_config_t mount_config;
    memset(&mount_config, 0, sizeof(mount_config));
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    Serial.println("[IDF] Calling esp_vfs_fat_sdspi_mount()...");
    err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (err != ESP_OK)
    {
        Serial.printf("[IDF] esp_vfs_fat_sdspi_mount failed: 0x%X\n", err);
        spi_bus_free(SPI3_HOST);
        return false;
    }

    Serial.println("[IDF] Mounted successfully!");
    Serial.printf("[IDF] Card Name: %s\n", card->cid.name);
    Serial.printf("[IDF] Sectors  : %u\n", card->csd.capacity);

    // Clean up
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    spi_bus_free(SPI3_HOST);
    return true;
}

// ============================================================================
// VARIANT 2: SdFat Library at Low Frequency (100 kHz)
// ============================================================================
#elif defined(TEST_VARIANT_SDFAT)
#include "SdFat.h"

SdFat sd;
SPIClass sdSPI(HSPI);

bool run_sdfat_test()
{
    Serial.println("[SdFat] Setting up SPI bus for SdFat...");
    
    // Explicitly configure pins & pull-ups
    gpio_reset_pin((gpio_num_t)SD_CS);
    gpio_reset_pin((gpio_num_t)SD_MOSI);
    gpio_reset_pin((gpio_num_t)SD_MISO);
    gpio_reset_pin((gpio_num_t)SD_SCK);

    gpio_pulldown_dis((gpio_num_t)SD_MISO);
    gpio_pullup_en((gpio_num_t)SD_MISO);
    pinMode(SD_MISO, INPUT_PULLUP);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(20);

    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Use SdSpiConfig to specify a dedicated low-speed (100 kHz) clock
    Serial.println("[SdFat] Calling sd.begin() at 100 kHz...");
    SdSpiConfig config(SD_CS, DEDICATED_SPI, 100000, &sdSPI);

    bool ok = sd.begin(config);
    if (!ok)
    {
        Serial.println("[SdFat] sd.begin() failed!");
        sdSPI.end();
        return false;
    }

    Serial.println("[SdFat] sd.begin() SUCCEEDED!");
    
    // Clean up
    sdSPI.end();
    return true;
}

// ============================================================================
// VARIANT 3: Software Bit-Banged SPI Mount sequence
// ============================================================================
#elif defined(TEST_VARIANT_BITBANG)

// Clock delay (microseconds per half-clock)
// 10 us half-clock delay = 50 kHz clock speed
const uint32_t HALF_CLOCK_DELAY_US = 10; 

uint8_t bitbang_spi_transfer(uint8_t out_byte)
{
    uint8_t in_byte = 0;
    for (int i = 0; i < 8; i++)
    {
        // 1. Output bit on MOSI (MSB first)
        digitalWrite(SD_MOSI, (out_byte & 0x80) ? HIGH : LOW);
        out_byte <<= 1;
        in_byte <<= 1;
        
        // 2. Wait half-clock period before rising edge
        delayMicroseconds(HALF_CLOCK_DELAY_US);
        digitalWrite(SD_SCK, HIGH);
        
        // 3. Settling delay and sample MISO
        delayMicroseconds(1);
        if (digitalRead(SD_MISO) == HIGH)
        {
            in_byte |= 0x01;
        }
        
        // 4. Wait remaining half-clock period
        delayMicroseconds(HALF_CLOCK_DELAY_US > 1 ? HALF_CLOCK_DELAY_US - 1 : 1);
        digitalWrite(SD_SCK, LOW);
    }
    return in_byte;
}

uint8_t bitbang_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    // Wait for card ready (MISO high)
    int timeout = 0;
    while (bitbang_spi_transfer(0xFF) != 0xFF)
    {
        if (++timeout > 2000) break;
    }

    // Send command header
    bitbang_spi_transfer(cmd | 0x40);
    bitbang_spi_transfer((arg >> 24) & 0xFF);
    bitbang_spi_transfer((arg >> 16) & 0xFF);
    bitbang_spi_transfer((arg >> 8) & 0xFF);
    bitbang_spi_transfer(arg & 0xFF);
    bitbang_spi_transfer(crc);

    // Read R1 response
    for (int i = 0; i < 64; i++)
    {
        uint8_t r1 = bitbang_spi_transfer(0xFF);
        if ((r1 & 0x80) == 0)
        {
            return r1;
        }
    }
    return 0xFF;
}

uint8_t bitbang_send_cmd_transaction(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    digitalWrite(SD_CS, LOW);
    bitbang_spi_transfer(0xFF);

    uint8_t r1 = bitbang_send_cmd(cmd, arg, crc);

    // Read additional response data for CMD8 or CMD58
    if (cmd == 8 || cmd == 58)
    {
        uint8_t resp[4];
        for (int i = 0; i < 4; i++)
        {
            resp[i] = bitbang_spi_transfer(0xFF);
        }
        if (cmd == 8)
        {
            Serial.printf("   [Bitbang] CMD8 response: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                          resp[0], resp[1], resp[2], resp[3]);
        }
    }

    digitalWrite(SD_CS, HIGH);
    bitbang_spi_transfer(0xFF);

    return r1;
}

bool run_bitbang_test()
{
    Serial.println("[Bitbang] Setting up bit-bang GPIOs...");
    
    gpio_reset_pin((gpio_num_t)SD_CS);
    gpio_reset_pin((gpio_num_t)SD_MOSI);
    gpio_reset_pin((gpio_num_t)SD_MISO);
    gpio_reset_pin((gpio_num_t)SD_SCK);

    gpio_pulldown_dis((gpio_num_t)SD_MISO);
    gpio_pullup_en((gpio_num_t)SD_MISO);
    pinMode(SD_MISO, INPUT_PULLUP);
    pinMode(SD_CS, OUTPUT);
    pinMode(SD_MOSI, OUTPUT);
    pinMode(SD_SCK, OUTPUT);

    digitalWrite(SD_CS, HIGH);
    digitalWrite(SD_MOSI, HIGH);
    digitalWrite(SD_SCK, LOW);
    delay(20);

    // 1. Send dummy clocks
    Serial.println("[Bitbang] Sending dummy clocks...");
    for (int i = 0; i < 15; i++)
    {
        bitbang_spi_transfer(0xFF);
    }
    delay(10);

    // 2. CMD0
    Serial.println("[Bitbang] Sending CMD0...");
    uint8_t r1 = bitbang_send_cmd_transaction(0, 0, 0x95);
    Serial.printf("[Bitbang] CMD0 R1: 0x%02X (Expected: 0x01)\n", r1);
    if (r1 != 0x01) return false;

    // 3. CMD8
    Serial.println("[Bitbang] Sending CMD8...");
    r1 = bitbang_send_cmd_transaction(8, 0x1AA, 0x87);
    Serial.printf("[Bitbang] CMD8 R1: 0x%02X\n", r1);

    // 4. ACMD41 loop
    Serial.println("[Bitbang] Running ACMD41 loop...");
    bool ready = false;
    for (int i = 0; i < 100; i++)
    {
        bitbang_send_cmd_transaction(55, 0, 0x65);
        uint8_t r41 = bitbang_send_cmd_transaction(41, 0x40000000, 0x77);
        if (r41 == 0x00)
        {
            ready = true;
            Serial.printf("   [Bitbang] Card ready at retry #%d\n", i);
            break;
        }
        delay(10);
    }

    if (!ready)
    {
        Serial.println("[Bitbang] ACMD41 loop timed out.");
        return false;
    }

    // 5. CMD16
    Serial.println("[Bitbang] Sending CMD16...");
    r1 = bitbang_send_cmd_transaction(16, 512, 0xFF);
    Serial.printf("[Bitbang] CMD16 R1: 0x%02X (Expected: 0x00)\n", r1);

    // 6. CMD17 Block 0 read
    Serial.println("[Bitbang] Sending CMD17...");
    digitalWrite(SD_CS, LOW);
    bitbang_spi_transfer(0xFF);

    r1 = bitbang_send_cmd(17, 0, 0xFF);
    Serial.printf("[Bitbang] CMD17 R1: 0x%02X\n", r1);
    if (r1 != 0x00)
    {
        digitalWrite(SD_CS, HIGH);
        bitbang_spi_transfer(0xFF);
        return false;
    }

    // Wait for data token
    bool token_found = false;
    uint8_t token = 0xFF;
    for (int i = 0; i < 5000; i++)
    {
        token = bitbang_spi_transfer(0xFF);
        if (token == 0xFE)
        {
            token_found = true;
            break;
        }
    }

    if (!token_found)
    {
        Serial.printf("[Bitbang] Data token 0xFE timeout, last byte: 0x%02X\n", token);
        digitalWrite(SD_CS, HIGH);
        bitbang_spi_transfer(0xFF);
        return false;
    }

    // Read 512 data bytes
    uint8_t buf[16];
    for (int i = 0; i < 512; i++)
    {
        uint8_t b = bitbang_spi_transfer(0xFF);
        if (i < 16) buf[i] = b;
    }

    // Read CRC
    uint8_t crc1 = bitbang_spi_transfer(0xFF);
    uint8_t crc2 = bitbang_spi_transfer(0xFF);

    digitalWrite(SD_CS, HIGH);
    bitbang_spi_transfer(0xFF);

    Serial.printf("[Bitbang] CMD17 success! CRC: 0x%02X%02X\n", crc1, crc2);
    Serial.print("   [Bitbang] Sector 0 content: ");
    for (int i = 0; i < 16; i++) Serial.printf("%02X ", buf[i]);
    Serial.println();

    return true;
}

#endif

// ============================================================================
// Standard Arduino Entry Points
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("========================================");
#if defined(TEST_VARIANT_IDF)
    Serial.println("  VARIANT 1: ESP-IDF NATIVE SDSPI MOUNT");
#elif defined(TEST_VARIANT_SDFAT)
    Serial.println("  VARIANT 2: SDFAT LIBRARY LOW SPEED (100kHz)");
#elif defined(TEST_VARIANT_BITBANG)
    Serial.println("  VARIANT 3: BIT-BANGED SOFTWARE SPI (50kHz)");
#else
    Serial.println("  ERROR: NO TEST VARIANT DEFINED!");
#endif
    Serial.printf("  Pins: CS=%d MOSI=%d MISO=%d SCK=%d\n", SD_CS, SD_MOSI, SD_MISO, SD_SCK);
    Serial.println("========================================");
}

void loop()
{
    totalAttempts++;
    Serial.printf("\n--- Test Attempt #%d (Running Tally: %d/%d Success) ---\n", 
                  totalAttempts, successfulAttempts, totalAttempts - 1);

    bool success = false;
#if defined(TEST_VARIANT_IDF)
    success = run_idf_test();
#elif defined(TEST_VARIANT_SDFAT)
    success = run_sdfat_test();
#elif defined(TEST_VARIANT_BITBANG)
    success = run_bitbang_test();
#endif

    if (success)
    {
        successfulAttempts++;
        Serial.println(">>> RESULT: SUCCESS");
    }
    else
    {
        Serial.println(">>> RESULT: FAIL");
    }

    // Clean up pins to high impedance state between tests
    pinMode(SD_CS, INPUT);
    pinMode(SD_MOSI, INPUT);
    pinMode(SD_MISO, INPUT);
    pinMode(SD_SCK, INPUT);

    delay(5000);
}
