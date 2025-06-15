#include <HardwareSerial.h>

// Change these to the UART pins wired to SIM7670G TX/RX on your board
static const int SIM_RX_PIN = 17;  
static const int SIM_TX_PIN = 18;  

HardwareSerial simSerial(2);  // UART2 on ESP32-S3

// Send an AT command and accumulate the response for up to 'timeout' ms
String sendAT(const String &cmd, unsigned long timeout = 2000) {
  while (simSerial.available()) simSerial.read();  // flush
  simSerial.println(cmd);

  
  String resp;
  unsigned long t0 = millis();
  while (millis() - t0 < timeout) {
    while (simSerial.available()) {
      char c = simSerial.read();
      resp += c;
    }
  }
  Serial.print(resp);  // echo to USB-Serial
  return resp;
}

// Parse "+HTTPACTION: 0,200,NNN" to extract the payload length NNN
int parseHttpLength(const String &resp) {
  int p = resp.indexOf("+HTTPACTION:");
  if (p < 0) return 0;
  int firstComma  = resp.indexOf(',', p);
  int secondComma = resp.indexOf(',', firstComma + 1);
  if (secondComma < 0) return 0;
  String len = resp.substring(secondComma + 1, resp.indexOf('\r', secondComma));
  return len.toInt();
}

void setup() {
  Serial.begin(115200);
  simSerial.begin(115200, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(1000);

  // 1. Basic sanity
  sendAT("AT");
  sendAT("AT+CPIN?");
  sendAT("AT+CSQ");
  sendAT("AT+CREG?");
  
  // 2. GPRS Attach & PDP context
  sendAT("AT+CGATT?");
  sendAT("AT+CGDCONT=1,\"IP\",\"everywhere\"");
  sendAT("AT+CGAUTH=1,1,\"eesecure\",\"secure\"");
  sendAT("AT+CGACT=1,1");
  sendAT("AT+CGPADDR=1");

  // // 3. HTTP GET flow
  // sendAT("AT+HTTPTERM");  // in case it was still running
  // sendAT("AT+HTTPINIT");
  // sendAT("AT+HTTPPARA=\"URL\",\"http://httpbin.org/get?from=sim7670\"");
  // sendAT("AT+HTTPPARA=\"READMODE\",1");


  // 3. HTTP GET over HTTPS
  sendAT("AT+HTTPTERM");                        // teardown any old session
  sendAT("AT+HTTPINIT");                        // init HTTP engine
  sendAT("AT+HTTPPARA=\"URL\",\"https://alusys.io/test/sample.bin\"");  // new URL
  sendAT("AT+HTTPPARA=\"READMODE\",1");         // direct read mode

  
  // fire the GET (give it up to 10s)
  String actionResp = sendAT("AT+HTTPACTION=0", 10000);
  int len = parseHttpLength(actionResp);
  if (len > 0) {
    // read exactly 'len' bytes
    sendAT("AT+HTTPREAD=0," + String(len), 10000);
  }
  sendAT("AT+HTTPTERM");
}

void loop() {
  // nothing here
}
