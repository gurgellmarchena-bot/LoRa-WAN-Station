#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <M5UnitENV.h>
#include <M5Unified.h>


SHT3X sht3x;
QMP6988 qmp;

#define LED_TWO  25
// Claus OTAA (substitueix-les per les teves de TTN)
static const u1_t PROGMEM APPEUI[8] = { 0x0C, 0x0B, 0x0A, 0x05, 0x04, 0x03, 0x02, 0x01 };
static const u1_t PROGMEM DEVEUI[8] = { 0x5F, 0x19, 0x07, 0xD0, 0x7E, 0xD5, 0xB3, 0x80 }; // De moment he canviat el MSB per cada dispositiu programat
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
  } else {
/*     int randomNumber = random(0, 1000);
    byte payload[2] = { highByte(randomNumber), lowByte(randomNumber) }; */
    sht3x.update();
    qmp.update();
    float temp =sht3x.cTemp;
    float hum= sht3x.humidity;
    float pres = qmp.calcPressure();
    float alt = qmp.altitude;
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

/// @brief 
void setup() {
  M5.begin();
  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0,30);
  M5.Display.setBrightness(100);
  M5.Display.println("Inicialitzant LCD i port sèrie");
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Inicialitzant port sèrie i LCD"));

  Wire.begin(21,22);
  Wire.setClock(10000);
  bool qmp_ok = qmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_L, 21, 22, 100000U);
  bool sht3x_ok= sht3x.begin(&Wire, SHT3X_I2C_ADDR,21,22, 100000U);
  M5.Display.setCursor(0,70);
  if (!qmp_ok)  M5.Display.println("QMP Errors");
  if (!sht3x_ok)  M5.Display.println("SHT3X Errors");
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
  M5.update();
  os_runloop_once();
  delay(10);

  if (M5.BtnB.wasClicked()){
    if (LCDState){
      M5.Display.println("Go to Sleep");     
      M5.Display.clear();
      M5.Display.sleep();
      M5.Display.powerSaveOn();
      LCDState=!LCDState;
      setCpuFrequencyMhz(40);
      M5.Display.setBrightness(0);

    }else{
      M5.Display.wakeup();
      M5.Display.powerSaveOff();
      M5.Display.setBrightness(128);
      M5.Display.println("Wake up");
      LCDState=!LCDState;
      M5.Display.clear();
    }
    delay(1000);
  }


}


