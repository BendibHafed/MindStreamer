#include <MindStreamer.h>

MindStreamer eeg;

static constexpr uint8_t kNumChannels = 8;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint8_t kPassesPerChannel = 5;
static constexpr uint16_t kDelayBetweenMeasuresMs = 200;

void printHelp() {
    Serial.println(F("Impedance all-channels scan"));
    Serial.println(F("Commands:"));
    Serial.println(F("m = single pass on channels 1..8"));
    Serial.println(F("a = average pass on channels 1..8"));
    Serial.println(F("h = print help"));
    Serial.println(F(""));
}

bool measureChannelAverage(uint8_t channel, float &avg_kohm) {
    float sum = 0.0f;
    uint8_t okCount = 0;

    for (uint8_t i = 0; i < kPassesPerChannel; ++i) {
        float z = 0.0f;
        if (eeg.measureImpedance(channel, z)) {
            sum += z;
            ++okCount;
        }
        delay(kDelayBetweenMeasuresMs);
    }

    if (okCount == 0) {
        return false;
    }

    avg_kohm = sum / static_cast<float>(okCount);
    return true;
}

void runSinglePass() {
    Serial.println(F("SCAN_SINGLE_START"));
    for (uint8_t ch = 1; ch <= kNumChannels; ++ch) {
        float z = 0.0f;
        if (eeg.measureImpedance(ch, z)) {
            Serial.print(F("SCAN_ITEM,CH"));
            Serial.print(ch);
            Serial.print(F(",Z_KOHM,"));
            Serial.println(z, 3);
        } else {
            Serial.print(F("SCAN_FAIL,CH"));
            Serial.print(ch);
            Serial.print(F(",ERR,"));
            Serial.println(static_cast<int>(eeg.getLastError()));
        }
        delay(kDelayBetweenMeasuresMs);
    }
    Serial.println(F("SCAN_SINGLE_END"));
}

void runAveragePass() {
    Serial.println(F("SCAN_AVG_START"));
    for (uint8_t ch = 1; ch <= kNumChannels; ++ch) {
        float zAvg = 0.0f;
        if (measureChannelAverage(ch, zAvg)) {
            Serial.print(F("SCAN_AVG_ITEM,CH"));
            Serial.print(ch);
            Serial.print(F(",AVG_KOHM,"));
            Serial.println(zAvg, 3);
        } else {
            Serial.print(F("SCAN_AVG_FAIL,CH"));
            Serial.print(ch);
            Serial.print(F(",ERR,"));
            Serial.println(static_cast<int>(eeg.getLastError()));
        }
    }
    Serial.println(F("SCAN_AVG_END"));
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
}

void loop() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        switch (c) {
            case 'm':
            case 'M':
                runSinglePass();
                break;

            case 'a':
            case 'A':
                runAveragePass();
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
