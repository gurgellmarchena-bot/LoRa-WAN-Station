#include <Arduino.h>
#include <M5UnitENV.h>
#include <M5Unified.h>
#include <M5GFX.h>  // Opcional para fuentes personalizadas, pero incluido por compatibilidad

SHT3X sht30;    // Sensor de temperatura y humedad
QMP6988 qmp6988;  // Sensor de presión

unsigned long startTime = 0;  // Tiempo de inicio
const unsigned long dimTimeout = 10000;
unsigned long lastActivity = 0;  
const unsigned long inactivityTimeout = 30000;

uint8_t battery = M5.Power.getBatteryLevel();

int allWhite = !allWhite;
int dimmed;

int temp, hum, pres;

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


}

void loop() {

  M5.update();  // Actualizar el estado de los botones y touch

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
  
  delay(2000); // Esperar 2 segundos antes de la siguiente lectura
}

