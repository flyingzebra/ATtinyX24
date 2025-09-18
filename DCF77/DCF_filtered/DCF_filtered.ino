#define DCF_GRAPH_DEBUG
const uint8_t bitmap_fan_16x16[] PROGMEM = {
    0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80
  };

#include "OLEDSerial.h"
#include "DCF77.h"

OLEDSerial oSerial;
DCF77Decoder dcf;


void setup()
{
  pinMode(2, OUTPUT);
  oSerial.begin(9600);
  dcf.setpin(3);
  dcf.begin();
}

char buf[20];
uint8_t draw_x = 0;

void loop()
{
  bool level = digitalRead(3);
  digitalWrite(2,level);

  dcf.update();

 

        uint8_t y=0;
        uint8_t page = y / 8;             // which 8-pixel-high page
        uint8_t y0 = y % 8;               // bit within the page

        oled.setCursor(draw_x,page);
        oled.startData();
        oled.sendData(pgm_read_byte(&bitmap_fan_16x16[y0+1]));
        oled.endData();
        draw_x++; if(draw_x>64) draw_x=0;



  if (dcf.frameReady())
  {
      auto frame = dcf.getFrame();
      if (frame.valid)
      {
          uint32_t high = 0, low = 0;
          for (int i = 0; i < 30; i++)
              high = (high << 1) | (frame.bits[i] & 1);
          for (int i = 30; i < 60; i++)
              low = (low << 1) | (frame.bits[i] & 1);

          snprintf(buf, sizeof(buf), "%08lX%08lX", high, low);
          oSerial.println("Frame: ");
          oSerial.println(buf);
      }
      else 
        oSerial.println("Finvalid");
  }
  else 
  {
    auto mon = dcf.monitor();
    if (dcf.isCalibrated()==false)  // prints only once per pulse
    {
        //snprintf(buf, sizeof(buf), "0-%d 1-%d", mon.avg0>>1, mon.avg1>>1);
        //oSerial.println(buf);
    }
  }
}