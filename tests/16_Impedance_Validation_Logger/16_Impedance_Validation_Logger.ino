#include <MindStreamer.h>

MindStreamer eeg;

static constexpr uint8_t kNumChannels = 8;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint8_t kRepeatCount = 10;
static constexpr uint16_t kDelayBetweenMeasuresMs = 300;

uint8_t selectedChannel = 1;

void printHelp() {
    Serial.println(F("Impedance validation logger"));
    Serial.println(F("Validated operating point:"));
    Serial.println(F("- 8 channels"));
    Serial.println(F("- DR_250"));
    Serial.println(F("- PGA_GAIN_24"));
    Serial.println(F("- SPI 500000 Hz"));
    Serial.println(F(""));
    Serial.println(F("Protocol suggestion on one channel:"));
    Serial.println(F("1) Physically short the input path, then press s"));
    Serial.println(F("2) Leave the input floating, then press f"));
    Serial.println(F("3) Apply a real contact, then press c"));
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("1..8 = select channel"));
    Serial.println(F("m = one impedance measurement"));
    Serial.println(F("t = 10 repeated measurements"));
    Serial.println(F("s = label next 10 measures as SHORTED"));
    Serial.println(F("f = label next 10 measures as FLOATING"));
    Serial.println(F("c = label next 10 measures as CONTACT"));
    Serial.println(F("h = print help"));
    Serial.println(F(""));
}

void printChannelSelection() {
    Serial.print(F("SELECTED_CHANNEL,"));
    Serial.println(selectedChannel);
}

bool measureOnce(float &impedance_kohm) {
    const bool ok = eeg.measureImpedance(selectedChannel, impedance_kohm);
    if (!ok) {
        Serial.print(F("MEASURE_FAIL,CH"));
        Serial.print(selectedChannel);
        Serial.print(F(",ERR,"));
        Serial.println(static_cast<int>(eeg.getLastError()));
        return false;
    }
    return true;
}

void runSeries(const __FlashStringHelper* label) {
    float minZ = 0.0f;
    float maxZ = 0.0f;
    float sumZ = 0.0f;
    uint8_t okCount = 0;

    Serial.print(F("SERIES_START,"));
    Serial.print(label);
    Serial.print(F(",CH"));
    Serial.println(selectedChannel);

    for (uint8_t i = 0; i < kRepeatCount; ++i) {
        float z = 0.0f;
        if (measureOnce(z)) {
            if (okCount == 0) {
                minZ = z;
                maxZ = z;
            } else {
                if (z < minZ) minZ = z;
                if (z > maxZ) maxZ = z;
            }
            sumZ += z;
            ++okCount;

            Serial.print(F("SERIES_ITEM,"));
            Serial.print(label);
            Serial.print(F(",CH"));
            Serial.print(selectedChannel);
            Serial.print(F(",INDEX,"));
            Serial.print(i + 1);
            Serial.print(F(",Z_KOHM,"));
            Serial.println(z, 3);
        }
        delay(kDelayBetweenMeasuresMs);
    }

    if (okCount > 0) {
        Serial.print(F("SERIES_SUMMARY,"));
        Serial.print(label);
        Serial.print(F(",CH"));
        Serial.print(selectedChannel);
        Serial.print(F(",COUNT,"));
        Serial.print(okCount);
        Serial.print(F(",MIN_KOHM,"));
        Serial.print(minZ, 3);
        Serial.print(F(",MAX_KOHM,"));
        Serial.print(maxZ, 3);
        Serial.print(F(",AVG_KOHM,"));
        Serial.println(sumZ / static_cast<float>(okCount), 3);
    } else {
        Serial.print(F("SERIES_FAIL,"));
        Serial.print(label);
        Serial.print(F(",CH"));
        Serial.println(selectedChannel);
    }
}

void setup() {
    Serial.begin(kSerialBaud);
    delay(1000);

    MindStreamerConfig config;
    config.num_channels = kNumChannels;
    config.data_rate = DR_250;
    config.pga_gain = PGA_GAIN_24;
    config.spi_clock_hz = 500000;
    config.output_mode = OUTPUT_TEXT;
    config.enable_drl = false;

    if (!eeg.begin(config)) {
        Serial.print(F("BEGIN_FAIL,ERR,"));
        Serial.println(static_cast<int>(eeg.getLastError()));
        while (true) {
            delay(1000);
        }
    }

    printHelp();
    printChannelSelection();
}

void loop() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());

        if (c >= '1' && c <= '8') {
            selectedChannel = static_cast<uint8_t>(c - '0');
            printChannelSelection();
            continue;
        }

        switch (c) {
            case 'm':
            case 'M': {
                float z = 0.0f;
                if (measureOnce(z)) {
                    Serial.print(F("MEASURE_OK,CH"));
                    Serial.print(selectedChannel);
                    Serial.print(F(",Z_KOHM,"));
                    Serial.println(z, 3);
                }
                break;
            }

            case 't':
            case 'T':
                runSeries(F("GENERIC"));
                break;

            case 's':
            case 'S':
                runSeries(F("SHORTED"));
                break;

            case 'f':
            case 'F':
                runSeries(F("FLOATING"));
                break;

            case 'c':
            case 'C':
                runSeries(F("CONTACT"));
                break;

            case 'h':
            case 'H':
                printHelp();
                break;

            case '\r':
            case '\n':
                break;

            default:
                Serial.print(F("UNKNOWN_COMMAND,"));
                Serial.println(c);
                break;
        }
    }
}
