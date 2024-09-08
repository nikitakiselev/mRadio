#include <Arduino.h>
#include <Audio.h>
#include <WiFi.h>
#include <EncButton.h>
#include <EEManager.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>

#define AP_SSID "Skynet"
#define AP_PASS "eZ8n8viD"

#define MILLIS_MULTIPLIER 1000
#define MAX_STATIONS_COUNT 50

// Digital I/O used for DAC
#define I2S_DOUT      GPIO_NUM_25
#define I2S_BCLK      GPIO_NUM_27
#define I2S_LRC       GPIO_NUM_26
#define RADIO_BUFFER (1600 * 30)  // default 1600*5, этого МАЛО
#define VOLUME_DEFAULT 1
#define VOLUME_MIN 1
#define VOLUME_MAX 20

// Button settings and pins.
#define BTN_POWER_PIN   GPIO_NUM_32
#define BTN_PLAY_PIN    GPIO_NUM_21
#define BTN_MINUS_PIN   GPIO_NUM_16
#define BTN_PLUS_PIN    GPIO_NUM_4
#define EB_HOLD_TIME 1000

// LED
#define LED_PIN GPIO_NUM_13

Button btnPower(BTN_POWER_PIN, INPUT_PULLDOWN);
Button btnPlay(BTN_PLAY_PIN, INPUT_PULLDOWN);
Button btnMinus(BTN_MINUS_PIN, INPUT_PULLDOWN);
Button btnPlus(BTN_PLUS_PIN, INPUT_PULLDOWN);

Audio audio;
bool mustReconnect = false;
uint8_t stationsCount = 0;
String stations[MAX_STATIONS_COUNT];

struct Settings {
  uint8_t stationIndex = 0;
  uint8_t volume = VOLUME_DEFAULT;
};

Settings settings;
EEManager memory(settings);

bool ledStatus = false;

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

void delayed_esp_restart(uint32_t seconds = 5) {
  char buf[50];
  sprintf(buf, "The system will restart in %d seconds...", seconds); 
  Serial.println(buf);
  delay(seconds * MILLIS_MULTIPLIER);
  esp_restart();
}

void ledBlink() {
  // if (ledStatus) {
  //   analogWrite(LED_PIN, 100);
  // } else {
  //   analogWrite(LED_PIN, 0);
  // }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    
    analogWrite(LED_PIN, 100);

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
      analogWrite(LED_PIN, 100);
      delay(200);
      analogWrite(LED_PIN, 0);
      delay(200);
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

    Serial.println("Read radio stations from SPIFFS.");
    if(! SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      delayed_esp_restart();
      return;
    }

    File file = SPIFFS.open("/stations.txt");
    if(! file){
      Serial.println("Failed to open stations.txt for reading");
      return;
    }
    Serial.println("File stations.txt found.");
    Serial.println("File Content:");

    while(stationsCount < MAX_STATIONS_COUNT && file.available()) {
      stations[stationsCount] = file.readStringUntil('\n');
      stationsCount++;
    }
    file.close();

    Serial.print("Read ");
    Serial.print(stationsCount);
    Serial.println(" stations from file.");

    Serial.println("Here the list of the stations.");
    for (int i = 0; i < stationsCount; i++) {
      Serial.println(stations[i]);
    }
    
    mustReconnect = true;
    ledStatus = false;
}

void loop() {
  btnMinus.tick();
  btnPlus.tick();
  btnPlay.tick();
  btnPower.tick();
  if (memory.tick()) Serial.println("Memory Updated!");
  ArduinoOTA.handle();

  ledBlink();

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

  if (btnPlay.hasClicks(3)) {
    if (settings.stationIndex - 1 < 0) {
      settings.stationIndex = (stationsCount - 1);
    } else {
      settings.stationIndex--;
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
    analogWrite(LED_PIN, 30);
    Serial.print("Connect to host...");;
    audio.connecttohost(stations[settings.stationIndex].c_str());
    if (!audio.isRunning()) audio.pauseResume();
    mustReconnect = false;
  }
}

// optional
void audio_info(const char *info){
  Serial.print("info        "); Serial.println(info);
}
// void audio_id3data(const char *info){  //id3 metadata
//     Serial.print("id3data     ");Serial.println(info);
// }
// void audio_eof_mp3(const char *info){  //end of file
//     Serial.print("eof_mp3     ");Serial.println(info);
// }
void audio_showstation(const char *info){
  analogWrite(LED_PIN, 0);
    Serial.print("station     ");Serial.println(info);
}
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

