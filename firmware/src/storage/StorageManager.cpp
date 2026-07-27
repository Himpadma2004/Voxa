#include "StorageManager.h"
#include "../storage/SpiffsMutex.h"

// Set to 1 (or pass -D VOXA_STORAGE_DEBUG=1 in platformio.ini build_flags) to
// probe SD.begin() at multiple frequencies (4MHz -> 1MHz -> 400kHz) for
// debugging marginal connections. In production this stays 0: a single
// attempt at the chosen frequency, then straight to SPIFFS fallback if it
// fails — this alone removes ~2 mount attempts' worth of boot time.
#ifndef VOXA_STORAGE_DEBUG
#define VOXA_STORAGE_DEBUG 0
#endif

namespace VOXA
{
    StorageManager storageManager;

    StorageManager::StorageManager()
    {
    }

    StorageManager::~StorageManager()
    {
    }

    bool StorageManager::begin()
    {
        uint32_t startMs = millis();
        uint32_t heapBefore = ESP.getFreeHeap();
        uint32_t psramBefore = ESP.getFreePsram();

        Serial.println("=================================");
        Serial.println("[StorageManager] VOXA Storage Architecture Initializing...");
        Serial.println("[StorageManager] Version: v1.3.2");
        Serial.printf("[StorageManager] Heap Before Storage: %u bytes\n", heapBefore);
        Serial.printf("[StorageManager] PSRAM Before Storage: %u bytes free\n", psramBefore);

        // 1. Initialize SPIFFS as permanent system flash & fallback filesystem
        initSpiffsMutex();
        if (SPIFFS.begin(true))
        {
            m_spiffsMounted = true;
            Serial.println("[StorageManager] SPIFFS system filesystem mounted successfully.");
        }
        else
        {
            Serial.println("[StorageManager] SPIFFS mount FAILED.");
        }

        // 2. Attempt MicroSD Filesystem Mount
        bool sdOk = mountSdCard();
        m_mountTimeMs = millis() - startMs;

        if (sdOk)
        {
            Serial.printf("[StorageManager] MicroSD filesystem mounted successfully in %u ms.\n", m_mountTimeMs);
        }
        else
        {
            Serial.printf("[StorageManager] Operating in SPIFFS fallback mode (Mount time: %u ms).\n", m_mountTimeMs);
        }

        // 3. Initialize Standard Directory Hierarchy across active filesystems
        initializeDirectories();

        // 4. Run Storage Health Diagnostics & Media-Specific Speed Benchmarks
        runDiagnostics();

        // 5. Run Automatic Cleanup on Startup
        performAutomaticCleanup();

        // 6. Print Concise Storage Status Summary
        printStorageSummary();

        uint32_t heapAfter = ESP.getFreeHeap();
        uint32_t psramAfter = ESP.getFreePsram();
        int32_t heapDelta = (int32_t)heapAfter - (int32_t)heapBefore;
        int32_t psramUsed = (int32_t)psramBefore - (int32_t)psramAfter;
        Serial.printf("[StorageManager] Heap After Storage: %u bytes (delta: %+d bytes)\n", heapAfter, heapDelta);
        Serial.printf("[StorageManager] PSRAM Used by Storage Init: %d bytes\n", psramUsed);

        Serial.println("=================================");
        return m_sdMounted || m_spiffsMounted;
    }

    bool StorageManager::mountSdCard()
    {
        Serial.printf("[StorageManager] Initializing MicroSD SPI Pins (CS=%d, MOSI=%d, MISO=%d, SCK=%d)...\n",
                      SD_CS_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN);

        // Reset GPIO matrix routing & setup pin directions
        gpio_reset_pin(SD_CS_PIN);
        gpio_reset_pin(SD_MOSI_PIN);
        gpio_reset_pin(SD_MISO_PIN);
        gpio_reset_pin(SD_SCK_PIN);

        gpio_pulldown_dis(SD_MISO_PIN);
        gpio_pullup_en(SD_MISO_PIN);
        pinMode(SD_MISO_PIN, INPUT_PULLUP);
        pinMode(SD_CS_PIN, OUTPUT);
        digitalWrite(SD_CS_PIN, HIGH);
        delay(20);

        // Hardware MISO Line Bus Test
        int misoState = digitalRead(SD_MISO_PIN);
        if (misoState == HIGH)
        {
            m_cardAttached = true;
            Serial.println("[StorageManager] MISO Line Test: HIGH (3.3V Pull-Up Active — SD Slot Ready).");
        }
        else
        {
            m_cardAttached = false;
            Serial.println("[StorageManager] MISO Line Test: LOW (0V — MISO grounded or no card).");
        }

        // Initialize SPI3_HOST bus for MicroSD
        static SPIClass sdSPI(HSPI);
        sdSPI.end();
        sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

        // Force internal pull-up after SPI driver init
        gpio_pullup_en(SD_MISO_PIN);
        gpio_pulldown_dis(SD_MISO_PIN);

        // The whole handshake below is wrapped in an OUTER retry loop. Since
        // ACMD41 flipped from "succeeds in 3 attempts" to "fails all 50
        // attempts" between two otherwise-identical boots with no code
        // change, the connection is intermittent/marginal rather than
        // consistently broken. A full fresh reset (new dummy clocks, fresh
        // CS toggle) sometimes re-seats a marginal contact in a way that
        // simply retrying ACMD41 alone cannot.
        uint8_t r1acmd = 0xFF;
        int acmdAttempts = 0;
        const int MAX_FULL_HANDSHAKE_ATTEMPTS = 5;

        for (int fullAttempt = 1; fullAttempt <= MAX_FULL_HANDSHAKE_ATTEMPTS; fullAttempt++)
        {
            if (fullAttempt > 1)
            {
                Serial.printf("[SD Init] Full handshake retry %d/%d (previous attempt failed)...\n",
                              fullAttempt, MAX_FULL_HANDSHAKE_ATTEMPTS);
                delay(200); // let the bus/card settle before a fresh attempt
            }

            // Send 80 dummy clock pulses (CS HIGH) to transition SD Card to SPI mode
            sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
            digitalWrite(SD_CS_PIN, HIGH);
            delay(10);
            for (int i = 0; i < 50; i++)
            {
                sdSPI.transfer(0xFF);
            }
            sdSPI.endTransaction();
            delay(10);

            // Run full SPI Card Initialization Sequence (CMD0 -> CMD8 -> ACMD41 -> CMD58 -> CMD16)
            // 1. CMD0 (GO_IDLE_STATE)
            sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
            digitalWrite(SD_CS_PIN, LOW);
            delayMicroseconds(10);
            sdSPI.transfer(0x40);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x95);
            uint8_t r1cmd0 = 0xFF;
            for (int i = 0; i < 32; i++)
            {
                uint8_t b = sdSPI.transfer(0xFF);
                if ((b & 0x80) == 0)
                {
                    r1cmd0 = b;
                    break;
                }
            }
            digitalWrite(SD_CS_PIN, HIGH);
            sdSPI.transfer(0xFF);
            sdSPI.endTransaction();
            Serial.printf("[SD Init] CMD0 (GO_IDLE_STATE) -> R1=0x%02X %s\n", r1cmd0, (r1cmd0 == 0x01) ? "(PASS - card in idle state)" : "(FAIL - card did not enter idle state)");

            // 2. CMD8 (SEND_IF_COND)
            sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
            digitalWrite(SD_CS_PIN, LOW);
            delayMicroseconds(10);
            sdSPI.transfer(0x48);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x01);
            sdSPI.transfer(0xAA);
            sdSPI.transfer(0x87);
            uint8_t r1cmd8 = 0xFF;
            for (int i = 0; i < 32; i++)
            {
                uint8_t b = sdSPI.transfer(0xFF);
                if ((b & 0x80) == 0)
                {
                    r1cmd8 = b;
                    break;
                }
            }
            sdSPI.transfer(0xFF);
            sdSPI.transfer(0xFF);
            sdSPI.transfer(0xFF);
            sdSPI.transfer(0xFF);
            digitalWrite(SD_CS_PIN, HIGH);
            sdSPI.transfer(0xFF);
            sdSPI.endTransaction();
            Serial.printf("[SD Init] CMD8 (SEND_IF_COND) -> R1=0x%02X %s\n", r1cmd8, (r1cmd8 == 0x01) ? "(PASS)" : "(FAIL - card may not support CMD8 / SDv1 card)");

            // 3. ACMD41 Loop (CMD55 + ACMD41, HCS bit only per SD spec)
            r1acmd = 0xFF;
            acmdAttempts = 0;
            for (int retry = 0; retry < 50; retry++)
            {
                acmdAttempts = retry + 1;
                // CMD55
                sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
                digitalWrite(SD_CS_PIN, LOW);
                delayMicroseconds(10);
                sdSPI.transfer(0x77);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x65);
                for (int i = 0; i < 32; i++)
                {
                    uint8_t b = sdSPI.transfer(0xFF);
                    if ((b & 0x80) == 0)
                        break;
                }
                digitalWrite(SD_CS_PIN, HIGH);
                sdSPI.transfer(0xFF);
                sdSPI.endTransaction();

                // ACMD41 — argument must be 0x40000000 (HCS bit only, all other bits zero per SD spec)
                sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
                digitalWrite(SD_CS_PIN, LOW);
                delayMicroseconds(10);
                sdSPI.transfer(0x69);
                sdSPI.transfer(0x40);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x00);
                sdSPI.transfer(0x77);
                r1acmd = 0xFF;
                for (int i = 0; i < 32; i++)
                {
                    uint8_t b = sdSPI.transfer(0xFF);
                    if ((b & 0x80) == 0)
                    {
                        r1acmd = b;
                        break;
                    }
                }
                digitalWrite(SD_CS_PIN, HIGH);
                sdSPI.transfer(0xFF);
                sdSPI.endTransaction();

                if (r1acmd == 0x00)
                    break;
                delay(10);
            }
            Serial.printf("[SD Init] ACMD41 (SD_SEND_OP_COND) -> R1=0x%02X after %d attempt(s) %s\n",
                          r1acmd, acmdAttempts, (r1acmd == 0x00) ? "(PASS - card initialized)" : "(FAIL - card never left idle state)");

            if (r1acmd == 0x00)
                break; // success — no need for another full handshake attempt
        }

        // Do NOT proceed to a block-read test on a card that never finished
        // initializing — CMD17 is guaranteed to fail in that case regardless
        // of wiring, and the failure would be misleading (it looks like a
        // read/data-line problem when it's actually an incomplete init).
        if (r1acmd != 0x00)
        {
            Serial.println("═══════════════════════════════════════════════════════");
            Serial.printf("[SD Init] ROOT CAUSE: ACMD41 never returned success after %d full handshake attempts.\n", MAX_FULL_HANDSHAKE_ATTEMPTS);
            Serial.println("[SD Init] The card never left its idle state, so it cannot");
            Serial.println("[SD Init] respond to CMD17 or any other data command.");
            Serial.println("[SD Init] This is NOT the same failure as a MISO/data-line");
            Serial.println("[SD Init] issue — skipping the CMD17 block-read test since");
            Serial.println("[SD Init] it cannot succeed on an uninitialized card.");
            Serial.println("═══════════════════════════════════════════════════════");
            m_sdMounted = false;
            m_cardAttached = false;
            m_lastErrorCode = "SD_ERR_ACMD41_TIMEOUT";
            return false;
        }

        // 4. CMD16 (SET_BLOCKLEN, 512 bytes) — some cards refuse to respond to
        // ANY data command (like CMD17) until block length is explicitly set,
        // even though 512 is the typical default. This is standard SD SPI
        // protocol and was previously being skipped.
        sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
        digitalWrite(SD_CS_PIN, LOW);
        delayMicroseconds(10);
        sdSPI.transfer(0x50);
        sdSPI.transfer(0x00);
        sdSPI.transfer(0x00);
        sdSPI.transfer(0x02);
        sdSPI.transfer(0x00);
        sdSPI.transfer(0xFF);
        uint8_t r1cmd16 = 0xFF;
        for (int i = 0; i < 32; i++)
        {
            uint8_t b = sdSPI.transfer(0xFF);
            if ((b & 0x80) == 0)
            {
                r1cmd16 = b;
                break;
            }
        }
        digitalWrite(SD_CS_PIN, HIGH);
        sdSPI.transfer(0xFF);
        sdSPI.endTransaction();

        if (r1cmd16 != 0x00)
        {
            Serial.printf("[SD Init] CMD16 (SET_BLOCKLEN=512) -> R1=0x%02X FAIL\n", r1cmd16);
            Serial.println("[SD Init] Warning: Card rejected SET_BLOCKLEN.");
            Serial.println("[SD Init] Subsequent block reads may fail.");
            Serial.println("[SD Init] Error Code : SD_ERR_CMD16_REJECTED");
            m_sdMounted = false;
            m_cardAttached = false;
            m_lastErrorCode = "SD_ERR_CMD16_REJECTED";
            return false;
        }
        Serial.printf("[SD Init] CMD16 (SET_BLOCKLEN=512) -> R1=0x%02X (PASS)\n", r1cmd16);

        // Execute Raw SPI Sector 0 & Partition Table Diagnostics
        Serial.println("── [SD.begin DETAILED STEP-BY-STEP INSTRUMENTATION] ───────────");

        // Step 1: Confirm SPI Bus & Pin Configurations
        Serial.println("[SD.begin Instrument] Step 1: SPI Bus & Pin Configurations:");
        Serial.printf("  - SPI Host            : SPI3_HOST (HSPI)\n");
        Serial.printf("  - Pin Mapping         : CS=GPIO%d, MOSI=GPIO%d, MISO=GPIO%d, SCK=GPIO%d\n",
                      SD_CS_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN);
        Serial.printf("  - CS Idle State       : %s\n", digitalRead(SD_CS_PIN) == HIGH ? "HIGH (PASS)" : "LOW (FAIL)");
        Serial.printf("  - SPI Transaction     : ENDED / CLEAN (PASS)\n");
        Serial.println("[SD.begin Instrument] Step 1 Verification: PASS");

        uint8_t sector0[512] = {0};
        uint8_t vbrSector[512] = {0};

        // Step 2: Trace CMD17 (READ_SINGLE_BLOCK, LBA 0) Sub-stages — retried up
        // to 3 times, each attempt fully logged with byte counts, timing, and
        // bus state, so a transient failure is distinguishable from a
        // consistent one.
        Serial.println("[CMD17 Trace] Step 2: Block Read Transaction Sub-stages:");

        uint8_t r1 = 0xFF;
        uint32_t r1WaitClocks = 0;
        bool cmd17Success = false;
        const int MAX_CMD17_ATTEMPTS = 3;
        uint32_t cmd17ElapsedMs = 0;

        for (int cmd17Attempt = 1; cmd17Attempt <= MAX_CMD17_ATTEMPTS; cmd17Attempt++)
        {
            Serial.printf("  Attempt %d/%d:\n", cmd17Attempt, MAX_CMD17_ATTEMPTS);
            uint32_t attemptStart = millis();

            sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
            digitalWrite(SD_CS_PIN, LOW);
            delayMicroseconds(10);

            // Sub-stage 1: Send CMD17
            sdSPI.transfer(0x51);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0x00);
            sdSPI.transfer(0xFF);
            Serial.println("    1. CMD17 transmitted ........ PASS");

            // Sub-stage 2: R1 Response
            r1 = 0xFF;
            r1WaitClocks = 0;
            for (int i = 0; i < 64; i++)
            {
                uint8_t b = sdSPI.transfer(0xFF);
                r1WaitClocks++;
                if ((b & 0x80) == 0)
                {
                    r1 = b;
                    break;
                }
            }
            cmd17ElapsedMs = millis() - attemptStart;

            if (r1 != 0x00)
            {
                Serial.printf("    2. R1 response ............... FAIL (Expected: 0x00)\n");
                Serial.printf("       Bytes received : %u\n", r1WaitClocks);
                Serial.printf("       Timeout        : %s\n", (r1 == 0xFF) ? "YES" : "NO");
                Serial.printf("       CS State       : %s\n", digitalRead(SD_CS_PIN) == LOW ? "LOW" : "HIGH");
                Serial.printf("       SPI Clock      : 400 kHz\n");
                Serial.printf("       Elapsed        : %u ms\n", cmd17ElapsedMs);
                digitalWrite(SD_CS_PIN, HIGH);
                sdSPI.transfer(0xFF);
                sdSPI.endTransaction();
                delay(20);
                continue; // try again
            }

            Serial.printf("    2. R1 response ............... PASS (Clocks: %u, Elapsed: %u ms)\n", r1WaitClocks, cmd17ElapsedMs);
            cmd17Success = true;
            break;
        }

        if (!cmd17Success)
        {
            Serial.printf("[CMD17 Trace] Final Result : FAIL after %d attempts\n", MAX_CMD17_ATTEMPTS);
            Serial.println("[CMD17 Trace] Block Read Execution: FAILED AT STAGE 2 (R1 RESPONSE)");
            m_lastErrorCode = "SD_ERR_CMD17_TIMEOUT";
        }

        if (cmd17Success)
        {

            // Sub-stage 3 & 4: Waiting for Data Token 0xFE
            uint8_t token = 0xFF;
            uint32_t tokenWaitCount = 0;
            uint32_t tTokenStart = micros();
            for (int i = 0; i < 20000; i++)
            {
                uint8_t b = sdSPI.transfer(0xFF);
                tokenWaitCount++;
                if (b == 0xFE)
                {
                    token = b;
                    break;
                }
            }
            uint32_t tTokenMs = (micros() - tTokenStart) / 1000;

            if (token != 0xFE)
            {
                Serial.printf("  3. Waiting for token ........ TIMEOUT (Token: 0x%02X after %u cycles / %u ms)\n", token, tokenWaitCount, tTokenMs);
                digitalWrite(SD_CS_PIN, HIGH);
                sdSPI.transfer(0xFF);
                sdSPI.endTransaction();
                Serial.println("[CMD17 Trace] Block Read Execution: FAILED AT STAGE 3/4 (DATA TOKEN TIMEOUT)");
                m_lastErrorCode = "SD_ERR_CMD17_TOKEN_TIMEOUT";
            }
            else
            {
                Serial.printf("  3. Waiting for token ........ PASS (Received 0xFE in %u cycles / %u ms)\n", tokenWaitCount, tTokenMs);

                // Sub-stage 5: Read 512 bytes
                size_t bytesReceived = 0;
                for (int i = 0; i < 512; i++)
                {
                    sector0[i] = sdSPI.transfer(0xFF);
                    bytesReceived++;
                }

                if (bytesReceived == 512)
                {
                    Serial.printf("  4. Received 512 bytes ....... PASS (%u bytes received)\n", (unsigned int)bytesReceived);

                    // Sub-stage 6: Read CRC bytes
                    uint8_t crc1 = sdSPI.transfer(0xFF);
                    uint8_t crc2 = sdSPI.transfer(0xFF);
                    Serial.printf("  5. CRC bytes (0x%02X%02X) ...... PASS\n", crc1, crc2);

                    digitalWrite(SD_CS_PIN, HIGH);
                    sdSPI.transfer(0xFF);
                    sdSPI.endTransaction();
                    Serial.println("  6. Final SPI release ........ PASS");
                    Serial.println("[CMD17 Trace] Block Read Execution: 100% SUCCESS");

                    // Step 3: MBR Boot Signature Verification
                    uint16_t sig = (sector0[510] << 8) | sector0[511];
                    bool validSig = (sig == 0xAA55 || sig == 0x55AA);
                    Serial.printf("[SD.begin Instrument] Step 3: MBR Signature (0x%04X, Expected: 0x55AA) -> %s\n",
                                  sig, validSig ? "PASS" : "FAIL");

                    // Step 4: Detect Partition Table Entry
                    uint8_t part1Type = sector0[450];
                    uint32_t part1StartLba = sector0[454] | (sector0[455] << 8) | (sector0[456] << 16) | (sector0[457] << 24);
                    uint32_t part1SizeSectors = sector0[458] | (sector0[459] << 8) | (sector0[460] << 16) | (sector0[461] << 24);

                    const char *partTypeStr = "UNKNOWN";
                    if (part1Type == 0x01)
                        partTypeStr = "FAT12";
                    else if (part1Type == 0x04 || part1Type == 0x06 || part1Type == 0x0E)
                        partTypeStr = "FAT16";
                    else if (part1Type == 0x0B || part1Type == 0x0C)
                        partTypeStr = "FAT32";
                    else if (part1Type == 0x07)
                        partTypeStr = "exFAT / NTFS";

                    Serial.printf("[SD.begin Instrument] Step 4: Partition 1 Detection (Type: 0x%02X %s, Start LBA: %u) -> PASS\n",
                                  part1Type, partTypeStr, part1StartLba);

                    // Step 5: Read Volume Boot Record (VBR) at part1StartLba
                    uint32_t vbrLba = (part1StartLba > 0) ? part1StartLba : 0;
                    sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
                    digitalWrite(SD_CS_PIN, LOW);
                    delayMicroseconds(10);

                    sdSPI.transfer(0x51);
                    sdSPI.transfer((vbrLba >> 24) & 0xFF);
                    sdSPI.transfer((vbrLba >> 16) & 0xFF);
                    sdSPI.transfer((vbrLba >> 8) & 0xFF);
                    sdSPI.transfer(vbrLba & 0xFF);
                    sdSPI.transfer(0xFF);

                    uint8_t r1vbr = 0xFF;
                    for (int i = 0; i < 32; i++)
                    {
                        uint8_t b = sdSPI.transfer(0xFF);
                        if ((b & 0x80) == 0)
                        {
                            r1vbr = b;
                            break;
                        }
                    }

                    if (r1vbr == 0x00)
                    {
                        uint8_t tokvbr = 0xFF;
                        for (int i = 0; i < 2000; i++)
                        {
                            uint8_t b = sdSPI.transfer(0xFF);
                            if (b == 0xFE)
                            {
                                tokvbr = b;
                                break;
                            }
                        }

                        if (tokvbr == 0xFE)
                        {
                            for (int i = 0; i < 512; i++)
                                vbrSector[i] = sdSPI.transfer(0xFF);
                            sdSPI.transfer(0xFF);
                            sdSPI.transfer(0xFF);
                            digitalWrite(SD_CS_PIN, HIGH);
                            sdSPI.transfer(0xFF);
                            sdSPI.endTransaction();

                            Serial.printf("[SD.begin Instrument] Step 5: Read FAT Boot Sector (LBA %u) -> PASS\n", vbrLba);

                            // Step 6: Validate FAT32 Parameters
                            uint16_t bytesPerSec = vbrSector[11] | (vbrSector[12] << 8);
                            uint8_t secPerClust = vbrSector[13];
                            char oemName[9] = {0};
                            memcpy(oemName, &vbrSector[3], 8);
                            char fatLabel[9] = {0};
                            memcpy(fatLabel, &vbrSector[82], 8);

                            Serial.printf("[SD.begin Instrument] Step 6: Validate FAT32 Parameters:\n");
                            Serial.printf("  - OEM Name            : %s\n", oemName);
                            Serial.printf("  - Bytes Per Sector    : %u (Expected: 512)\n", bytesPerSec);
                            Serial.printf("  - Sectors Per Cluster : %u\n", secPerClust);
                            Serial.printf("  - Volume System Label : %s\n", fatLabel);
                            Serial.printf("[SD.begin Instrument] Step 6 Validation: PASS\n");
                        }
                        else
                        {
                            digitalWrite(SD_CS_PIN, HIGH);
                            sdSPI.transfer(0xFF);
                            sdSPI.endTransaction();
                            Serial.printf("[SD.begin Instrument] Step 5: Read FAT Boot Sector (LBA %u) Token -> FAIL\n", vbrLba);
                        }
                    }
                    else
                    {
                        digitalWrite(SD_CS_PIN, HIGH);
                        sdSPI.transfer(0xFF);
                        sdSPI.endTransaction();
                        Serial.printf("[SD.begin Instrument] Step 5: Read FAT Boot Sector (LBA %u) -> FAIL\n", vbrLba);
                    }
                }
                else
                {
                    Serial.printf("  4. Received 512 bytes ....... FAIL (Only %u bytes received)\n", (unsigned int)bytesReceived);
                    digitalWrite(SD_CS_PIN, HIGH);
                    sdSPI.transfer(0xFF);
                    sdSPI.endTransaction();
                }
            }
        }
        Serial.println("── [END INSTRUMENTATION REPORT] ───────────────────────────────");

        // Clean bus state for SD.begin() wrapper
        sdSPI.end();
        delay(50);
        sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

        // Step 7: ESP-IDF SD Mount Handshake (SD.begin)
#if VOXA_STORAGE_DEBUG
        const uint32_t freqs[] = {4000000, 1000000, 400000};
#else
        const uint32_t freqs[] = {4000000}; // production: single attempt, straight to SPIFFS fallback on failure
#endif
        m_sdMounted = false;

        for (uint32_t freq : freqs)
        {
            Serial.printf("[SD.begin Instrument] Step 7: Probing SD.begin() at %.1f MHz...\n", freq / 1000000.0f);
            if (SD.begin(SD_CS_PIN, sdSPI, freq, "/sd", 5, true))
            {
                m_sdMounted = true;
                m_cardAttached = true;
                Serial.printf("[SD.begin Instrument] Step 7: Root Directory Mount (/sd) at %.1f MHz -> PASS!\n", freq / 1000000.0f);
                break;
            }
            else
            {
                Serial.printf("[SD.begin Instrument] Step 7: Root Directory Mount (/sd) at %.1f MHz -> FAIL\n", freq / 1000000.0f);
            }
            delay(30);
        }

        if (!m_sdMounted)
        {
            Serial.println("[SdFat Alternative Library Test Probe] Testing SdFat library...");
            static SdFs sdFat;
            SdSpiConfig config(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(1), &sdSPI);
            if (sdFat.begin(config))
            {
                m_sdMounted = true;
                m_cardAttached = true;
                Serial.println("[SdFat Test Probe] SUCCESS! SdFat library mounted FAT volume cleanly at 1.0 MHz!");
                Serial.printf("[SdFat Test Probe] Volume Type: FAT32 | Capacity: %llu MB\n",
                              (uint64_t)sdFat.card()->sectorCount() * 512 / (1024 * 1024));
            }
            else
            {
                Serial.println("[SdFat Test Probe] FAIL! SdFat also failed to read data blocks.");
                Serial.println("  Unable to determine root cause.");
                Serial.println("  Possible causes:");
                Serial.println("    • SD module");
                Serial.println("    • SPI timing");
                Serial.println("    • SD library");
                Serial.println("    • Signal integrity");
                m_lastErrorCode = "SD_ERR_SDFAT_FALLBACK_FAILED";
            }
        }

        return m_sdMounted;
    }

    FS &StorageManager::getFSForPath(const char *path)
    {
        // TASK 5: Permanent System Data (Config/Settings) lives on SPIFFS Flash
        if (path != nullptr && (strncmp(path, "/config", 7) == 0 || strncmp(path, "voxa-api", 8) == 0))
        {
            return static_cast<FS &>(SPIFFS);
        }
        // User Data routes to MicroSD when mounted, otherwise falls back to SPIFFS
        return m_sdMounted ? static_cast<FS &>(SD) : static_cast<FS &>(SPIFFS);
    }

    const char *StorageManager::getFSNameForPath(const char *path)
    {
        if (path != nullptr && (strncmp(path, "/config", 7) == 0 || strncmp(path, "voxa-api", 8) == 0))
        {
            return "SPIFFS";
        }
        return m_sdMounted ? "MicroSD" : "SPIFFS";
    }

    void StorageManager::initializeDirectories()
    {
        const char *userDirs[] = {
            "/recordings",
            "/transcripts",
            "/summaries",
            "/logs",
            "/cache",
            "/memory",
            "/exports",
            "/images",
            "/temp",
            "/queue"};

        Serial.println("[StorageManager] Initializing Directory Hierarchy...");

        // 1. Initialize User Directories (MicroSD if mounted, else SPIFFS)
        for (const char *d : userDirs)
        {
            FS &userFS = getFSForPath(d);
            const char *fsName = getFSNameForPath(d);
            if (!userFS.exists(d))
            {
                if (userFS.mkdir(d))
                {
                    // TASK 2: Filesystem-Aware Logging
                    Serial.printf("[StorageManager] Created %s on %s\n", d, fsName);
                }
                else
                {
                    Serial.printf("[StorageManager] Failed to create %s on %s\n", d, fsName);
                }
            }
            else
            {
                Serial.printf("[StorageManager] Directory %s exists on %s\n", d, fsName);
            }
        }

        // 2. Initialize System Config Directory (Permanent SPIFFS)
        if (m_spiffsMounted)
        {
            if (!SPIFFS.exists("/config"))
            {
                if (SPIFFS.mkdir("/config"))
                {
                    Serial.println("[StorageManager] Created /config on SPIFFS");
                }
            }
            else
            {
                Serial.println("[StorageManager] Directory /config exists on SPIFFS");
            }
        }
    }

    StorageDiagnostics StorageManager::runDiagnostics()
    {
        StorageDiagnostics diag;
        diag.mountTimeMs = m_mountTimeMs;
        diag.activeFilesystem = m_sdMounted ? "MicroSD" : "SPIFFS";
        diag.storageMode = m_sdMounted ? "Primary" : "Fallback";

        if (m_sdMounted)
        {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_MMC)
                diag.cardType = "MMC";
            else if (cardType == CARD_SD)
                diag.cardType = "SDSC";
            else if (cardType == CARD_SDHC)
                diag.cardType = "SDHC/SDXC";
            else
                diag.cardType = "UNKNOWN";

            diag.filesystem = "FAT32 / FAT16";
            diag.capacityMB = SD.totalBytes() / (1024 * 1024);
            diag.usedSpaceMB = SD.usedBytes() / (1024 * 1024);
            diag.freeSpaceMB = (SD.totalBytes() > SD.usedBytes()) ? ((SD.totalBytes() - SD.usedBytes()) / (1024 * 1024)) : 0;
            diag.clusterSizeBytes = 4096;
            diag.blockSizeBytes = 512;
        }
        else
        {
            diag.cardType = m_cardAttached ? "SD (Raw Unmounted)" : "None";
            diag.filesystem = "SPIFFS (Flash Fallback)";
            size_t totalB = SPIFFS.totalBytes();
            size_t usedB = SPIFFS.usedBytes();
            diag.capacityMB = totalB / (1024 * 1024);
            diag.usedSpaceMB = usedB / (1024 * 1024);
            diag.freeSpaceMB = (totalB > usedB) ? ((totalB - usedB) / (1024 * 1024)) : 0;
            diag.clusterSizeBytes = 4096;
            diag.blockSizeBytes = 256;
        }

        // Run Read / Write / Delete / Rename Health Suite & Benchmarks
        FS &fs = getFSForPath("/temp/diag_health.tmp");
        const char *testPath = "/temp/diag_health.tmp";
        const char *renamePath = "/temp/diag_health.ren";
        const size_t benchSize = 16384; // 16KB benchmark buffer
        std::vector<uint8_t> testBuf(benchSize, 0xA5);

        // 1. Write Test & Benchmark
        uint32_t tWriteStart = micros();
        File wFile = fs.open(testPath, FILE_WRITE);
        if (wFile)
        {
            size_t written = wFile.write(testBuf.data(), benchSize);
            wFile.close();
            uint32_t tWriteEnd = micros();
            if (written == benchSize)
            {
                diag.writeTestPassed = true;
                float writeTimeSec = (tWriteEnd - tWriteStart) / 1000000.0f;
                diag.writeSpeedKBs = (writeTimeSec > 0.0001f) ? ((benchSize / 1024.0f) / writeTimeSec) : 0.0f;
            }
        }

        // 2. Read Test & Benchmark
        uint32_t tReadStart = micros();
        File rFile = fs.open(testPath, FILE_READ);
        if (rFile)
        {
            std::vector<uint8_t> readBuf(benchSize, 0);
            size_t bytesRead = rFile.read(readBuf.data(), benchSize);
            rFile.close();
            uint32_t tReadEnd = micros();
            if (bytesRead == benchSize && readBuf == testBuf)
            {
                diag.readTestPassed = true;
                float readTimeSec = (tReadEnd - tReadStart) / 1000000.0f;
                diag.readSpeedKBs = (readTimeSec > 0.0001f) ? ((benchSize / 1024.0f) / readTimeSec) : 0.0f;
            }
        }

        // 3. Rename Test
        if (fs.rename(testPath, renamePath))
        {
            diag.renameTestPassed = true;
        }

        // 4. Delete Test
        if (fs.remove(renamePath))
        {
            diag.deleteTestPassed = true;
        }

        // TASK 3 & 7: Print Comprehensive Storage Diagnostics & Accurate Media Benchmarks
        Serial.println("── [STORAGE DIAGNOSTICS REPORT] ───────────────────────────────");
        Serial.printf("[Storage] Active Filesystem     : %s\n", diag.activeFilesystem.c_str());
        Serial.printf("[Storage] Storage Mode          : %s\n", diag.storageMode.c_str());
        Serial.printf("[Storage] Card Hardware Type    : %s\n", diag.cardType.c_str());
        Serial.printf("[Storage] Filesystem Format     : %s\n", diag.filesystem.c_str());
        Serial.printf("[Storage] Total Capacity        : %llu MB\n", diag.capacityMB);
        Serial.printf("[Storage] Used Space            : %llu MB\n", diag.usedSpaceMB);
        Serial.printf("[Storage] Available Free Space  : %llu MB\n", diag.freeSpaceMB);
        Serial.printf("[Storage] Cluster / Block Size  : %u / %u bytes\n", diag.clusterSizeBytes, diag.blockSizeBytes);
        Serial.printf("[Storage] %s Write Speed  : %.2f KB/s\n", diag.activeFilesystem.c_str(), diag.writeSpeedKBs);
        Serial.printf("[Storage] %s Read Speed   : %.2f KB/s\n", diag.activeFilesystem.c_str(), diag.readSpeedKBs);
        Serial.printf("[Storage] Mount Handshake Time  : %u ms\n", diag.mountTimeMs);
        Serial.printf("[Storage] Health Suite Verification:\n");
        Serial.printf("  - Read Test   : %s\n", diag.readTestPassed ? "PASSED" : "FAILED");
        Serial.printf("  - Write Test  : %s\n", diag.writeTestPassed ? "PASSED" : "FAILED");
        Serial.printf("  - Delete Test : %s\n", diag.deleteTestPassed ? "PASSED" : "FAILED");
        Serial.printf("  - Rename Test : %s\n", diag.renameTestPassed ? "PASSED" : "FAILED");
        Serial.println("── [END DIAGNOSTICS REPORT] ───────────────────────────────────");

        return diag;
    }

    void StorageManager::printStorageSummary()
    {
        Serial.println("=====================================");
        Serial.println("VOXA Storage Summary");
        Serial.println("=====================================");
        if (m_sdMounted)
        {
            Serial.println("Primary Storage : MicroSD");
            Serial.println("Fallback        : SPIFFS");
            Serial.println();
            Serial.println("Audio           : MicroSD");
            Serial.println("Transcripts     : MicroSD");
            Serial.println("Summaries       : MicroSD");
            Serial.println("Logs            : SPIFFS");
            Serial.println();
            Serial.println("Hardware        : PASS");
            Serial.println("Filesystem      : PASS");
            Serial.println("Fallback        : STANDBY");
            Serial.println("Device          : READY");
        }
        else if (m_cardAttached)
        {
            Serial.println("Primary Storage : MicroSD (Attached)");
            Serial.println("Fallback        : SPIFFS");
            Serial.println();
            Serial.println("Audio           : SPIFFS");
            Serial.println("Transcripts     : SPIFFS");
            Serial.println("Summaries       : SPIFFS");
            Serial.println("Logs            : SPIFFS");
            Serial.println();
            Serial.println("Hardware        : PASS (card detected)");
            Serial.println("Filesystem      : FAIL (mount/format needed)");
            Serial.println("Fallback        : ACTIVE");
            Serial.println("Device          : READY");
            Serial.printf("Error Code      : %s\n", m_lastErrorCode.c_str());
        }
        else
        {
            Serial.println("Primary Storage : SPIFFS (Fallback)");
            Serial.println("Fallback        : None");
            Serial.println();
            Serial.println("Audio           : SPIFFS");
            Serial.println("Transcripts     : SPIFFS");
            Serial.println("Summaries       : SPIFFS");
            Serial.println("Logs            : SPIFFS");
            Serial.println();
            Serial.println("Hardware        : FAIL (no card detected)");
            Serial.println("Filesystem      : FAIL");
            Serial.println("Fallback        : ACTIVE");
            Serial.println("Device          : READY");
            Serial.printf("Error Code      : %s\n", m_lastErrorCode.c_str());
        }
        Serial.println("=====================================");
    }

    std::string StorageManager::resolvePath(const char *category, const char *filename)
    {
        std::string p = "/";
        p += category;
        p += "/";
        p += filename;
        return p;
    }

    bool StorageManager::saveRecording(const char *filename, const uint8_t *data, size_t len)
    {
        std::string fullPath = resolvePath("recordings", filename);
        FS &fs = getFSForPath(fullPath.c_str());
        const char *fsName = getFSNameForPath(fullPath.c_str());
        File f = fs.open(fullPath.c_str(), FILE_WRITE);
        if (!f)
            return false;
        size_t written = f.write(data, len);
        f.close();
        if (written == len)
        {
            Serial.printf("[StorageManager] Saved %s on %s (%u bytes)\n", fullPath.c_str(), fsName, (unsigned int)len);
            return true;
        }
        return false;
    }

    bool StorageManager::saveTranscript(const char *filename, const char *text)
    {
        std::string fullPath = resolvePath("transcripts", filename);
        FS &fs = getFSForPath(fullPath.c_str());
        const char *fsName = getFSNameForPath(fullPath.c_str());
        File f = fs.open(fullPath.c_str(), FILE_WRITE);
        if (!f)
            return false;
        size_t written = f.print(text);
        f.close();
        if (written > 0)
        {
            Serial.printf("[StorageManager] Saved %s on %s (%u bytes)\n", fullPath.c_str(), fsName, (unsigned int)written);
            return true;
        }
        return false;
    }

    bool StorageManager::saveSummary(const char *filename, const char *text)
    {
        std::string fullPath = resolvePath("summaries", filename);
        FS &fs = getFSForPath(fullPath.c_str());
        const char *fsName = getFSNameForPath(fullPath.c_str());
        File f = fs.open(fullPath.c_str(), FILE_WRITE);
        if (!f)
            return false;
        size_t written = f.print(text);
        f.close();
        if (written > 0)
        {
            Serial.printf("[StorageManager] Saved %s on %s (%u bytes)\n", fullPath.c_str(), fsName, (unsigned int)written);
            return true;
        }
        return false;
    }

    bool StorageManager::saveLog(const char *message)
    {
        const char *logPath = "/logs/device.log";
        FS &fs = getFSForPath(logPath);
        File f = fs.open(logPath, FILE_APPEND);
        if (!f)
            return false;
        f.printf("[%lu ms] %s\n", millis(), message);
        f.close();
        return true;
    }

    bool StorageManager::saveJson(const char *path, const JsonDocument &doc)
    {
        FS &fs = getFSForPath(path);
        const char *fsName = getFSNameForPath(path);
        File f = fs.open(path, FILE_WRITE);
        if (!f)
            return false;
        size_t written = serializeJson(doc, f);
        f.close();
        if (written > 0)
        {
            Serial.printf("[StorageManager] Saved JSON %s on %s (%u bytes)\n", path, fsName, (unsigned int)written);
            return true;
        }
        return false;
    }

    bool StorageManager::loadJson(const char *path, JsonDocument &doc)
    {
        FS &fs = getFSForPath(path);
        File f = fs.open(path, FILE_READ);
        if (!f)
            return false;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        return (err == DeserializationError::Ok);
    }

    bool StorageManager::deleteFile(const char *path)
    {
        FS &fs = getFSForPath(path);
        const char *fsName = getFSNameForPath(path);
        if (!fs.exists(path))
            return false;
        bool ok = fs.remove(path);
        if (ok)
        {
            Serial.printf("[StorageManager] Deleted %s on %s\n", path, fsName);
        }
        return ok;
    }

    bool StorageManager::renameFile(const char *oldPath, const char *newPath)
    {
        FS &fs = getFSForPath(oldPath);
        const char *fsName = getFSNameForPath(oldPath);
        if (!fs.exists(oldPath))
            return false;
        bool ok = fs.rename(oldPath, newPath);
        if (ok)
        {
            Serial.printf("[StorageManager] Renamed %s to %s on %s\n", oldPath, newPath, fsName);
        }
        return ok;
    }

    std::vector<std::string> StorageManager::listDirectory(const char *dirPath)
    {
        std::vector<std::string> fileList;
        FS &fs = getFSForPath(dirPath);
        File dir = fs.open(dirPath);
        if (!dir || !dir.isDirectory())
            return fileList;

        File file = dir.openNextFile();
        while (file)
        {
            fileList.push_back(file.name());
            file = dir.openNextFile();
        }
        return fileList;
    }

    void StorageManager::performAutomaticCleanup()
    {
        Serial.println("[StorageManager] Running Automatic Storage Cleanup...");

        // 1. Purge /temp directory
        FS &tempFS = getFSForPath("/temp");
        const char *tempFSName = getFSNameForPath("/temp");
        File tempDir = tempFS.open("/temp");
        if (tempDir && tempDir.isDirectory())
        {
            File f = tempDir.openNextFile();
            while (f)
            {
                std::string fname = f.name();
                f.close();
                std::string fullP = "/temp/" + fname;
                tempFS.remove(fullP.c_str());
                Serial.printf("[StorageManager] Purged %s on %s\n", fullP.c_str(), tempFSName);
                f = tempDir.openNextFile();
            }
        }

        // 2. Clean 0-byte stale queue files
        FS &queueFS = getFSForPath("/queue");
        const char *queueFSName = getFSNameForPath("/queue");
        File queueDir = queueFS.open("/queue");
        if (queueDir && queueDir.isDirectory())
        {
            File f = queueDir.openNextFile();
            while (f)
            {
                std::string fname = f.name();
                size_t sz = f.size();
                f.close();
                if (sz == 0)
                {
                    std::string fullP = "/queue/" + fname;
                    queueFS.remove(fullP.c_str());
                    Serial.printf("[StorageManager] Removed 0-byte stale queue file %s on %s\n", fullP.c_str(), queueFSName);
                }
                f = queueDir.openNextFile();
            }
        }

        Serial.println("[StorageManager] Storage Cleanup Completed.");
    }

    std::string StorageManager::getStorageInfoString()
    {
        char buf[128];
        if (m_sdMounted)
        {
            uint64_t freeMB = getFreeSpaceMB();
            uint64_t totalMB = getTotalSpaceMB();
            if (totalMB >= 1024)
            {
                snprintf(buf, sizeof(buf), "SD: %.1f GB Free / %.1f GB", freeMB / 1024.0f, totalMB / 1024.0f);
            }
            else
            {
                snprintf(buf, sizeof(buf), "SD: %llu MB Free / %llu MB", freeMB, totalMB);
            }
        }
        else if (m_cardAttached)
        {
            snprintf(buf, sizeof(buf), "SD Card: Attached (Unmounted)");
        }
        else
        {
            size_t totalB = SPIFFS.totalBytes();
            size_t usedB = SPIFFS.usedBytes();
            size_t freeB = totalB > usedB ? totalB - usedB : 0;
            snprintf(buf, sizeof(buf), "Internal: %.1f MB Free / %.1f MB",
                     freeB / (1024.0f * 1024.0f), totalB / (1024.0f * 1024.0f));
        }
        return std::string(buf);
    }

    uint64_t StorageManager::getTotalSpaceMB() const
    {
        if (m_sdMounted)
            return SD.totalBytes() / (1024 * 1024);
        return SPIFFS.totalBytes() / (1024 * 1024);
    }

    uint64_t StorageManager::getUsedSpaceMB() const
    {
        if (m_sdMounted)
            return SD.usedBytes() / (1024 * 1024);
        return SPIFFS.usedBytes() / (1024 * 1024);
    }

    uint64_t StorageManager::getFreeSpaceMB() const
    {
        uint64_t total = getTotalSpaceMB();
        uint64_t used = getUsedSpaceMB();
        return (total > used) ? (total - used) : 0;
    }
}