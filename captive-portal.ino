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
uint32_t lastRPM[4] = {0};

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
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });

	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });

	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");
		request->send(response);
		Serial.println("Served Basic HTML Page");
	});

    // apis
    server.on("/toogle-master", [](AsyncWebServerRequest *request){
        // set all the mosfets to off
        for (int i = 0; i < 4; i++) {
            if(masterOn){
                digitalWrite(mosfetPins[i], LOW);
            } else {
                digitalWrite(mosfetPins[i], HIGH);
            }
        }
        masterOn = !masterOn;

        request->send(200, "application/json", "{ \"success\": \"true\" }");
    });

    server.on("/fan-info", [](AsyncWebServerRequest *request){
        preferences.begin("speed", true);
        String json = "[";
        for (int i = 0; i < 4; i++) {
            int fanSpeed = preferences.getInt("fan" + char(i), 30);
            json += "{ \"rpm\":";
            json += String(lastRPM[i]);
            json += ",\"rpm\": \"";
            json += String(digitalRead(mosfetPins[i]));
            json += "\",\"speed\":\"";
            json += String(fanSpeed);
            json += "\"}";
            if (i < 3) json += ",";
        }
        json += "]";

        request->send(200, "application/json", json);
    });

    

    server.on("/server-stats", [](AsyncWebServerRequest *request) {
        // Empty for now...
    });

    server.on("/reset-prefs", [](AsyncWebServerRequest *request) {
        // ...
    });

    server.on("/update-speed", [](AsyncWebServerRequest *request) {
        if(request->hasArg("fanIndex") && request->hasArg("updateValue")){
            int fanIndex = request->arg("fanIndex").toInt();
            int updateValue = request->arg("updateValue").toInt();
            updateValue = constrain(updateValue, 1, 100);
            
            preferences.begin("speed", false);
            preferences.putInt("fan" + char(fanIndex), updateValue);
            preferences.end();

            //TODO: update the speed 
            ledcWrite(pwmChannels[fanIndex], updateValue);

            request->send(200, "application/json", "{ \"success\": \"true\" }");
        }
        request->send(400, "application/json", "{ \"error\": \"Failed to get query parameters.\" }");
    });

    server.on("/toogle-fan", [](AsyncWebServerRequest *request) {
        if(request->hasArg("fanIndex")){
            int fanIndex = request->arg("fanIndex").toInt();
            
            int state = digitalRead(mosfetPins[fanIndex]);

            if(state == 1){
                digitalWrite(mosfetPins[fanIndex], 0);
            } else {
                digitalWrite(mosfetPins[fanIndex], 1);
            }

            request->send(200, "application/json", "{ \"success\": \"true\" }");
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
void setupController(){
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

void setup(){
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

void loop(){
    delay(1000);

    int16_t count;

    Serial.print("RPMs: ");
    for(int i = 0; i < 4; i++) {
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