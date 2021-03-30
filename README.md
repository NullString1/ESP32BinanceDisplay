# ESP32BinanceDisplay
Cycles between spot balance and futures balance in USDT on segment display. 
## Installation
1. Copy into blank sketch
2. Fill in requires values
     | Macro / Variable | Default Value | Hint
     | --- | --- | --- |
     | DIGIT_ON | HIGH | HIGH for common anode displays (invert for common cathode displays)|
     | DIGIT_OFF | LOW | LOW for common anode displays |
     | SEGMENT_ON | LOW | LOW for common anode displays |
     | SEGMENT_OFF | HIGH | HIGH for common anode displays |
     | digit1 (1-4) | 32, 33, 25, 26 | PIN numbers to set which digit to light |
     | APIKEY | (None) | Insert your API key from Binance |
     | APISECRET | (None) | Insert your API secret from Binance |
     | ntpServer | pool.ntp.org | Change NTP server if you wish |
     | gmtOffset_sec | 0 | Seconds to offset from GMT |
     | daylightOffset_sec | 3600 | Daylight offset in seconds |
     | ssid | (None) | Insert your network SSID |
     | password | (None) | Insert your network PSK |
     | segA (A-G) | 19, 18, 5, 17, 16, 4, 0 | PIN numbers to set which segment to light |
3. Wire display to set pins and flash to ESP32
## Limitations
- Only reads USDT in futures wallet
- Only reads BTC in spot wallet
