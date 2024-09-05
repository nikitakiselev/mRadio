#include <Arduino.h>
#include <Audio.h>
#include <WiFi.h>
#include <EncButton.h>
#include <EEManager.h>

#define AP_SSID "Skynet"
#define AP_PASS "eZ8n8viD"

// Digital I/O used for DAC
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#define RADIO_BUFFER (1600 * 25)  // default 1600*5, этого МАЛО
#define VOLUME_DEFAULT 1
#define VOLUME_MIN 1
#define VOLUME_MAX 10

// Button settings and pins.
#define BTN_POWER_PIN 23
#define BTN_PLAY_PIN 21
#define BTN_MINUS_PIN 16
#define BTN_PLUS_PIN 4
#define EB_HOLD_TIME 1000

Button btnPower(BTN_POWER_PIN, INPUT_PULLDOWN);
Button btnPlay(BTN_PLAY_PIN, INPUT_PULLDOWN);
Button btnMinus(BTN_MINUS_PIN, INPUT_PULLDOWN);
Button btnPlus(BTN_PLUS_PIN, INPUT_PULLDOWN);

Audio audio;
bool mustReconnect = false;
uint8_t stationsCount = 10;
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
    "https://live-bauerno.sharp-stream.com/radiorock_no_mp3"
};

struct Settings {
  uint8_t stationIndex = 0;
  uint8_t volume = VOLUME_DEFAULT;
};

Settings settings;
EEManager memory(settings);

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.println("WIFI mRadio");
    Serial.print("Connecting to WIFI");

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

    Serial.println();
    Serial.println("WIFI connected.");
    Serial.println(WiFi.localIP());

    audio.setBufsize(RADIO_BUFFER, -1);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.forceMono(true);
    audio.setVolume(settings.volume);

    mustReconnect = true;
}

void loop() {
  btnMinus.tick();
  btnPlus.tick();
  btnPlay.tick();
  btnPower.tick();
  if (memory.tick()) Serial.println("Memory Updated!");

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

  if (btnPower.holding()) {
    Serial.println("Restarting in 2 seconds");
    delay(2000);
    ESP.restart();
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
