#ifndef OLED_WIDTH
#define OLED_WIDTH  128
#endif
#ifndef OLED_HEIGHT
#define OLED_HEIGHT 64
#endif
#define OLED_PAGES  (OLED_HEIGHT/8)


#include <Tiny4kOLED.h>

class OLEDscope
{
  public:
    OLEDscope() :  x(0), y0(0) {}

    void begin()
    {
        oled.enableChargePump();
        oled.clear();
        oled.on();
    }

    int update(uint8_t byte_value)      // value range = 0-255
    {
        uint8_t y1 = 31 - (byte_value >> 3);   // pixel position of y1
        vline(x, y0, y1);
        x++; y0 = y1; if(x == OLED_WIDTH) x = 0;
        return byte_value;
    }

  private:
    uint8_t x, y0;

    void strip(uint8_t x0, uint8_t y0, uint8_t byte)
    {
        oled.setCursor(x0, y0);
        oled.startData();
        if(x0 & 1) oled.sendData(byte); else oled.sendData(byte);
        #ifdef OLED_WIPER
        if(x0 & 1) oled.sendData(0b10101010); else oled.sendData(0b01010101);
        #endif
        oled.endData();
    }

    void stripe(uint8_t x_pix, uint8_t *buf)
    {
        switch (OLED_PAGES)
        {
            case  8: strip(x_pix, 7, buf[7]);
            case  7: strip(x_pix, 6, buf[6]);
            case  6: strip(x_pix, 5, buf[5]);
            case  5: strip(x_pix, 4, buf[4]);
            case  4: strip(x_pix, 3, buf[3]);
            case  3: strip(x_pix, 2, buf[2]);
            case  2: strip(x_pix, 1, buf[1]);
            case  1: strip(x_pix, 0, buf[0]);
            case  0: break;
        }
    }

    inline void vline(uint8_t x_pix, uint8_t y0_pix, uint8_t y1_pix)
    {
        uint8_t buf[OLED_PAGES] = {0};
        if(y1_pix < y0_pix) { uint8_t t = y1_pix; y1_pix = y0_pix; y0_pix = t; }
        uint8_t byte0 = y0_pix >> 3, byte1 = y1_pix >> 3;
        uint8_t bit0  = y0_pix & 7,  bit1  = y1_pix & 7;

        if (byte0 == byte1)
        {
            buf[byte0] = ((uint8_t)0xFF >> (7 - bit1)) & ((uint8_t)0xFF << bit0);
            stripe(x_pix, buf);
            return;
        }

        buf[byte0] = (uint8_t)0xFF << bit0;
        buf[byte1] |= (uint8_t)0xFF >> (7 - bit1);
        switch (byte1 - byte0 - 1)
        {
            case  8: buf[byte0+ 8] = 0xFF;
            case  7: buf[byte0+ 7] = 0xFF;
            case  6: buf[byte0+ 6] = 0xFF;
            case  5: buf[byte0+ 5] = 0xFF;
            case  4: buf[byte0+ 4] = 0xFF;
            case  3: buf[byte0+ 3] = 0xFF;
            case  2: buf[byte0+ 2] = 0xFF;
            case  1: buf[byte0+ 1] = 0xFF;
            case  0: break;
        }
        stripe(x_pix, buf);
    }
};
