#include <avr/io.h>
#include <avr/interrupt.h>
#include "OLEDSerial.h"

OLEDSerial oSerial;

// ==== CONFIGURATION ====
#define MEAS_PIN   PIN_PA4   // configurable input pin
#define PINCTRL    PORTA.PIN2CTRL  // pin control register
#define POLARITY_HIGH 1      // pulse = HIGH

#define LOG_LEN    60

struct PulseLog {
  uint32_t period;
  uint32_t pulseWidth;
};

volatile PulseLog logBuffer[LOG_LEN];
volatile uint8_t logIndex = 0;
volatile bool logFull = false;

// Timer extension
volatile uint32_t timerHigh = 0;  // high word of 32-bit counter

// State tracking
volatile uint32_t lastRise = 0;
volatile uint32_t riseTime = 0;
volatile bool lastStateHigh = false;

ISR(TCB0_INT_vect) {
  // Overflow every 65536 ticks of TCB0 (with clk/2 this is ~6.5 ms at 20 MHz)
  timerHigh += 0x10000UL;
  TCB0.INTFLAGS = TCB_CAPT_bm; // clear flag
}

uint32_t nowTicks() {
    static uint16_t lastTCB = 0;
    static uint32_t extended = 0;

    uint16_t tcb = TCB0.CNT;
    if (tcb < lastTCB) {
        // overflow happened
        extended += 0x10000UL;
    }
    lastTCB = tcb;
    return extended | tcb;
}

ISR(PORTA_PORT_vect) {
  uint32_t now = nowTicks();
  bool pinHigh = VPORTA.IN & (1 << 2);

  if (pinHigh && !lastStateHigh) {
    // Rising edge
    uint32_t period = now - lastRise;
    lastRise = now;
    riseTime = now;
    logBuffer[logIndex].period = period; // width filled on fall

    display_measurement("p",period);

  } else if (!pinHigh && lastStateHigh) {
    // Falling edge
    uint32_t width = now - riseTime;
    logBuffer[logIndex].pulseWidth = width;
    display_measurement("w",width);

    logIndex++;
    if (logIndex >= LOG_LEN) {
      logIndex = 0;
      logFull = true;
    }
  }
  lastStateHigh = pinHigh;
  VPORTA.INTFLAGS = (1 << 2); // clear interrupt
}

void setupTimer() {
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc;
  TCB0.INTCTRL = TCB_CAPT_bm; // overflow interrupt
}

void setupPinInterrupt() {
  PORTA.DIRCLR = (1 << 2); // input
  PINCTRL = PORT_ISC_BOTHEDGES_gc; // trigger on both edges
}

void setup() {

  pinMode(0, INPUT);
  pinMode(10, OUTPUT);
  Serial.begin(9600);
  pinMode(0, INPUT);

  oSerial.begin(115200);
  oSerial.println("DCF calibr");

  cli();
  setupTimer();
  setupPinInterrupt();
  sei();
  logFull = false;
}

struct Cluster {
  uint32_t avg;
  uint8_t count;
};

bool within10(uint32_t a, uint32_t b) {
  uint32_t minv = (a < b) ? a : b;
  uint32_t maxv = (a > b) ? a : b;
  return maxv <= (minv * 11) / 10; // within +10%
}

  Cluster p1 = {0,0}, p2 = {0,0};
  Cluster w1 = {0,0}, w2 = {0,0};

void categorizeLogs() {
  if (!logFull) return;



  // Seed clusters with first two distinct values
  p1.avg = logBuffer[0].period; p1.count = 1;
  for (uint8_t i=1;i<LOG_LEN;i++) {
    if (!within10(logBuffer[i].period, p1.avg)) {
      p2.avg = logBuffer[i].period; p2.count = 1;
      break;
    }
  }
  w1.avg = logBuffer[0].pulseWidth; w1.count = 1;
  for (uint8_t i=1;i<LOG_LEN;i++) {
    if (!within10(logBuffer[i].pulseWidth, w1.avg)) {
      w2.avg = logBuffer[i].pulseWidth; w2.count = 1;
      break;
    }
  }

  // Assign values to clusters
  for (uint8_t i=1;i<LOG_LEN;i++) {
    uint32_t p = logBuffer[i].period;
    if (within10(p, p1.avg)) { p1.avg = (p1.avg*p1.count + p)/(p1.count+1); p1.count++; }
    else if (within10(p, p2.avg)) { p2.avg = (p2.avg*p2.count + p)/(p2.count+1); p2.count++; }
    else {
      // Outlier period
      // Report or handle
    }

    uint32_t w = logBuffer[i].pulseWidth;
    if (within10(w, w1.avg)) { w1.avg = (w1.avg*w1.count + w)/(w1.count+1); w1.count++; }
    else if (within10(w, w2.avg)) { w2.avg = (w2.avg*w2.count + w)/(w2.count+1); w2.count++; }
    else {
      // Outlier width
      // Report or handle
    }
  }

  // Results in p1, p2, w1, w2
  // (you can now print them or use them in your application)
}

void loop() {

  bool b = digitalRead(0);
  digitalWrite(10,b?HIGH:LOW);

  if (logFull) {
    cli();
    categorizeLogs();
    display_updates();
    logFull = false;
    sei();
  }
}

void display_updates()
{
    oled.clear();
    oled.setCursor(0,0);
    oSerial.print("p1");
    oSerial.print(HEXstr(p1.avg));
    oSerial.print("p2");
    oSerial.print(HEXstr(p2.avg));
    oSerial.print("w1");
    oSerial.print(HEXstr(w1.avg));
    oSerial.print("w2");
    oSerial.print(HEXstr(w2.avg));  
}

const char* HEXstr(uint32_t value)
{
    static char buffer[9]; // 8 hex digits + null terminator
    const char* hexChars = "0123456789ABCDEF";
    uint8_t DIGITS = 8;

    // --- Configuration ---
    // true  -> pad with spaces
    // false -> pad with zeroes
    const int useSpaces = 1;
    // ---------------------

    int started = 0; // Flag to skip leading zeros
    for (int i = 0; i < DIGITS; ++i) {
        int shift = ((DIGITS-1) - i) * 4;
        uint8_t nibble = (value >> shift) & 0xF;

        if (nibble != 0 || started || i == 7) {
            buffer[i] = hexChars[nibble];
            started = 1;
        } else {
            buffer[i] = useSpaces ? ' ' : '0';
        }
    }

    buffer[DIGITS] = '\0'; // Null terminator
    return buffer;
}

void display_measurement(char* c, uint32_t d)
{
  /*
    oled.clear();
    oled.setCursor(0,0);
    oSerial.print(c);
    oSerial.print("=");
    oSerial.println(HEXstr(d)); 
  */
}

