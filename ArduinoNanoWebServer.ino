#include <SoftwareSerial.h>
SoftwareSerial esp8266(2, 3);
#define serialCommunicationSpeed 9600
#define DEBUG true


const int lightPin = 5;
//---------- Vars for commands ----------
int lightPower = 0;

bool lightTimer = false;
unsigned long turnOffMillis = 0;

//---------- ---------- ---------- ----------

const PROGMEM char headerResponseTemplate[] = {"HTTP/1.1 200\r\nContent-Length: REPLACE_BODY_LEN\r\nConnection: Closed\r\nContent-Type: REPLACE_CONTENT_TYPE\r\nAccess-Control-Allow-Origin: *\r\n\r\n"};
const PROGMEM char controlPage[] = "HTTP/1.1 200\r\nConnection: Closed\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n<meta name=\"viewport\" content=\"width=device-width, maximum-scale=1, user-scalable=no\"><script src=\"https://code.jquery.com/jquery-3.5.1.min.js\" integrity=\"sha256-9/aliU8dGd2tb6OSsuzixeV4y/faTqgFtohetphbbj0=\" crossorigin=\"anonymous\"></script><script>$(document).ready(function (){console.log(\"ready!\"); $(\"#sendButton\").click(function (){power=$(\"#lightPower\").val();timer=$(\"#timer\").val(); console.log(\"Power: \" + power + \" Timer: \" + timer);$(\"#debug\").text(\"Loading...\"); $.get(\"http://192.168.1.167/light\",{power: power, timeout: timer}) .done(function (data){$(\"#debug\").html(\"<pre>\" + JSON.stringify(data, null, 4) + \"</pre>\");});});});</script>Power<input id=\"lightPower\" type=\"text\"><br>Timer (s)<input id=\"timer\" type=\"number\"><br><button id=\"sendButton\"> OK</button><br><br>Debug<div id=\"debug\" style=\"border: 1px solid black;\">...</div>";

const PROGMEM char lightPageJsonStructure[] = "{\"upTime\": REPLACE_UPTIME, \"power\": REPLACE_POWER, \"timeout\": REPLACE_TIMEOUT}";

const int BUF_SIZE = 500;
char globalBuffer[BUF_SIZE];

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(lightPin, OUTPUT);
  analogWrite(lightPin, lightPower);


  Serial.begin(serialCommunicationSpeed);
  Serial.println(F("Init..."));

  esp8266.begin(serialCommunicationSpeed);

  delay(25);

  InitWifiModule();

  Serial.print(F("Memory available: "));
  Serial.println(availableMemory());
  Serial.println(F("Starting..."));
  digitalWrite(LED_BUILTIN, LOW);
}
void loop() {

  // ---------- MANAGE DATA INCOMING FROM ESP8266 ----------
  if (esp8266.available()) {
    char *staticContent = NULL;
    String esp8266Data = "";

    delay(250); //Give some time to fill the buffer
    while (esp8266.available()) {
      char c = esp8266.read();
      esp8266Data += c;
    }

    Serial.print(F("Memory abailable: "));
    Serial.println(availableMemory());

    Serial.print(F("Data from ESP8266: "));
    Serial.println(esp8266Data);
    if (esp8266Data.indexOf("FAIL") > 0) {
      error(F("\n\n---------- ERROR: SOMETHING'S WRONG WITH THE CHIP ----------\n\n"));
    } else if (esp8266Data.indexOf(F("DISCONNECT")) > 0) {
      error(F("\n\n---------- ERROR: WIFI DISCONNECTED ----------\n\n"));
    } else if (esp8266Data.indexOf(F("GET")) > 0) { //If it is an HTTP Request

      String connectionId = getConnectionId(esp8266Data);
      Serial.print(F("Connection Id: "));
      Serial.println(connectionId);

      stringFromMem(globalBuffer, headerResponseTemplate); //Copy from MEM
      String httpResponse = String(globalBuffer); //Starting with header
      int headerLength;

      Serial.print(F("Memory abailable after String headerResponse: "));
      Serial.println(availableMemory());


      if (esp8266Data.indexOf("/light") > 0) { //---------- /light
        httpResponse.replace("REPLACE_CONTENT_TYPE", "application/json");
        headerLength = httpResponse.length();

        int lightPowerParam = getQueryParam(esp8266Data, "power").toInt();
        int timeoutParam = getQueryParam(esp8266Data, "timeout").toInt();

        if (timeoutParam > 0) {
          lightTimer = true;
          long turnOffDelay = (long)timeoutParam * 1000;
          turnOffMillis = millis() + turnOffDelay;
        } else {
          lightTimer = false;
        }

        stringFromMem(globalBuffer, lightPageJsonStructure); //Copy response template from MEM
        httpResponse += String(globalBuffer);
        httpResponse.replace("REPLACE_POWER", "\"" + String(lightPowerParam) + "\"");
        httpResponse.replace("REPLACE_TIMEOUT", "\"" + String(turnOffMillis) + "\"");
        httpResponse.replace("REPLACE_UPTIME", "\"" + String(millis()) + "\"");

        Serial.print("\nPower: " + String(lightPowerParam) + " Timeout: " + String(timeoutParam) + "\n");

        analogWrite(lightPin, lightPowerParam);

      } else if (esp8266Data.indexOf("/control") > 0) { //---------- /control
        Serial.println(F("/control accessed"));
        httpResponse.replace("REPLACE_CONTENT_TYPE", "text/html");
        headerLength = httpResponse.length();

        staticContent = controlPage;

        Serial.print(F("Memory abailable copying body: "));
        Serial.println(availableMemory());
      } else { // ---------- All other pages
        httpResponse.replace("REPLACE_CONTENT_TYPE", "text/html");
        headerLength = httpResponse.length();

        httpResponse += "Try visiting page /light";
        httpResponse += "\n\nUpTime: " + String(millis() / 1000) + " s";
      }

      Serial.print(F("Memory abailable after String bodyResponse: "));
      Serial.println(availableMemory());

      httpResponse.replace("REPLACE_BODY_LEN", String(httpResponse.length() - headerLength)); //Setting body length

      //Serial.print("httpResponse: ");
      //Serial.println(httpResponse);

      String espCommand = "";
      int httpResponseLen;

      if (staticContent == NULL) {
        httpResponseLen = httpResponse.length();
      } else {
        httpResponseLen = strlen_P(staticContent); //Header and body are bot included in static content
      }

      espCommand = "AT+CIPSEND=" + connectionId + "," + httpResponseLen + "\r\n"; //Composing espCommand => AT+CIPSEND=<connectionId>,<responseLength>

      Serial.println("Sending command: " + espCommand);
      sendData(espCommand, 1000, DEBUG);

      Serial.println("Sending HTTPResponse: " + httpResponse);
      if (staticContent == NULL) {
        esp8266.print(httpResponse); //Sending HTTP Response
      } else {
        //Sending static content byte by byte
        for (int k = 0; k < strlen_P(staticContent); k++) {
          char c = pgm_read_byte_near(staticContent + k);
          esp8266.print(c);
        }

        delay(100); //Wait for ESP8266 to process incoming data
        espCommand = "AT+CIPCLOSE=" + connectionId + "\r\n";
        sendData(espCommand, 1000, DEBUG); //Closing connection since we didn't send header Content-length
      }
    }
  }


  // ---------- MANAGE DATA INCOMING FROM SERIAL ----------
  if (Serial.available()) { //This part allows to send command to the ESP8266 through the serial monitor
    delay(250); //Give some time to fill the buffer
    while (Serial.available()) {
      char c = Serial.read();
      esp8266.print(c);
    }
  }


  // ---------- MANAGE SYSTEM ----------
  handleTimer();
}

String sendData(String command, const int timeout, boolean debug)
{
  String incomingData = "";

  esp8266.print(command);
  long int time = millis();
  while ( (time + timeout) > millis())
  {
    while (esp8266.available())
    {
      char c = esp8266.read();
      incomingData += c;
    }
  }
  if (debug)
  {
    Serial.print(F("[DEBUG]: "));
    Serial.print(incomingData);
  }
  return incomingData;
}

void InitWifiModule()
{
  sendData(F("AT+RST\r\n"), 2000, DEBUG); //Reset chip
  delay(1000);
  sendData(F("AT+CWJAP=\"YOUR_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n"), 2000, DEBUG); //Connect to network
  delay (3000);
  sendData(F("AT+CWMODE=1\r\n"), 1500, DEBUG);//SET Station  Mode
  delay (1000);
  sendData(F("AT+CIFSR\r\n"), 1500, DEBUG);//Show IP
  delay (1000);
  sendData(F("AT+CIPMUX=1\r\n"), 1500, DEBUG);//Accept multiple connections
  delay (1000);
  sendData(F("AT+CIPSERVER=1,80\r\n"), 1500, DEBUG);//Start server on port 80
}

String getConnectionId(String esp8266Data) {
  int start = esp8266Data.indexOf(F("+IPD,")) + 5; //Search of +IPD,
  int end = esp8266Data.indexOf(F(","), start); //Search second comma after the first one

  if ((start < 0) || (end < 0)) { //something's wrong
    return "X"; //Error
  }

  return esp8266Data.substring(start, end);
}

String getQueryParam(String esp8266Data, String searchParam) {
  int queryStringStart = esp8266Data.indexOf(F("GET ")) + 4;
  int queryStringEnd = esp8266Data.indexOf(F(" HTTP/1.1"));

  if ((queryStringStart < 0) || (queryStringEnd < 0)) { //something's wrong
    return "X"; //Error
  }

  String queryString = esp8266Data.substring(queryStringStart, queryStringEnd);

  int paramStart = queryString.indexOf(searchParam) + searchParam.length() + 1; //Last +1 is to skip the "="
  if (paramStart < 0) {
    return "X"; //Not found
  }
  int paramEnd = queryString.indexOf("&", paramStart); //In case this is not the last param search &
  if (paramEnd < 0) {
    paramEnd = queryString.length(); //If there is no & it means this is the last param then must end at the end of the string
  }

  String param = queryString.substring(paramStart, paramEnd);

  return param;
}

int availableMemory() {
  int size = 2048;
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL);
  free(buf);
  return size;
}

void stringFromMem(char *buf, char *mem) { //Should pass bufSize or remove from params since it's a global var
  for (int i = 0; i < BUF_SIZE; i++) { //Clear buf
    buf[i] = '\0';
  }
  for (int k = 0; k < strlen_P(mem); k++) {
    buf[k] = pgm_read_byte_near(mem + k);
  }
}


void handleTimer() {
  if (lightTimer) {
    if (millis() > turnOffMillis) {
      analogWrite(lightPin, 0);
      lightTimer = false;

      Serial.println("Turning off lights");
    }
  }
}

void error(String e) {
  Serial.println(e);
  while (1) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
}
