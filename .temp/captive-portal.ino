#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <driver/pcnt.h>
#include <driver/ledc.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_wifi.h>

#define PULSES_PER_REV 2

// WIFI Setup start

const char *ssid = "captive";  // FYI The SSID can't have a space in it.
// const char * password = "12345678"; //Atleast 8 chars
const char *password = NULL;  // no password


#define MAX_CLIENTS 4   // ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6  // 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);           // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);         // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "/";  // a string version of the local IP with http, used for redirecting clients to your webpage

// HTML content
const char index_html[] PROGMEM = R"rawliteral(










)rawliteral";

DNSServer dnsServer;
AsyncWebServer server(80);
// WIFI Setup end

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

Preferences preferences;

// wifi setup

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

  // Set the TTL for DNS response and start the DNS server
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

  // Set the WiFi mode to access point and station
  WiFi.mode(WIFI_MODE_AP);

  // Define the subnet mask for the WiFi network
  const IPAddress subnetMask(255, 255, 255, 0);

  // Configure the soft access point with a specific IP and subnet mask
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

  // Start the soft access point with the given ssid, password, channel, max number of clients
  WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

  // Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
  my_config.ampdu_rx_enable = false;
  esp_wifi_init(&my_config);
  esp_wifi_start();
  vTaskDelay(100 / portTICK_PERIOD_MS);  // Add a small delay
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
  //======================== Webserver ========================
  // WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
  // SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
  // SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
  // SAFARI (IOS) popup browserÂ has some severe limitations (javascript disabled, cookies disabled)

  // Required
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) {
    request->redirect("http://logout.net");
  });  // windows 11 captive portal workaround
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) {
    request->send(404);
  });  // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

  // Background responses: Probably not all are Required, but some are. Others might speed things up?
  // A Tier (commonly used by modern systems)
  server.on("/generate_204", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });  // android captive portal redirect
  server.on("/redirect", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });  // microsoft redirect
  server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });  // apple call home
  server.on("/canonical.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });  // firefox captive portal call home
  server.on("/success.txt", [](AsyncWebServerRequest *request) {
    request->send(200);
  });  // firefox captive portal call home
  server.on("/ncsi.txt", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  });  // windows call home

  // B Tier (uncommon)
  //  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
  //  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
  //  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
  //  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

  // return 404 to webpage icon
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) {
    request->send(404);
  });  // webpage icon

  // Serve Basic HTML Page
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
    response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
    request->send(response);
    Serial.println("Served Basic HTML Page");
  });

  // the catch all
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
    Serial.print("onnotfound ");
    Serial.print(request->host());  // This gives some insight into whatever was being requested on the serial monitor
    Serial.print(" ");
    Serial.print(request->url());
    Serial.print(" sent redirect to " + localIPURL + "\n");
  });


  server.on("/savePWM", [](AsyncWebServerRequest *request) {
    if (request->hasArg("value")) {
      int val = request->arg("value").toInt();
      Serial.print("Value Save PWM");
      Serial.println(val);
      val = constrain(val, 1, 100);

      preferences.begin("fan", false);
      preferences.putInt("pwm", val);
      preferences.end();

      for (int i = 0; i < 4; i++) {
        ledcWrite(pwmChannels[i], val);
      }
      request->send(200, "text/html", "");
    }
  });

  server.on("/getRPM", [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int i = 0; i < 4; i++) {
      json += String(lastRPM[i]);
      if (i < 3) json += ",";
    }
    json += "]";
    request->send(200, "text/html", json);
  });

  server.on("/getPWM", [](AsyncWebServerRequest *request) {
    preferences.begin("fan", true);
    int pwmPercent = preferences.getInt("pwm", 30);
    preferences.end();
    request->send(200, "application/json", "{ \"value\": " + String(pwmPercent) + " }");
  });
}

// wifi setup end


void handleClientTask(void *parameter) {
  for (;;) {
    dnsServer.processNextRequest();  // I call this atleast every 10ms in my other projects (can be higher but I haven't tested it for stability)
    delay(DNS_INTERVAL);             // seems to help with stability, if you are doing other things in the loop this may not be needed
    vTaskDelay(1);                   // Yield to other tasks
  }
}

void setupPWMSetup() {

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


void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);

  // Wait for the Serial object to become available.
  while (!Serial)
    ;

  // Print a welcome message to the Serial port.
  Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");  //__DATE__ is provided by the platformio ide
  Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

  startSoftAccessPoint(ssid, password, localIP, gatewayIP);
  setUpDNSServer(dnsServer, localIP);

  setUpWebserver(server, localIP);
  server.begin();

  Serial.print("\n");
  Serial.print("Startup Time:");  // should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
  Serial.println(millis());
  Serial.print("\n");

  for (int i = 0; i < 4; i++) {
    pinMode(mosfetPins[i], OUTPUT);
    digitalWrite(mosfetPins[i], HIGH);
  }

  setupPWMSetup();

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
