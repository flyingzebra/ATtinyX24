#define OLED_HEIGHT 32
#define OLED_WIDTH  64
#define OLED_WIPER

#include "Simple_OLEDscope.h"

OLEDscope scope;

void setup()
{
    pinMode(PIN_PA4, INPUT);
    pinMode(10,OUTPUT);
    oled.begin(OLED_WIDTH, OLED_HEIGHT, sizeof(tiny4koled_init_64x32r), tiny4koled_init_64x32r);
}

void loop()
{
    delay(12);
    uint8_t y = analogRead(PIN_PA4)>>2;
    if(scope.update(y) > 512)
         digitalWrite(10,HIGH);
    else digitalWrite(10,LOW);
}
