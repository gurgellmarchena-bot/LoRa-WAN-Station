#include <Arduino.h>
#include <M5UnitENV.h>
#include <M5Unified.h>
#include <M5GFX.h>  // Opcional para fuentes personalizadas, pero incluido por compatibilidad
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>


SHT3X sht30;    // Sensor de temperatura y humedad
QMP6988 qmp6988;  // Sensor de presión


#define LED_TWO  25
// Claus OTAA (substitueix-les per les teves de TTN)
static const u1_t PROGMEM APPEUI[8] = { 0x0C, 0x0B, 0x0A, 0x05, 0x04, 0x03, 0x02, 0x01 };
static const u1_t PROGMEM DEVEUI[8] = { 0x5F, 0x19, 0x07, 0xD0, 0x7E, 0xD5, 0xB3, 0x90 }; // De moment he canviat el MSB per cada dispositiu programat
static const u1_t PROGMEM APPKEY[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,0x08 ,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00 };

void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

static osjob_t sendjob;
const unsigned TX_INTERVAL = 60; // segons
unsigned long lastTxStart =0;
bool LCDState=1;
const unsigned TX_TIMEOUT=15000;

const lmic_pinmap lmic_pins = {
    .nss = 5,      // SX1276 CSp
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 26,      // SX1276 RESET
    .dio = {36, 35, LMIC_UNUSED_PIN} // DIO0, DIO1, DIO2
};


unsigned long startTime = 0;  // Tiempo de inicio
const unsigned long dimTimeout = 10000;
unsigned long lastActivity = 0;  
const unsigned long inactivityTimeout = 30000;

uint8_t battery = M5.Power.getBatteryLevel();

int allWhite = !allWhite;
int dimmed;

int temp, hum, pres;

void do_send(osjob_t* j) {

  if (LMIC.opmode & OP_TXRXPEND) {
    
    unsigned long now = millis();
    if (now - lastTxStart>TX_TIMEOUT){
      Serial.println(F("TimeOut: Tx Stuck. Resetting LMIC state.\n"));
      LMIC_reset();
      LMIC_startJoining();
    }else{
      Serial.println(F("TX en curs, esperant..."));
    }
  }  else {
/*     int randomNumber = random(0, 1000);
    byte payload[2] = { highByte(randomNumber), lowByte(randomNumber) }; */
    sht30.update();
    qmp6988.update();
    float temp =sht30.cTemp;
    float hum= sht30.humidity;
    float pres = qmp6988.calcPressure();
    float alt = qmp6988.altitude;
    int16_t bat=M5.Power.getBatteryLevel();
      

    int16_t t=temp*100;
    int16_t h= hum*100;
    uint16_t p = pres/10; //hPa *10
    uint16_t a = alt;
    uint16_t b= bat;

    byte payload[10]={
      highByte(t), lowByte(t),
      highByte(h), lowByte(h),
      highByte(p), lowByte(p),
      highByte(a), lowByte(a),
      highByte(b), lowByte(b)

    };

    lastTxStart=millis();
    LMIC_setTxData2(1, payload, sizeof(payload), 0);
  
    M5.update();
   
    M5.Display.setCursor(0,30);
    M5.Display.printf("Temp   : %.2f %cC \n", temp, 247);  // Error símbol de grau centígrad pendent de trobar per l'M5 Display
    M5.Display.printf("Hum    : %.2f %%\n",hum);
    M5.Display.printf("Pressio: %.1f hPa\n",pres/100);
    M5.Display.printf("Altitud: %.1f m\n",alt);
    M5.Display.printf("Battery: %d%%",bat );
    Serial.printf("Altres esdeveniments t= %.2f °C h= %.2f %% i P= %.1f hPa Alt=%.1f\n", temp,hum,pres, alt);
  }
  os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);

}

void onEvent(ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_JOINING: Serial.println(F("Connectant a TTN...")); break;
    case EV_JOINED: 
      Serial.println(F("Connectat!")); 
      do_send(&sendjob);
      break;
    case EV_TXCOMPLETE: Serial.println(F("Missatge enviat!")); break;
    default: Serial.println(F("Altres esdeveniments")); break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // Inicializar los sensores ENV III con Wire (I2C inicializado por M5.begin()).
  if (!sht30.begin(&Wire, 0x44)) {
    Serial.println("Error inicializando SHT30");
    while (1) delay(1);  // Detener si falla
  }
  if (!qmp6988.begin(&Wire, 0x70)) {
    Serial.println("Error inicializando QMP6988");
    while (1) delay(1);  // Detener si falla
  }
  
  M5.Lcd.setRotation(1); // Opcional: Rotar la pantalla para mejor visualización
  M5.Lcd.setFont(&fonts::FreeSansBold12pt7b);
  M5.Lcd.setTextDatum(top_center);
  M5.Lcd.setCursor(M5.Lcd.width() / 2, 0);
  //M5.Lcd.println("Sensor ENV III");
Wire.begin(21,22);
  Wire.setClock(10000);
  bool qmp6988_ok = qmp6988.begin(&Wire,QMP6988_SLAVE_ADDRESS_L, 21, 22, 100000U);
  bool sht30_ok= sht30.begin(&Wire, SHT3X_I2C_ADDR,21,22, 100000U);
  M5.Display.setCursor(0,70);
  if (!qmp6988_ok)  M5.Display.println("QMP Errors");
  if (!sht30_ok)  M5.Display.println("SHT3X Errors");
  delay(1000);

  os_init();
  LMIC_reset();
  LMIC_startJoining();
  M5.Display.clear();
  btStop();
  //Wifi.mode(RSSI_OFF)
  
  /*for (unsigned char i=240; i<255; i++){
    M5.Display.setCursor(0,60);
    M5.Display.printf("Char  %c num %i \n   ", i,i);
    delay(1000);
  }*/



}

void loop() {


  M5.update();  // Actualizar el estado de los botones y touch
  os_runloop_once();

  uint8_t battery = M5.Power.getBatteryLevel();

  // Verificar si hay actividad (presión de botones)
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
    lastActivity = millis();
  }

  // Verificar si ha pasado el tiempo de inactividad
  if (millis() - lastActivity > inactivityTimeout) {
    M5.Power.deepSleep();  // Entrar en modo deep sleep (mínimo consumo de energía)
  }

  if (allWhite = false || M5.BtnA.wasPressed()) {
    allWhite = true;
    // Reiniciar temporizadores para evitar dim/sleep inmediato
    startTime = millis();
    dimmed = false;
    M5.Lcd.setBrightness(255);  // Restaurar brillo máximo al interactuar
}
  else {
   allWhite = false; 
  }

  float temp = 0.0, hum = 0.0, pres = 0.0;
  
  // Actualizar y leer los valores del sensor
  if (sht30.update() && qmp6988.update()) {
    temp = sht30.cTemp;
    hum = sht30.humidity;
    pres = qmp6988.pressure / 100.0f;  // Convertir de Pa a hPa

    if (!dimmed && (millis() - startTime > dimTimeout)) {
    M5.Lcd.setBrightness(50);  // Bajar el brillo a 50 (ajusta este valor según prefieras, 0-255)
    dimmed = true;
    }
    


    M5.Lcd.fillRect(0, 30, M5.Lcd.width(), M5.Lcd.height() - 30, TFT_BLACK); // Limpiar el área de datos
    
   if (allWhite) {
      M5.Lcd.setTextColor(TFT_WHITE);
    } else {
      M5.Lcd.setTextColor(TFT_RED); 
    }

    M5.Lcd.setCursor(M5.Lcd.width() / 20, 80);
    M5.Lcd.printf("Temperatura: %.2f °C", temp);
    
    if (allWhite) {
      M5.Lcd.setTextColor(TFT_WHITE);
    } else {
      M5.Lcd.setTextColor(TFT_SKYBLUE); 
    }

    M5.Lcd.setCursor(M5.Lcd.width() / 20, 120);
    M5.Lcd.printf("Humedad: %.2f %%", hum);
    
    if (allWhite) {
      M5.Lcd.setTextColor(TFT_WHITE);
    } else {
      M5.Lcd.setTextColor(TFT_PURPLE); 
    }

    M5.Lcd.setCursor(M5.Lcd.width() / 20, 160);
    M5.Lcd.printf("Presion: %.2f hPa", pres);
    
    if (allWhite) {
      M5.Lcd.setTextColor(TFT_WHITE);
    } else {
      M5.Lcd.setTextColor(TFT_GREEN); 
    }

    M5.Lcd.setCursor(M5.Lcd.width() / 20, 40);
    M5.Lcd.printf("Bateria: %d %%", battery);

    //M5.Lcd.setTextColor(TFT_WHITE); 
    //M5.Lcd.setCursor(M5.Lcd.width() / 20, 180);
    //M5.Lcd.printf("Tonto quien lo lea");
  } else {
    M5.Lcd.fillRect(0, 30, M5.Lcd.width(), M5.Lcd.height() - 30, TFT_BLACK);
    M5.Lcd.setCursor(M5.Lcd.width() / 20, 60);
    M5.Lcd.println("Error de lectura del sensor.");
  }
  
  delay(1000); // Esperar 2 segundos antes de la siguiente lectura
}