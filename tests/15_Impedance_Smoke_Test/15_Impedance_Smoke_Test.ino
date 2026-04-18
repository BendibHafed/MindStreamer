#include <MindStreamer.h>

MindStreamer eeg;

static constexpr uint8_t kTestChannel = 1;
static constexpr uint8_t kNumChannels = 8;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint32_t kMeasurePeriodMs = 10000;
static constexpr uint32_t kStatusPeriodMs = 2000;

int32_t samples[kNumChannels] = {0};
uint32_t sampleCount = 0;
uint32_t lastMeasureMs = 0;
uint32_t lastStatusMs = 0;

void printConfigSummary() {
    Serial.println(F("Impedance smoke test"));
    Serial.println(F("Validated operating point:"));
    Serial.println(F("- 8 channels"));
    Serial.println(F("- DR_250"));
    Serial.println(F("- PGA_GAIN_24"));
    Serial.println(F("- SPI 500000 Hz"));
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("m = measure impedance now"));
    Serial.println(F("p = print one raw sample snapshot"));
    Serial.println(F("h = print help"));
    Serial.println(F(""));
}

void printOneSnapshot() {
    Serial.print(F("RAW,"));
    Serial.print(sampleCount);
    for (uint8_t ch = 0; ch < kNumChannels; ++ch) {
        Serial.print(',');
        Serial.print(samples[ch]);
    }
    Serial.println();
}

void measureNow() {
    float impedance_kohm = 0.0f;

    Serial.print(F("MEASURE_START,CH"));
    Serial.println(kTestChannel);

    const bool ok = eeg.measureImpedance(kTestChannel, impedance_kohm);

    if (ok) {
        Serial.print(F("MEASURE_OK,CH"));
        Serial.print(kTestChannel);
        Serial.print(F(",Z_KOHM,"));
        Serial.println(impedance_kohm, 3);
    } else {
        Serial.print(F("MEASURE_FAIL,ERR,"));
        Serial.println(static_cast<int>(eeg.getLastError()));
    }
}

void handleSerialCommands() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        switch (c) {
            case 'm':
            case 'M':
                measureNow();
                break;

            case 'p':
            case 'P':
                printOneSnapshot();
                break;

            case 'h':
            case 'H':
                printConfigSummary();
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

    printConfigSummary();

    if (!eeg.begin(config)) {
        Serial.print(F("BEGIN_FAIL,ERR,"));
        Serial.println(static_cast<int>(eeg.getLastError()));
        while (true) {
            delay(1000);
        }
    }

    if (!eeg.startStreaming()) {
        Serial.print(F("START_FAIL,ERR,"));
        Serial.println(static_cast<int>(eeg.getLastError()));
        while (true) {
            delay(1000);
        }
    }

    Serial.println(F("STREAMING_OK"));
}

void loop() {
    handleSerialCommands();

    if (eeg.dataAvailable()) {
        if (eeg.readAllChannels(samples, kNumChannels)) {
            ++sampleCount;
        }
    }

    const uint32_t now = millis();

    if ((now - lastStatusMs) >= kStatusPeriodMs) {
        lastStatusMs = now;
        Serial.print(F("STATUS,SAMPLES,"));
        Serial.print(sampleCount);
        Serial.print(F(",STREAMING,"));
        Serial.println(eeg.isStreaming() ? F("YES") : F("NO"));
    }

    if ((now - lastMeasureMs) >= kMeasurePeriodMs) {
        lastMeasureMs = now;
        measureNow();
    }
}
