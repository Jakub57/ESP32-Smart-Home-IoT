/**
 * Projekt: EWSI - moduł bezprzewodowego sensora wilgotności i temperatury
 * Moduł: Sensor ESP32 (Temperatura i Wilgotność)
 * Autorzy: Jakub KochańczykMaciej Bębenek
 * Data: 05.2026
 * Wersja: 1.4.0
 * Opis: Moduł czujnika DHT22 zoptymalizowany pod kątem oszczędności energii (Deep Sleep).
 *       Obsługuje dynamiczną konfigurację WiFi i Home Assistant przez portal AP.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <driver/rtc_io.h>
#include "web_pages.h"

#define DEBUG 1 // Zakomentuj tę linię, aby wyłączyć tryb debugowania

// --- KONFIGURACJA CZASU UŚPIENIA ---
#ifdef DEBUG
  #define TIME_TO_SLEEP  30   // 30 sekund w trybie debug
#else
  #define TIME_TO_SLEEP  300  // 5 minut w trybie normalnym
#endif
#define uS_TO_S_FACTOR 1000000ULL
#define CONFIG_TIME 180 

// --- KONFIGURACJA SPRZĘTOWA ---
const int CONFIG_BUTTON_PIN = 0;
const int STATUS_LED_PIN = 2; // Wbodowana dioda D2 

// --- ZMIENNE GLOBALNE I OBIEKTY ---
RTC_DATA_ATTR uint8_t bssid[6];
RTC_DATA_ATTR int wifi_channel = 0;
RTC_DATA_ATTR float filter_temp = NAN;
RTC_DATA_ATTR float filter_hum = NAN;
Preferences prefs;

String ssid, pass;
String ha_ip;
bool use_static_ip = false;
String static_ip_str, gateway_str, subnet_str;

const char* webhookPath = ":8123/api/webhook/e22_sensors";
#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);
DNSServer dnsServer;

float hum_global = NAN;
float temp_global = NAN;

// --- FUNKCJE SYSTEMOWE ---

void goToSleep() {
    #ifdef DEBUG
        Serial.printf("Czas wybudzenia (aktywności): %lu ms\n", millis());
        Serial.printf("Dobranoc. Uśpienie na: %d s.\n", TIME_TO_SLEEP);
        Serial.flush(); 
    #endif
    // Sygnalizacja końca aktywności
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(50);
    digitalWrite(STATUS_LED_PIN, LOW);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop(); // Wyłączenie Bluetooth (oszczędność prądu i stabilność)
    
    esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP * uS_TO_S_FACTOR);
    
    delay(100); // Czas na ustabilizowanie napięcia po wyłączeniu radia
    esp_deep_sleep_start();
}

// --- TRYB KONFIGURACJI (AP) ---

void startConfigMode() {
    #ifdef DEBUG
        Serial.println("Tryb konfiguracyjny: E22_Sensor_Config");
    #endif
    WiFi.mode(WIFI_AP);
    WiFi.softAP("E22_Sensor_Config");
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    server.on("/", []() {
        server.send(200, "text/html", CONFIG_HTML);
    });

    server.on("/save", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("ha_ip")) {
            prefs.begin("config", false);
            prefs.putString("ssid", server.arg("ssid"));
            prefs.putString("pass", server.arg("pass"));
            prefs.putString("ha_ip", server.arg("ha_ip"));
            
            bool staticMode = server.hasArg("use_static");
            prefs.putBool("use_static", staticMode);
            
            if (staticMode) {
                prefs.putString("static_ip", server.arg("static_ip"));
                prefs.putString("gateway", server.arg("gateway"));
                prefs.putString("subnet", server.arg("subnet"));
            }
            
            prefs.end();
            server.send(200, "text/plain", "Zapisano. Restartowanie urządzenia...");
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "Błąd: SSID i adres IP HA są wymagane!");
        }
    });

    server.begin();
    unsigned long start = millis();
    while (millis() - start < CONFIG_TIME * 1000) { 
        dnsServer.processNextRequest();
        server.handleClient();
        delay(10);
    }
    goToSleep();
}

// --- GŁÓWNA KONFIGURACJA (SETUP) ---

void setup() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(50);
    digitalWrite(STATUS_LED_PIN, LOW);

    #ifdef DEBUG
        Serial.begin(115200);
        delay(500); // Czas na stabilizację Seriala
        Serial.println("\n--- START MODUŁU SENSORA ---");
        
        esp_reset_reason_t reason = esp_reset_reason();
        Serial.print("Powód resetu: ");
        Serial.println(reason);
        
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        Serial.print("Źródło wybudzenia: ");
        Serial.println(wakeup_reason);
    #endif

    // --- ODCZYT DHT Z FILTRACJĄ EMA ---
    dht.begin();
    delay(1000); // Czas na rozruch DHT22
    temp_global = dht.readTemperature();
    hum_global = dht.readHumidity();

    if (!isnan(temp_global) && !isnan(hum_global)) {
        if (isnan(filter_temp)) {
            filter_temp = temp_global;
            filter_hum = hum_global;
        } else {
            // Współczynnik 0.3 (30% nowej próbki, 70% historii) - płynne wygładzanie
            filter_temp = (temp_global * 0.3f) + (filter_temp * 0.7f);
            filter_hum = (hum_global * 0.3f) + (filter_hum * 0.7f);
        }
        temp_global = filter_temp;
        hum_global = filter_hum;
        #ifdef DEBUG
            Serial.printf("Pomiar OK (Filtrowany) -> T: %.1f, H: %.1f\n", temp_global, hum_global);
        #endif
    } else {
        #ifdef DEBUG
            Serial.println("Błąd odczytu DHT22 (NaN)!");
        #endif
    }
    
    pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
    
    // --- OBSŁUGA WYBUDZENIA PRZYCISKIEM ---
    esp_sleep_wakeup_cause_t wakeup_reason_btn = esp_sleep_get_wakeup_cause();
    
    // Jeśli wybudzony przyciskiem LUB przycisk jest wciśnięty przy starcie
    if (wakeup_reason_btn == ESP_SLEEP_WAKEUP_EXT0 || digitalRead(CONFIG_BUTTON_PIN) == LOW) {
        #ifdef DEBUG
            Serial.println("Wykryto przycisk. Trzymaj 3s dla KONFIGURACJI...");
        #endif
        
        unsigned long startHold = millis();
        bool heldLongEnough = true;
        
        while (millis() - startHold < 3000) {
            if (digitalRead(CONFIG_BUTTON_PIN) == HIGH) { // Puszczono przycisk
                heldLongEnough = false;
                #ifdef DEBUG
                    Serial.println("Puszczono za wcześnie. Normalny pomiar.");
                #endif
                break;
            }
            delay(10);
        }
        
        if (heldLongEnough) {
            startConfigMode();
        }
    }

    prefs.begin("config", true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    ha_ip = prefs.getString("ha_ip", "");
    use_static_ip = prefs.getBool("use_static", false);
    if (use_static_ip) {
        static_ip_str = prefs.getString("static_ip", "");
        gateway_str = prefs.getString("gateway", "");
        subnet_str = prefs.getString("subnet", "");
    }
    prefs.end();

    if (ssid == "" || ha_ip == "") {
        startConfigMode();
    }

    // --- ŁĄCZENIE Z WIFI ---
    if (use_static_ip && static_ip_str != "") {
        IPAddress local_IP, gateway, subnet;
        if (local_IP.fromString(static_ip_str) && gateway.fromString(gateway_str) && subnet.fromString(subnet_str)) {
            WiFi.config(local_IP, gateway, subnet);
            #ifdef DEBUG
                Serial.println("Używam statycznego adresu IP.");
            #endif
        }
    }

    if (wifi_channel > 0) {
        WiFi.begin(ssid.c_str(), pass.c_str(), wifi_channel, bssid);
    } else {
        WiFi.begin(ssid.c_str(), pass.c_str());
    }

    int counter = 0;
    #ifdef DEBUG
        Serial.print("WiFi: ");
    #endif
    while (WiFi.status() != WL_CONNECTED && counter < 40) {
        delay(100);
        counter++;
        #ifdef DEBUG
            Serial.print(".");
        #endif
    }

    // Fallback na DHCP jeśli Static IP zawiodło
    if (WiFi.status() != WL_CONNECTED && use_static_ip) {
        #ifdef DEBUG
            Serial.println("\nStatyczne IP zawiodło. Przełączam na DHCP...");
        #endif
        WiFi.disconnect();
        WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        counter = 0;
        while (WiFi.status() != WL_CONNECTED && counter < 40) {
            delay(100);
            counter++;
            #ifdef DEBUG
                Serial.print(".");
            #endif
        }
    }
    
    #ifdef DEBUG
        Serial.println();
    #endif

    if (WiFi.status() == WL_CONNECTED) {
        #ifdef DEBUG
            Serial.print("Połączono! IP: ");
            Serial.println(WiFi.localIP());
        #endif
        wifi_channel = WiFi.channel();
        memcpy(bssid, WiFi.BSSID(), 6);
        
        if (!isnan(hum_global) && !isnan(temp_global)) {
            HTTPClient http;
            String fullUrl = "http://" + ha_ip + webhookPath;
            http.begin(fullUrl);
            http.addHeader("Content-Type", "application/json");
            String jsonPayload = "{\"temperature\":" + String(temp_global) + ",\"humidity\":" + String(hum_global) + "}";
            
            #ifdef DEBUG
                Serial.printf("Wysyłanie do %s: %s\n", fullUrl.c_str(), jsonPayload.c_str());
            #endif

            int code = http.POST(jsonPayload);
            #ifdef DEBUG
                if (code > 0) {
                    Serial.printf("Sukces, kod HTTP: %d\n", code);
                } else {
                    Serial.printf("Błąd HTTP: %s\n", http.errorToString(code).c_str());
                }
            #endif
            http.end();
        } else {
            #ifdef DEBUG
                Serial.println("Błąd: Nie udało się odczytać DHT22 (NaN) przed wysyłką.");
            #endif
        }
    } else {
        #ifdef DEBUG
            Serial.println("Brak połączenia WiFi.");
        #endif
    }
    
    // Włączamy wybudzanie pinem przed uśpieniem
    rtc_gpio_pullup_en((gpio_num_t)CONFIG_BUTTON_PIN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)CONFIG_BUTTON_PIN, 0); // 0 = LOW
    goToSleep();
}

void loop() {}
