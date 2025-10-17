#include <M5Stack.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

Adafruit_SHT31 sht30 = Adafruit_SHT31();

void setup() {
  M5.begin();
  Serial.begin(115200);

  if (!sht30.begin(0x44)) {  // Dirección I2C común del SHT30 es 0x44
    Serial.println("No se pudo encontrar un sensor SHT30 válido");
    M5.Lcd.println("Sensor SHT30 no encontrado");
    while (1);
  }
  M5.Lcd.println("Sensor SHT30 listo!");
}

void loop() {
  float temperatura = sht30.readTemperature();
  float humedad = sht30.readHumidity();

  if (!isnan(temperatura) && !isnan(humedad)) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Temp: %.2f C\n", temperatura);
    M5.Lcd.printf("Hum: %.2f %%\n", humedad);
  } else {
    M5.Lcd.println("Error leyendo sensor");
  }
  delay(2000);
}
