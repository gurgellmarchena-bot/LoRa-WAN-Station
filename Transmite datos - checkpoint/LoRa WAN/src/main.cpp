#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <M5UnitENV.h>
#include <M5Unified.h>

SHT3X sht3x;
QMP6988 qmp;

// --- CONFIGURACIÓ LoRaWAN ---
// RECORDA: APPEUI i DEVEUI en format LSB (llegit al revés, 0x01, 0x02...)
static const u1_t PROGMEM APPEUI[8] = { 0x0C, 0x0B, 0x0A, 0x05, 0x04, 0x03, 0x02, 0x01 };
static const u1_t PROGMEM DEVEUI[8] = { 0x5F, 0x19, 0x07, 0xD0, 0x7E, 0xD5, 0xB3, 0x90 }; 
// APPKEY en format MSB (tal qual surt a la web)
static const u1_t PROGMEM APPKEY[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00 };

void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

static osjob_t sendjob;
const unsigned TX_INTERVAL = 60; // segons
unsigned long lastTxStart = 0;
bool LCDState = true;
const unsigned TX_TIMEOUT = 15000;

const lmic_pinmap lmic_pins = {
    .nss = 5,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 26,
    .dio = {36, 35, LMIC_UNUSED_PIN} 
};

void do_send(osjob_t* j) {
    // Comprovem si hi ha feina pendent abans d'enviar res nou
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("TX en curs, esperant..."));
    } else {
        // 1. Llegir Sensors
        sht3x.update();
        qmp.update();
        
        float temp = sht3x.cTemp;
        float hum = sht3x.humidity;
        float pres = qmp.pressure / 100.0; // Assegura't que la llibreria usa .pressure o .calcPressure()
        float alt = qmp.altitude;
        int bat = M5.Power.getBatteryLevel();

        // 2. Preparar Payload
        int16_t t = temp * 100;
        int16_t h = hum * 100;
        uint16_t p = pres * 10;
        uint16_t a = alt;
        uint8_t b = bat;

        byte payload[10] = {
            highByte(t), lowByte(t),
            highByte(h), lowByte(h),
            highByte(p), lowByte(p),
            highByte(a), lowByte(a),
            0, b // El byte 9 el deixem a 0 o posem highByte(b) si bat fos > 255
        };

        // 3. Enviar LoRa
        LMIC_setTxData2(1, payload, sizeof(payload), 0);
        Serial.println(F("Paquet enviat a la cua..."));

        // 4. Actualitzar Pantalla
        if (LCDState) {
            M5.Display.clear(); // Neteja la pantalla per pintar dades noves
            
            M5.Display.setTextSize(2);
            M5.Display.setCursor(0, 30);
            
            M5.Display.setTextColor(TFT_RED);
            // Símbol de grau (char 248 sol ser millor que 247 en algunes fonts, prova-ho)
            M5.Display.printf("Temp   : %.2f %cC\n", temp, (char)248); 

            M5.Display.setTextColor(TFT_SKYBLUE);
            M5.Display.printf("Hum    : %.2f %%\n", hum);

            M5.Display.setTextColor(TFT_PURPLE);
            M5.Display.printf("Pressio: %.1f hPa\n", pres);

            M5.Display.setTextColor(TFT_GREEN);
            M5.Display.printf("Altitud: %.1f m\n", alt);

            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.printf("Bat: %d%%", bat);
        }
    }
    // Programar el següent enviament
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
}

void onEvent(ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev) {
        case EV_JOINING: 
            Serial.println(F("Connectant a TTN..."));
            // --- AQUESTA PART ÉS LA QUE MANCAVA ---
            M5.Display.clear();
            M5.Display.setCursor(0, 50);
            M5.Display.setTextColor(TFT_YELLOW);
            M5.Display.println("Connectant a TTN...");
            M5.Display.println("Espera si us plau.");
            break;
            
        case EV_JOINED: 
            Serial.println(F("Connectat!")); 
            M5.Display.clear();
            M5.Display.setCursor(0, 50);
            M5.Display.setTextColor(TFT_GREEN);
            M5.Display.println("Connectat a TTN!");
            delay(1000); // Petita pausa per veure el missatge
            
            // Desactivar comprovació de link per estalviar dades
            LMIC_setLinkCheckMode(0); 
            
            // Primer enviament de dades!
            do_send(&sendjob);
            break;
            
        case EV_TXCOMPLETE: 
            Serial.println(F("Missatge enviat + RX complet!")); 
            break;
            
        default: 
            Serial.printf("Event: %d\n", ev); 
            break;
    }
}

void setup() {
    M5.begin();
    M5.Display.setRotation(1);
    M5.Display.setTextSize(2);
    M5.Display.setBrightness(100);
    
    M5.Display.println("Inicialitzant...");
    Serial.begin(115200);

    // Inicialitzar I2C i Sensors
    Wire.begin(21, 22);
    
    if (!sht3x.begin(&Wire, SHT3X_I2C_ADDR, 21, 22, 100000U)) {
        M5.Display.setTextColor(TFT_RED);
        M5.Display.println("Error SHT3X");
    }
    if (!qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 21, 22, 100000U)) {
        M5.Display.setTextColor(TFT_RED);
        M5.Display.println("Error QMP6988");
    }
    
    delay(1000); // Temps per llegir errors si n'hi ha

    // Inicialitzar LoRa
    os_init();
    LMIC_reset();
    
    // CORRECCIÓ DE RELLOTGE (Molt important per ESP32)
    LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

    LMIC_startJoining();
    
    // Nota: NO fem M5.Display.clear() aquí. 
    // Deixem que l'event EV_JOINING gestioni la pantalla.
}

void loop() {
    M5.update();
    os_runloop_once();
    
    // He tret el delay(10) perquè pot interferir amb el timing de LoRaWAN
    // Si necessites delay, fes-ho molt petit (1ms) o millor res.

    if (M5.BtnB.wasClicked()) {
        if (LCDState) {
            M5.Display.clear();
            M5.Display.setCursor(0, 50);
            M5.Display.println("Pantalla OFF");
            delay(500);
            M5.Display.sleep();
            M5.Display.setBrightness(0);
            LCDState = false;
        } else {
            M5.Display.wakeup();
            M5.Display.setBrightness(100);
            M5.Display.clear();
            M5.Display.setCursor(0, 50);
            M5.Display.println("Pantalla ON");
            LCDState = true;
            // Forcem una actualització visual en el proper cicle si calgués
        }
    }
}