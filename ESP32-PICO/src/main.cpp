#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

const char* ssid     = "ESP32-LED-1";
const char* password = "1145141919810";
unsigned int LEDPin = 5,SwitchPin = 4,USBPowerPin = 2;
bool USBPowerState = false; //用于在有USB供电时关断输出，记得先检查这个
unsigned int Mode = 0; //0关闭 1开启 2呼吸 3预留
WiFiServer server(80);

void setup()
{
    analogReadResolution(12);
    int VCC = analogReadMilliVolts(USBPowerPin);//板子上有分压，读取到的是VCC的1/2
    if (VCC > 1250)
    {
        USBPowerState = true;
    }
    
    Serial.begin(115200);
    pinMode(LEDPin, OUTPUT);      // set the LED pin mode
    pinMode(SwitchPin, INPUT);    // set the Switch pin mode
    pinMode(USBPowerPin, INPUT);  // set the USB Power pin mode

    delay(10);

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    server.begin();

}

void loop(){
    WiFiClient client = server.available();   // listen for incoming clients
    int VCC = analogReadMilliVolts(USBPowerPin);//板子上有分压，读取到的是VCC的1/2
    if (VCC > 1250)
        {
            USBPowerState = true;
        }
 
    if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("Click <a href=\"/H\">here</a> to use next mode.<br>");
            client.print("Click <a href=\"/L\">here</a> to use previous mode.<br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("GET /H")) {
          if (Mode <=2)
          {
            Mode++;
          }
          else
          {
            Mode = 0;
          }
                  
        }
        if (currentLine.endsWith("GET /L")) {
            if (Mode >=1)
            {
                Mode--;
            }
            else
            {
                Mode = 3;
            }
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
    if (!USBPowerState)
    {
       switch (Mode)
    {
    case 0:
        digitalWrite(LEDPin, LOW);
        break;
    case 1:
        digitalWrite(LEDPin, HIGH);
        break;
    case 2: //呼吸灯
        for (int i = 0; i < 1024; i++)
        {
            analogWrite(LEDPin, i);
            delay(2);
        }
        for (int i = 1023; i >= 0; i--)
        {
            analogWrite(LEDPin, i);
            delay(2);
        }
        break;
    case 3:
        break;
    default:    
        break;
    }
    }
    else
    {
        digitalWrite(LEDPin, LOW);
    }
}

