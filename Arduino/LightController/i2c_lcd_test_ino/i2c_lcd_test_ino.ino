#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR      0x27 // I2C address of PCF8574A
#define BACKLIGHT_PIN 3
#define En_pin        2
#define Rw_pin        1
#define Rs_pin        0
#define D4_pin        4
#define D5_pin        5
#define D6_pin        6
#define D7_pin        7

LiquidCrystal_I2C twilcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin, BACKLIGHT_PIN, POSITIVE);

#define TRIGGER_INIT_VALUE  0
#define INTERVAL_INIT_VALUE 1
#define INTERVAL_MAX_VALUE  512
#define EMBEDDED_LED_PIN    13
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
long interval = INTERVAL_INIT_VALUE;
byte trigger;
byte ledState = LOW;

//override printf output
int my_putc(char outtext, FILE *t){
  Serial.print( outtext );
  twilcd.print( outtext );
};
void setup(){
  fdevopen( &my_putc, 0 ); //override printf output, this allows to print to more than one output device
  Serial.begin(115200);
  Serial.println(F("Start..."));
  pinMode(EMBEDDED_LED_PIN, OUTPUT);
  trigger = TRIGGER_INIT_VALUE;
  twilcd.begin(20,4);
  twilcd.home();
  //1234567890123456
  //I2C/TWI BackPack
  twilcd.print("I2C/TWI BackPack");
  twilcd.setBacklight(HIGH);
  delay(3000);
  twilcd.setBacklight(LOW);
  delay(3000);
  twilcd.setBacklight(HIGH);
  delay(3000);
  twilcd.setBacklight(LOW);
  delay(3000);
  twilcd.setBacklight(HIGH);
  Serial.println(F("Blinking done."));
  twilcd.clear();
}

byte bcdToDec(byte val)
{
  return ( (val/16*10) + (val%16) );
}

void loop() {
  // put your main code here, to run repeatedly:
twilcd.setCursor (0,0);
printf("Cheese1");
twilcd.setCursor (0,1);
printf("Cheese2");
}
