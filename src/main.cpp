#include <Arduino.h>
#include <Audio.h>
#include <WiFi.h>
#include <EncButton.h>
#include <EEManager.h>
#include <ArduinoOTA.h>

#define AP_SSID "Skynet"
#define AP_PASS "eZ8n8viD"

// Digital I/O used for DAC
#define I2S_DOUT      GPIO_NUM_25
#define I2S_BCLK      GPIO_NUM_27
#define I2S_LRC       GPIO_NUM_26
#define RADIO_BUFFER (1600 * 25)  // default 1600*5, этого МАЛО
#define VOLUME_DEFAULT 1
#define VOLUME_MIN 1
#define VOLUME_MAX 20

// Button settings and pins.
#define BTN_POWER_PIN   GPIO_NUM_32
#define BTN_PLAY_PIN    GPIO_NUM_21
#define BTN_MINUS_PIN   GPIO_NUM_16
#define BTN_PLUS_PIN    GPIO_NUM_4
#define EB_HOLD_TIME 1000

Button btnPower(BTN_POWER_PIN, INPUT_PULLDOWN);
Button btnPlay(BTN_PLAY_PIN, INPUT_PULLDOWN);
Button btnMinus(BTN_MINUS_PIN, INPUT_PULLDOWN);
Button btnPlus(BTN_PLUS_PIN, INPUT_PULLDOWN);

Audio audio;
bool mustReconnect = false;
uint8_t stationsCount = 12;
const char* stations[] = {
    "https://uk3.internet-radio.com/proxy/majesticjukebox?mp=/live",
    "http://prmstrm.1.fm:8000/electronica",
    "http://prmstrm.1.fm:8000/x",
    "http://stream81.metacast.eu/radio1rock128",
    "http://mp3.ffh.de/radioffh/hqlivestream.mp3",
    "https://20073.live.streamtheworld.com/100PNL_MP3_SC",
    "https://icecast.omroep.nl/radio6-bb-mp3",
    "https://lyd.nrk.no/nrk_radio_jazz_mp3_h",
    "https://lyd.nrk.no/nrk_radio_folkemusikk_mp3_h",
    "https://live-bauerno.sharp-stream.com/radiorock_no_mp3",
    "https://0n-electro.radionetz.de/0n-electro.mp3",
    "https://pub0102.101.ru:8443/stream/trust/mp3/128/7"
};

struct Settings {
  uint8_t stationIndex = 0;
  uint8_t volume = VOLUME_DEFAULT;
};

Settings settings;
EEManager memory(settings);

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.println("WIFI mRadio");
    Serial.print("Connecting to WIFI");

    print_wakeup_reason();
    esp_sleep_enable_ext0_wakeup(BTN_POWER_PIN, 0); //1 = High, 0 = Low

    EEPROM.begin(memory.blockSize());
    memory.begin(0, 'b');

    Serial.print("Volume: ");
    Serial.println(settings.volume);
    Serial.print("Station: ");
    Serial.println(settings.stationIndex);

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(AP_SSID, AP_PASS);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {  // U_SPIFFS
          type = "filesystem";
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          Serial.println("End Failed");
        }
      });

    ArduinoOTA.begin();

    Serial.println();
    Serial.println("WIFI connected.");
    Serial.println(WiFi.localIP());

    audio.setBufsize(RADIO_BUFFER, -1);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.forceMono(true);
    audio.setVolume(settings.volume);
    audio.setTone(6, -10, 4);

    mustReconnect = true;
}

void loop() {
  btnMinus.tick();
  btnPlus.tick();
  btnPlay.tick();
  btnPower.tick();
  if (memory.tick()) Serial.println("Memory Updated!");
  ArduinoOTA.handle();

  if (btnMinus.click()) {
    settings.volume = constrain(settings.volume - 1, VOLUME_MIN, VOLUME_MAX);
    audio.setVolume(settings.volume);
    Serial.print("Current volume: ");
    Serial.println(settings.volume);
    memory.update();
  }

  if (btnPlus.click()) {
    settings.volume = constrain(settings.volume + 1, VOLUME_MIN, VOLUME_MAX);
    audio.setVolume(settings.volume);
    Serial.print("Current volume: ");
    Serial.println(settings.volume);
    memory.update();
  }

  if (btnPlay.hasClicks(1)) {
      Serial.println("Play/Pause");
      audio.pauseResume();
  }

  if (btnPlay.hasClicks(2)) {
    if (settings.stationIndex + 1 >= stationsCount) {
      settings.stationIndex = 0;
    } else {
      settings.stationIndex++;
    }

    Serial.print("Current station index: ");
    Serial.print(settings.stationIndex);
    Serial.print(" (");
    Serial.print(stations[settings.stationIndex]);
    Serial.println(")");

    memory.update();
    
    mustReconnect = true;
  }

  if (btnPower.releaseHold()) {
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  audio.loop();

  if (mustReconnect == true) {
    audio.connecttohost(stations[settings.stationIndex]);
    if (!audio.isRunning()) audio.pauseResume();
    mustReconnect = false;
  }
}

// optional
// void audio_info(const char *info){
//     Serial.print("info        "); Serial.println(info);
// }
// void audio_id3data(const char *info){  //id3 metadata
//     Serial.print("id3data     ");Serial.println(info);
// }
// void audio_eof_mp3(const char *info){  //end of file
//     Serial.print("eof_mp3     ");Serial.println(info);
// }
// void audio_showstation(const char *info){
//     Serial.print("station     ");Serial.println(info);
// }
// void audio_showstreamtitle(const char *info){
//     Serial.print("streamtitle ");Serial.println(info);
// }
// void audio_bitrate(const char *info){
//     Serial.print("bitrate     ");Serial.println(info);
// }
// void audio_commercial(const char *info){  //duration in sec
//     Serial.print("commercial  ");Serial.println(info);
// }
// void audio_icyurl(const char *info){  //homepage
//     Serial.print("icyurl      ");Serial.println(info);
// }
// void audio_lasthost(const char *info){  //stream URL played
//     Serial.print("lasthost    ");Serial.println(info);
// }
// void audio_eof_speech(const char *info){
//     Serial.print("eof_speech  ");Serial.println(info);
// }

