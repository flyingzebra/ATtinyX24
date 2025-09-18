class DCF77Decoder
{
public:
  struct Settings {
    const uint8_t pulseTolerancePercent = 20;
    const uint8_t calibrationPulses     = 10;
  };

  struct Pulse {
    uint8_t bitIndex;                     // index in the current frame
    int8_t bitValue;                      // -1 if ambiguous, 0 or 1 otherwise
    uint16_t shortPulse           = 80;   // initial guess in ms
    uint16_t longPulse            = 100;  // initial guess in ms  
    const uint16_t thresh         = 150;  // initial split
    bool calibrated;                       // indicate when pulse preamble is calibrated
    bool lastLevel;
    bool pulseReported;
    bool pulsePolarity;                   // true=falling edge false=rising edge
    unsigned long lastEdgeTime;
    bool level;
    int level_fil;
  };

  struct Frame {
    uint8_t bits[60];  // store full 60-bit frame
    bool valid;
    bool frameReady;
    bool frameDetected;
  };

  struct MonitorInfo {
    uint16_t avg0;        // EMA short pulse width
    uint16_t avg1;        // EMA long pulse width
    bool     frameStart;  // minute marker detected
  };

  // Define a structure for EMA filter state
  struct EMAFilter
  {
    uint8_t shift = 3;      // Smoothing factor (N). Weight = 1 / (2^N)
    long filteredValue = 0; // Stores the last filtered value (scaled)
    bool initialised = false;   // Flag to initialize with first sample
  };

  void begin()
  {
    pinMode(_pin, INPUT);
    p.lastLevel     = digitalRead(_pin);
    p.lastEdgeTime  = micros();
    p.bitIndex      = 0;
    f.frameReady    = false;
    f.frameDetected = false;
    p.calibrated    = false;
    p.pulseReported = false;
    m.avg0 = m.avg1 = 0;
  }

  int x = 0;

  void update() 
  {
    p.level = digitalRead(_pin);
    p.level_fil = updateEMAFilter(myFilter, p.level?32:0);

    p.pulseReported = false;
    if (p.level != p.lastLevel)       // detect edge
    {
      p.pulseReported = true;
      unsigned long now = micros();
      uint16_t widthMs  = (now - p.lastEdgeTime) / 1000;
      p.lastEdgeTime    = now;

      // process *both* low and high durations
      if (widthMs > 1500)          // Minute marker detected
      {
        m.frameStart = true;
        p.bitIndex   = 0;
      }
      else
      {
        m.frameStart = false;
        if (p.calibrated) 
            processPulse(widthMs);
        else
            calibrate(widthMs);
      }
    }
    p.lastLevel = p.level;
  }

  int updateEMAFilter(EMAFilter &filter, int rawValue)
  {
    if (!filter.initialised) {
      // Initialize with the first value
      filter.filteredValue = (long)rawValue << filter.shift; // scale up
      filter.initialised = true;
    } else {
      // EMA update: y[n] = y[n-1] + (x[n] - y[n-1]) / 2^N
      filter.filteredValue += (((long)rawValue << filter.shift) - filter.filteredValue) >> filter.shift;
    }

    // Return filtered value in the same scale as input
    return filter.filteredValue >> filter.shift;
  }


  MonitorInfo monitor()
  {
    m.avg0        = m.avg0;
    m.avg1        = m.avg1;
    m.frameStart  = f.frameDetected;
    return m;
  }

  bool frameReady() const {
    return f.frameReady;
  }

  Frame getFrame() {
    f.frameReady = false;
    return f;
  }

  bool isCalibrated()
  {
    return p.calibrated;
  }

  void setpin(int pin)
  {
    _pin = pin;
  }

  Pulse p;

private:
  Settings s;
  MonitorInfo m;
  Frame f;
  EMAFilter myFilter;

  uint8_t _pin;
  uint8_t _emaShift;

  void calibrate(uint16_t widthMs)
  {
    // minute marker detection (>1500 ms)
    if (widthMs > 1500) { f.frameDetected = true; return; }
    f.frameDetected = false;

    // EMA for short/long pulses
    if (widthMs < p.thresh) {
      if (m.avg0 == 0) m.avg0 = widthMs;
      else m.avg0 = (m.avg0 * 7 + widthMs) >> 3;
    } else {
      if (m.avg1 == 0) m.avg1 = widthMs;
      else m.avg1 = (m.avg1 * 7 + widthMs) >> 3;
    }

    // finalize thresholds after 60 pulses
    static uint16_t pulseCount = 0;
    pulseCount++;
    if (pulseCount >= s.calibrationPulses) {
      p.shortPulse = (m.avg0 + (m.avg1 - m.avg0)) >> 2;
      p.longPulse  =  (m.avg1 - (m.avg1 - m.avg0)) >> 2;
      p.calibrated = true;
      pulseCount   = 0;
    }
  }

  void processPulse(uint16_t widthMs)
  {
    p.bitValue   = -1;
    m.frameStart = false;

    // Minute marker detection (~2 s)
    if (widthMs > 1500) {
      if (p.bitIndex > 0) {           // any bits collected
        f.valid = true;  // frame is complete
        f.frameReady = true;
      }
      p.bitIndex      = 0;
      f.frameDetected = true;
      m.frameStart    = true;
    }
    f.frameDetected = false;

    // classify bit using calibrated thresholds with tolerance
    uint16_t shortLower = p.shortPulse * (100 - s.pulseTolerancePercent) / 100;
    uint16_t longLower  = p.longPulse * (100 - s.pulseTolerancePercent) / 100;

    if (widthMs >= longLower) p.bitValue = 1;
    else if (widthMs >= shortLower) p.bitValue = 0;

    if (p.bitIndex < 60 && p.bitValue != -1) {
      f.bits[p.bitIndex++] = p.bitValue;
    }

/*
    if (_debugPort) {
      oled.setCursor(0, 0);
      //_debugPort->print("W");
      //_debugPort->print(widthMs);
      _debugPort->print(" I");
      _debugPort->print(p.bitIndex);
      _debugPort->print(" B");
      _debugPort->print(p.bitValue);
      if (m.frameStart) _debugPort->print(" M");
      _debugPort->println("       ");
    }
*/

  }
};
