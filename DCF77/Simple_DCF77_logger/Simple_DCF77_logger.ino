#define NUM_SAMPLES 600       // Campaign length
#define SKIP_OUTLIERS

volatile uint32_t last_rising = 0;
volatile uint32_t last_falling = 0;
volatile uint32_t pulse_width_ms = 0;
volatile uint32_t pulse_period_ms = 0;
volatile bool new_measurement = false;
volatile bool bInit = true;
volatile bool b = true;
volatile bool bSkipEdge = false;
static uint16_t sentCount = 0;


ISR(PORTA_PORT_vect) {
  // Read which pin triggered
  uint8_t flags = VPORTA.INTFLAGS;

  if (flags & PIN4_bm) {   // PA4 caused the interrupt
    uint32_t now = millis();

    if (sentCount < NUM_SAMPLES) 
    {
      b = digitalRead(PIN_PA4);
      digitalWrite(10, b ? HIGH : LOW);
    }

    if (VPORTA.IN & PIN4_bm)
    {
      // Rising edge
      if(bSkipEdge)
      {
        pulse_period_ms = now - last_rising;
        last_rising = now;
      }
      else bSkipEdge = false;
    }
    else 
    {
      // Falling edge
      pulse_width_ms = now - last_rising;

      #ifdef SKIP_OUTLIERS
        if(pulse_width_ms>75)
        {
          last_falling = now;
          new_measurement = true;
        }
        else bSkipEdge = true;
      #else
          last_falling = now;
          new_measurement = true;
      #endif
    }

    // Clear interrupt flag for PA4
    VPORTA.INTFLAGS = PIN4_bm;
  }
}

void setup() {
  delay(12000);
  Serial.begin(115200);

  // Configure PA4 as input with interrupt on both edges
  pinMode(PIN_PA4, INPUT);
  pinMode(10, OUTPUT);

  PORTA.PIN4CTRL = PORT_PULLUPEN_bm         // optional pull-up
                 | PORT_ISC_BOTHEDGES_gc;   // trigger on both edges

  sei(); // enable global interrupts

  // Start JSON array
}

void loop() {
  if (new_measurement) {
    new_measurement = false;

   //Serial.print("{\"P\":0x");
    if(bInit) { bInit = false; Serial.print("["); sentCount = 0; return;}

    if(sentCount>0) Serial.print(",");
    Serial.print("0x");
    Serial.print(HEXstr(pulse_period_ms));
    //Serial.print(",\"W\":0x");
    Serial.print(",");
    Serial.print("0x");
    Serial.print(HEXstr(pulse_width_ms));
    //Serial.print("}");

    sentCount++;
    if (sentCount < NUM_SAMPLES) 
    {
      if((sentCount-1)%60==59)
        Serial.println("");

    } else {
      Serial.println("]");
      while (1); // stop after campaign
    }

  }
}

const char* HEXstr(uint32_t value)
{
    static char buffer[9]; // 8 hex digits + null terminator
    const char* hexChars = "0123456789ABCDEF";
    uint8_t DIGITS = 3;

    // --- Configuration ---
    // true  -> pad with spaces
    // false -> pad with zeroes
    const int useSpaces = 0;
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
