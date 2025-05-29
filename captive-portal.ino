// Captive Portal
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <Preferences.h>

// Fan Controller Imports
#include <driver/pcnt.h>
#include <driver/ledc.h>

// Captive Portal - Constants
const char *ssid = "";
const char *password = NULL;

#define MAX_CLIENTS 4
#define WIFI_CHANNEL 6

Preferences preferences;

const IPAddress localIP(4, 3, 2, 1);
const IPAddress gatewayIP(4, 3, 2, 1);
const IPAddress subnetMask(255, 255, 255, 0);

const String localIPURL = "/";

const char index_html[] PROGMEM = R"=====(
    <!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ESP32 Fan Controller</title>
    <style>
      *,
      ::after,
      ::before,
      ::backdrop,
      ::file-selector-button {
        box-sizing: border-box;
        margin: 0;
        padding: 0;
        border: 0 solid;
      }
      .app {
        background: #222222;
        color: #f5f5f5;
        height: 100vh;
        padding: 1rem;
        overflow-y: auto;
        font-family: sans-serif;
      }

      .app-header {
        display: flex;
        justify-content: space-between;
        align-items: center;
      }

      .header-title {
        font-size: 1.5rem;
        font-weight: 500;
      }

      .master-button {
        background-color: #f5f5f5;
        color: #141414;
        font-weight: 500;
        font-size: 1.3rem;
        padding: 0.5rem 1.5rem;
        border: 1px solid #141414;
        border-radius: 0.55rem;
        cursor: pointer;
        min-width: 8rem;
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 0.5rem;
      }

      .master-button.running {
        background-color: #b91c1c;
        color: white;
        animation: pulse 2s infinite;
        border-color: #7f1d1d;
      }

      .fan-grid {
        display: grid;
        grid-template-columns: 1fr;
        gap: 1rem;
        padding-top: 1rem;
        padding-bottom: 0.5rem;
      }

      @media (min-width: 1024px) {
        .fan-grid {
          grid-template-columns: repeat(2, 1fr);
        }
      }

      .bg-dark {
        background-color: #171717;
      }

      .rounded-lg {
        border-radius: 0.5rem;
      }

      .px-5 {
        padding-left: 1rem;
        padding-right: 1rem;
      }

      .py-5 {
        padding-top: 1rem;
        padding-bottom: 1rem;
      }

      .fan-header {
        display: flex;
        justify-content: space-between;
      }

      .fan-toggle {
        width: 3.5rem;
        height: 2rem;
        border-radius: 9999px;
        position: relative;
        cursor: pointer;
        transition: background-color 0.3s ease;
      }

      .fan-toggle.off {
        background-color: #d1d5db;
      }

      .fan-toggle.on {
        background-color: #494949;
      }

      .fan-thumb {
        position: absolute;
        top: 0.2rem;
        left: 0.23rem;
        width: 1.6rem;
        height: 1.6rem;
        border-radius: 9999px;
        background-color: white;
        transition: transform 0.3s ease;
      }

      .fan-thumb.translate {
        transform: translateX(1.5rem);
      }

      .range-container {
        position: relative;
        flex-grow: 1;
        margin-top: 0.75rem;
      }

      .range-controls {
        display: flex;
        align-items: center;
        gap: 1.25rem;
      }

      .range-value {
        font-size: 2rem;
        font-weight: 600;
        color: #f5f5f5;
        min-width: 80px;
      }

      .slider {
        width: 100%;
        height: 2rem;
        border-radius: 3px;
        appearance: none;
        outline: none;
        transition: opacity 0.2s;
        background-color: #ffffff;
      }

      .slider::-webkit-slider-thumb {
        appearance: none;
        height: 2rem;
        width: 1.2rem;
        border-radius: 3px;
        background-color: #fff;
        cursor: pointer;
        position: relative;
        box-shadow: 0 0 0 0px #141414;
      }

      @keyframes pulse {
        0%,
        100% {
          opacity: 1;
        }
        50% {
          opacity: 0.5;
        }
      }
    </style>
    <style>
      .custom-button {
        appearance: button;
        font: inherit;
        font-feature-settings: inherit;
        font-variation-settings: inherit;
        letter-spacing: inherit;
        font-weight: 600;
        font-family: ui-sans-serif, system-ui, sans-serif, "Apple Color Emoji",
          "Segoe UI Emoji", "Segoe UI Symbol", "Noto Color Emoji";

        border-radius: 0.375rem;
        padding-block: 0.40rem;
        padding-inline: 1.25rem;
        padding-left: 1.25rem;
        padding-right: 1.25rem;

        background-color: oklch(45.5% 0.188 13.697);
        color: white;
        cursor: pointer;
        opacity: 1;
        -webkit-text-size-adjust: 100%;
        -webkit-tap-highlight-color: transparent;
        tab-size: 4;

        transition-property: color, background-color, border-color,
          outline-color, text-decoration-color, fill, stroke, --tw-gradient-from,
          --tw-gradient-via, --tw-gradient-to, opacity, box-shadow, transform,
          translate, scale, rotate, filter, -webkit-backdrop-filter,
          backdrop-filter, display, visibility, content-visibility, overlay,
          pointer-events;
        transition-timing-function: cubic-bezier(0.4, 0, 0.2, 1);
        transition-duration: 150ms;

        box-shadow: none;
        box-sizing: border-box;
        margin: 0;
        border: 0 solid;
      }

      .custom-button:active {
        box-shadow: 0 0 0 3px oklch(58.6% 0.253 17.585 / 0.3);
      }

      button[aria-checked="true"].custom-button:active {
        box-shadow: 0 0 0 3px oklch(89.7% 0.196 126.665 / 0.3);
      }

      button[aria-checked="true"].custom-button {
        background-color: oklch(89.7% 0.196 126.665);
        color: oklch(20.5% 0 0);
        box-shadow: 0 0 0 1px oklch(64.8% 0.2 131.684 / 0.3);
      }
    </style>
  </head>
  <body>
    <div class="app">
      <div class="app-header">
        <h1 class="header-title">Fan Controller</h1>
        <button id="master-button" style="padding: 0.5rem 1.5rem;" onclick="toogleMaster()" aria-checked="false" class="custom-button">Master stopped</button>
      </div>

      <div id="grid" class="fan-grid"></div>
    </div>
    <script>
      const renderControlElement = (ix, fan) => {
        return `<div class="bg-dark rounded-lg px-5 py-5">
            <div>
              <button onclick="toogleFan(${ix})" aria-checked="${fan.on ? "true": "false"}" class="custom-button">Fan ${
                ix + 1
              } Running</button>
            </div>

            <div class="range-container">
              <div class="range-controls">
                <input
                  type="range"
                  min="0"
                  max="100"
                  value="${fan.pwm}"
                  style="background: linear-gradient(to right, #fff 0%, #fff ${
                    fan.pwm
                  }%, #494949 ${fan.pwm}%, #494949 100%);"
                  class="slider"
                />
                <span class="range-value">${fan.rpm}</span>
              </div>
            </div>
          </div>`;
      };

      async function main() {
        const grid = document.getElementById("grid");
        if (!grid) return;

        const request = await fetch("/fan-info");
        const data = await request.json();

        Array.from(grid.children).forEach((element, ix) => {
          element.querySelector(
            ".range-container .range-controls span"
          ).textContent = data[ix].rpm
        });
      }

      async function initialize() {
        const grid = document.getElementById("grid");

        const request = await fetch("/fan-info");
        const data = await request.json();

        //   grid.innerHTML = "";
        data.forEach((element, ix) => {
          console.log(element);
          grid.innerHTML += renderControlElement(ix, {
            on: element.status,
            pwm: element.pwm,
            rpm: element.rpm
          });
        });

        Array.from(grid.children).forEach((element, ix) => {
          const rangeControl = element.querySelector(
            ".range-container .range-controls input"
          );

          rangeControl.addEventListener("input", (evt) => {
            rangeControl.style[
              "background"
            ] = `linear-gradient(to right, #fff 0%, #fff ${evt.target.value}%, #494949 ${evt.target.value}%, #494949 100%)`;
          });

          rangeControl.addEventListener("change", (evt) => {
            fetch(`/update-speed?fanIndex=${ix}&updateValue=${evt.target.value}`)
          });
        });

        const masterData = await fetch("/server-stats");
        const masterResponse = await masterData.json();
        const button = document.getElementById("master-button")
        if(masterResponse.success){
          button.textContent = "Master running";
          button.ariaChecked = "true";
        } else {
          button.textContent = "Master stopped";
          button.ariaChecked = "false";
        }
      }

      function toogleMaster(){
        fetch("/toogle-master");
        const button = document.getElementById("master-button")
        button.ariaChecked == "true" ? button.textContent = "Master stopped" : button.textContent = "Master running";
        button.ariaChecked == "true" ? button.ariaChecked = "false" : button.ariaChecked = "true";       
      }

      function toogleFan(idx) {
        const grid = document.getElementById("grid");
        if (!grid) return;

        const element = Array.from(grid.children)[idx].querySelector(
          "div button.custom-button"
        );

        fetch(`/toogle-fan?fanIndex=${idx}`)

        element.ariaChecked == "true" ? element.textContent = `Fan ${idx} Stopped` : element.textContent = `Fan ${idx} Running`;
        element.ariaChecked == "true" ? element.ariaChecked = "false" : element.ariaChecked = "true";
      }

      initialize();
      setInterval(main, 1000);
    </script>
  </body>
</html>
)=====";

// Fan Controller - Contants
const ledc_channel_t pwmChannels[4] = {
  LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};
const pcnt_unit_t pcntUnits[4] = {
  PCNT_UNIT_0, PCNT_UNIT_1, PCNT_UNIT_2, PCNT_UNIT_3
};

const int pwmPercent = 30;
int pwmDuty = (255 * pwmPercent) / 100;
uint32_t lastRPM[4] = { 0 };

// Fan Controller - Define Pins
bool masterOn = true;
const int pwmPins[4] = { 32, 33, 27, 19 };
const int tachPins[4] = { 15, 16, 17, 18 };
const int mosfetPins[4] = { 14, 24, 13, 26 };

// Server Variables
DNSServer dnsServer;
AsyncWebServer server(80);

// Captive Portal - Functions
void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
#define DNS_INTERVAL 30
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
  WiFi.mode(WIFI_MODE_AP);
  const IPAddress subnetMask(255, 255, 255, 0);
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
  WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
  my_config.ampdu_rx_enable = false;
  esp_wifi_init(&my_config);
  esp_wifi_start();
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) {
    request->redirect("http://logout.net");
  });
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) {
    request->send(404);
  });

  server.on("/generate_204", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });
  server.on("/redirect", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });
  server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });
  server.on("/canonical.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });
  server.on("/success.txt", [](AsyncWebServerRequest *request) {
    request->send(200);
  });
  server.on("/ncsi.txt", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });

  server.on("/favicon.ico", [](AsyncWebServerRequest *request) {
    request->send(404);
  });

  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
    response->addHeader("Cache-Control", "public,max-age=31536000");
    request->send(response);
    Serial.println("Served Basic HTML Page");
  });

  // apis
  server.on("/toogle-master", [](AsyncWebServerRequest *request) {
    // set all the mosfets to off
    for (int i = 0; i < 4; i++) {
      if (masterOn) {
        digitalWrite(mosfetPins[i], LOW);
      } else {
        digitalWrite(mosfetPins[i], HIGH);
      }
    }
    masterOn = !masterOn;

    request->send(200, "application/json", "{ \"success\": \"true\" }");
  });

  server.on("/fan-info", [](AsyncWebServerRequest *request) {
    digitalWrite(2, 1);
    preferences.begin("speed", true);

    Serial.println("Reading preferences...");
    Serial.println(preferences.getInt("fan0", -1));
    
    String json = "[";
    for (int i = 0; i < 4; i++) {
      char name[10];
      sprintf(name, "fan%d", i);
      int fanSpeed = preferences.getInt(name, 30);
      json += "{ \"rpm\":";
      json += String(lastRPM[i]);
      json += ",\"status\": ";
      json += String(digitalRead(mosfetPins[i]));
      json += ",\"pwm\":";
      json += String(fanSpeed);
      json += "}";
      if (i < 3) json += ",";
    }
    preferences.end();
    json += "]";

    request->send(200, "application/json", json);
    delayMicroseconds(500);
    digitalWrite(2, 0);
  });



  server.on("/reset-prefs", [](AsyncWebServerRequest *request) {
    // Empty for now...
  });

  server.on("/server-stats", [](AsyncWebServerRequest *request) {
    String json = "{ \"success\": ";
    if(masterOn){
      json += "true";
    } else {
      json += "false";
    }
    json+="}";
    return request->send(200, "application/json", json);
  });

  server.on("/update-speed", [](AsyncWebServerRequest *request) {
    if (request->hasArg("fanIndex") && request->hasArg("updateValue")) {
      int fanIndex = request->arg("fanIndex").toInt();
      int updateValue = request->arg("updateValue").toInt();
      updateValue = constrain(updateValue, 1, 100);

      preferences.begin("speed", false);
      char name[10];
      sprintf(name, "fan%d", fanIndex);
      preferences.putInt(name, updateValue);
      preferences.end();

      //TODO: update the speed
      ledcWrite(pwmChannels[fanIndex], updateValue);

      return request->send(200, "application/json", "{ \"success\": \"true\" }");
    }
    request->send(400, "application/json", "{ \"error\": \"Failed to get query parameters.\" }");
  });

  server.on("/toogle-fan", [](AsyncWebServerRequest *request) {
    Serial.print("toogle fan: ");
    Serial.println(request->arg("fanIndex"));
    if (request->hasArg("fanIndex")) {
      int fanIndex = request->arg("fanIndex").toInt();

      int state = digitalRead(mosfetPins[fanIndex]);

      if (state == 1) {
        digitalWrite(mosfetPins[fanIndex], 0);
      } else {
        digitalWrite(mosfetPins[fanIndex], 1);
      }

      return request->send(200, "application/json", "{ \"success\": \"true\" }");
    }
    request->send(400, "application/json", "{ \"error\": \"Failed to get query parameters.\" }");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
    Serial.print("onnotfound ");
    Serial.print(request->host());
    Serial.print(" ");
    Serial.print(request->url());
    Serial.print(" sent redirect to " + localIPURL + "\n");
  });
}

// Fan Controller - Functions
void setupController() {
  masterOn = true;
  for (int i = 0; i < 4; i++) {
    pinMode(mosfetPins[i], OUTPUT);
    digitalWrite(mosfetPins[i], HIGH);
  }

  ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 25000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  for (int i = 0; i < 4; i++) {
    ledc_channel_config_t channel = {
      .gpio_num = pwmPins[i],
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = pwmChannels[i],
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_0,
      .duty = pwmDuty,
      .hpoint = 0
    };
    ledc_channel_config(&channel);
  }
  for (int i = 0; i < 4; i++) {
    pcnt_config_t pcnt_config = {
      .pulse_gpio_num = tachPins[i],
      .ctrl_gpio_num = PCNT_PIN_NOT_USED,
      .lctrl_mode = PCNT_MODE_KEEP,
      .hctrl_mode = PCNT_MODE_KEEP,
      .pos_mode = PCNT_COUNT_INC,
      .neg_mode = PCNT_COUNT_DIS,
      .counter_h_lim = 10000,
      .counter_l_lim = 0,
      .unit = pcntUnits[i],
      .channel = PCNT_CHANNEL_0
    };
    pcnt_unit_config(&pcnt_config);
    pcnt_set_filter_value(pcntUnits[i], 1000);
    pcnt_filter_enable(pcntUnits[i]);
    pcnt_counter_pause(pcntUnits[i]);
    pcnt_counter_clear(pcntUnits[i]);
    pcnt_counter_resume(pcntUnits[i]);
  }
}

void handleClientTask(void *parameter) {
  for (;;) {
    dnsServer.processNextRequest();
    delay(DNS_INTERVAL);
    vTaskDelay(1);
  }
}

void setup() {
  pinMode(2, OUTPUT);


  Serial.setTxBufferSize(1024);
  Serial.begin(115200);

  while (!Serial)
    ;


  Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");
  Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

  // ...
  setupController();
  startSoftAccessPoint(ssid, password, localIP, gatewayIP);
  setUpDNSServer(dnsServer, localIP);
  setUpWebserver(server, localIP);
  server.begin();

  Serial.print("\n");
  Serial.print("Startup Time:");
  Serial.println(millis());
  Serial.print("\n");

  xTaskCreatePinnedToCore(
    handleClientTask,  // Task function
    "WebServerTask",   // Task name
    4096,              // Stack size
    NULL,              // Task input parameter
    1,                 // Priority (1 is low, higher = more)
    NULL,              // Task handle
    0                  // Core 0
  );
}

void loop() {
  delay(1000);

  int16_t count;

  Serial.print("RPMs: ");
  for (int i = 0; i < 4; i++) {
    pcnt_get_counter_value(pcntUnits[i], &count);
    lastRPM[i] = count * 30;
    pcnt_counter_clear(pcntUnits[i]);
    Serial.print("Fan ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(lastRPM[i]);
    Serial.print("  ");
  }
  Serial.println();
}