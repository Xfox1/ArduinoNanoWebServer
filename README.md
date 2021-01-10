# ArduinoNanoWebServer
Simple light control through Arduino Nano - ESP8266 WebServer


# Most used AT Commands

| Command | Params | Description |
|-|-|-|
| AT |-|Verify if chip is connected and responding|
| AT+RST |-|Restart the module|
| AT+CWMODE=\<mode> | 1 = Sta, 2 = AP, 3 = both | Change WiFi Mode |
| AT+CWJAP=\<ssid>,\<pwd> | ssd = ssd, pwd = wifi password | Connect to AP |
| AT+CWLAP | - | List the APs |
| AT+CIFSR | - | Get current IP address |
| AT+ CIPMUX=\<mode> | mode = 0 => single connection, mode = 1 => multiple connections | Set connection parallelism |
| AT+ CIPSERVER=\<mode>,\<port> | mode = 0 => close server mode, mode = 1 => start server mode, port = server port | Activate/deactivate server mode |
| AT+UART_DEF=\<baudrate>,\<databits>,\<stopbits>\<parity>,\<flow control>| ... | See [ESP8266 AT Istruction Set](https://www.espressif.com/sites/default/files/documentation/4a-esp8266_at_instruction_set_en.pdf) for more details |