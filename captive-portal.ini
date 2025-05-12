#include <WiFi.h>
#include <WebServer.h>
#include <driver/pcnt.h>
#include <driver/ledc.h>
#include <DNSServer.h>
#include <Preferences.h>

#define PULSES_PER_REV 2


const int pwmPins[4] = { 32, 33, 27, 19 };
const int tachPins[4] = { 15, 16, 17, 18 };
const int mosfetPins[4] = { 14, 24, 13, 26 };  // Always ON

const ledc_channel_t pwmChannels[4] = {
  LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};
const pcnt_unit_t pcntUnits[4] = {
  PCNT_UNIT_0, PCNT_UNIT_1, PCNT_UNIT_2, PCNT_UNIT_3
};

uint32_t lastRPM[4] = { 0 };

const char* ssid = "ESP32-Portal";
const char* password = "12345678";

DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1);

WebServer server(80);
Preferences preferences;

// HTML content
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Fan Controller</title>
  <style>
    body {
      font-family: sans-serif;
      background: #111;
      color: #eee;
      text-align: center;
      padding: 20px;
    }
    .slider {
      width: 80%;
      margin: 20px auto;
    }
    .rpm {
      margin: 10px;
      font-size: 1.2em;
    }
    .label {
      margin-top: 30px;
    }
    input[type=range] {
      width: 100%;
    }
    button {
      padding: 10px 20px;
      font-size: 1em;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <h1>ESP32 Fan Controller</h1>

  <div id="rpmContainer">
    <div class="rpm">Fan 1 RPM: <span id="rpm1">-</span></div>
    <div class="rpm">Fan 2 RPM: <span id="rpm2">-</span></div>
    <div class="rpm">Fan 3 RPM: <span id="rpm3">-</span></div>
    <div class="rpm">Fan 4 RPM: <span id="rpm4">-</span></div>
  </div>

  <div class="label">Set PWM Duty Cycle:</div>
  <div class="slider">
    <input type="range" min="1" max="100" value="30" id="pwmSlider" oninput="updatePWM(this.value)">
    <div>Current PWM: <span id="pwmValue">30</span>%</div>
    <button onclick="savePWM()">Save</button>
  </div>

  <script>
    function updatePWM(val) {
      document.getElementById("pwmValue").innerText = val;
    }

    function savePWM() {
      const pwm = document.getElementById("pwmSlider").value;
      fetch("/savePWM?value="+pwm)
    }

    // Simulate live RPM updates (mock)
    setInterval(() => {
      fetch("/getRPM").then(res => res.json()).then(data => {
        console.log(data);
        for (let i = 1; i <= 4; i++) {
          document.getElementById("rpm" + i).innerText = data[i-1];
        }
      })
    }, 1000);

    window.addEventListener("load", ()=>{
      fetch("/getPWM").then(res => res.json()).then(data => {
        console.log(data);
        document.getElementById("pwmSlider").value = data.value
        updatePWM(data.value)
      })
    })
  </script>
</body>
</html>

)rawliteral";

// Route handler
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleGetRPM() {
  String json = "[";
  for (int i = 0; i < 4; i++) {
    json += String(lastRPM[i]);
    if (i < 3) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleGetPWM(){
  preferences.begin("fan", true);
  int pwmPercent = preferences.getInt("pwm", 30);
  preferences.end();
  server.send(200, "application/json", "{ \"value\": " + String(pwmPercent) + " }");
}



void handleClientTask(void* parameter) {
  for (;;) {
    dnsServer.processNextRequest();
    server.handleClient();
    vTaskDelay(1);  // Yield to other tasks
  }
}

void setupPWMSetup(){

  preferences.begin("fan", true);
  int pwmPercent = preferences.getInt("pwm", 30);
  preferences.end();

  int pwmDuty = (255 * pwmPercent) / 100;

  // // PWM setup
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

  // // PCNT setup for RPM
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

// changes pins as it fixed my esp from restarting
// getPWM route create to update the slider value when restarted or reloaded page
// save function working (not live tested)
// server started on another thread so that it will not cause any problem while delaying main loop
// each time your PWM updates through web server, then it recalls the setupRPM and setupPWM, but I found a method ` ledcWrite(pwmChannels[i], val); `

void handleSavePWM() {
  if (server.hasArg("value")) {
    int val = server.arg("value").toInt();
    Serial.print("Value Save PWM");
    Serial.println(val);
    val = constrain(val, 1, 100);

    preferences.begin("fan", false);
    preferences.putInt("pwm", val);
    preferences.end();

    for (int i = 0; i < 4; i++) {
      ledcWrite(pwmChannels[i], val);
    }
  }
  server.send(200, "application/json", "");
}


void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(mosfetPins[i], OUTPUT);
    digitalWrite(mosfetPins[i], HIGH);
  }

  setupPWMSetup();

  WiFi.softAP(ssid, password);
  delay(100);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(53, "*", apIP);

  Serial.println("Access Point Started");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Define web route
  server.on("/", handleRoot);
  server.on("/savePWM", handleSavePWM);
  server.on("/getRPM", handleGetRPM);

  server.on("/getPWM", handleGetPWM);

  // fallbacks (required for popup shoing for the portal opening)
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  // Captive portal support (fallback)
  server.on("/generate_204", []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/library/test/success.html", handleRoot);
  server.on("/ncsi.txt", handleRoot);
  server.on("/redirect", []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  
  server.begin();
  Serial.println("HTTP server started");

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
  delay(500);
  int16_t count;
  for (int i = 0; i < 4; i++) {
    pcnt_get_counter_value(pcntUnits[i], &count);
    lastRPM[i] = count * 30;  // 2 pulses/rev × 60 = ×30
    pcnt_counter_clear(pcntUnits[i]);
    Serial.print("Fan ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(lastRPM[i]);
    Serial.print("  ");
  }
  Serial.println();
}
