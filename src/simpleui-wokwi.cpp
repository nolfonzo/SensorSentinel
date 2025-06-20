#include <Wire.h>
// #include "pintao.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "arduino.h"


// Definições do display OLED
#define SCREEN_WIDTH 128 // Largura do display OLED, em pixels
#define SCREEN_HEIGHT 64 // Altura do display OLED, em pixels

// Pinos do display OLED para a placa Heltec ESP32 LoRa V2
#define OLED_RESET 16
#define OLED_SDA 17
#define OLED_SCL 18

// Inicializa o display com as dimensões especificadas acima
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool meuBooleano;

void setup() {
 
  // Inicia a comunicação serial para depuração
  Serial.begin(115200);
  delay(1000); // Espera um pouco para a inicialização da serial

  // Configura os pinos SDA e SCL para o display OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  // Inicializa o display OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Endereço I2C 0x3C para o display 128x64
    Serial.println(F("Falha ao inicializar o display OLED"));
    while (true);
  }

  // Limpa o buffer do display
  display.clearDisplay();

  // Configura o tamanho da fonte
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Atualiza o display
  display.display();

//  meuBooleano = false;

}

void loop() {
  // Nada a fazer no loop
  display.clearDisplay();
//  display.drawBitmap(0, 0, imgDisplay_SSD1306, 128, 64, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.drawFastHLine(0, 52, 128, WHITE);
  display.drawFastHLine(0,0, 128, WHITE);
  display.write("FELIPE VIADAO");

  display.setCursor(27, 10);
  display.write("25%");

  display.fillRect(102, 5, 4, 8, WHITE);

 // if(!meuBooleano){
  //display.drawBitmap(84, 1, lockallArray[0], 13, 16, SSD1306_WHITE);
  //}else{
  //display.drawBitmap(84, 1, lockallArray[1], 13, 16, SSD1306_WHITE); 
 // }
 
  //display.drawBitmap(84, 1, lockallArray[0], 13, 16, SSD1306_WHITE);
  //display.drawBitmap(84, 1, lockallArray[1], 13, 16, SSD1306_WHITE);
  
  display.display();
  delay(1000);
 // meuBooleano = !meuBooleano;
}

