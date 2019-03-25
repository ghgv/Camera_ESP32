// ArduCAM Mini demo (C)2015 Lee
// web: http://www.ArduCAM.com
// This program is a demo for ESP8266 arduino board.
// Publish captured image to a webpage.
//
// This demo was made for ArduCAM Mini OV2640 2MP Camera.
// It needs to be used in combination with html webpage.
//

// This program requires the ArduCAM V3.4.3 (or later) and ESP8266-Websocket libraries
// and use Arduino IDE 1.6.5 compiler or above
//#define ESP82
//#ifdef ESP82
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
//#endif

/*#ifdef ESP32
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#endif*/
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#include <WebSocketServer.h>

// Enabe debug tracing to Serial port.
#define DEBUGGING

// Here we define a maximum framelength to 64 bytes. Default is 256.
#define MAX_FRAME_LENGTH 64

// Define how many callback functions you have. Default is 1.
#define CALLBACK_FUNCTIONS 1

#if defined(ESP8266)
// set GPIO15 as the slave select :
const int CS = 16;
#else
// set pin 10 as the slave select :
const int CS = 2;
#endif


const char* ssid = "Dali";
const char* password = "Dali12345";


// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
WebSocketServer webSocketServer;

void read_fifo_burst(ArduCAM myCAM);

ArduCAM myCAM(OV5642, CS);

void start_capture()
{
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}

void handleClientData(String &dataString) 
{
  String h="Hola";
  if (dataString.startsWith("capture"))
  {
    Serial.println(dataString);
    //webSocketServer.sendData(h);
    start_capture();
    //webSocketServer.disconnectStream();
  }
}

// send the client the analog value of a pin
void sendClientData(char *data, long lenght, unsigned char header, int size) {
  webSocketServer.sendDataFormated(data,size , header,size);
}

void setup() {
  // put your setup code here, to run once:
  uint8_t vid, pid;
  uint8_t temp;
#if defined(__AVR__) || defined(ESP8266)
  Wire.begin();
#endif
#if defined(__arm__)
  Wire1.begin();
#endif
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the CS as an output:
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();

  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  Serial.println(temp);
  if (temp != 0x55)
  {
    Serial.println("SPI1 interface Error!");
    //while(1);
  }

  //Check if the camera module type is OV5642
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  if ((vid != 0x56) || (pid != 0x42))
    Serial.println("Can't find OV5642 module!");
  else
    Serial.println("OV5642 detected.");

  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  //myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
  myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.clear_fifo_flag();
  myCAM.write_reg(ARDUCHIP_FRAMES, 0x00);

  // Connect to WiFi network
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
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());
  // This delay is needed to let the WiFi respond properly(very important)
  //delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  String data;
  int total_time = 0;
  //Serial.println("+");

  WiFiClient client = server.available();
  
  if (webSocketServer.handshake(client)) {
    Serial.println("Connected to client");
    while (client.connected() )
    {
      data = webSocketServer.getData();
      if (data.length() > 0) 
      {
        handleClientData(data);
      }

      if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
      {
        Serial.println("CAM Capture Done!");
        total_time = millis();
        read_fifo_burst(myCAM);
        total_time = millis() - total_time;
        Serial.print("total_time used (in miliseconds):");
        Serial.println(total_time, DEC);
        Serial.println("CAM send Done!");
        //Clear the capture done flag
        myCAM.clear_fifo_flag();
        
 
      }
    }

  }
  // This delay is needed to let the WiFi respond properly(very important)
  delay(100);
}



void read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp, temp_last;
  static int i = 0;
  static uint8_t first_packet = 1;
  byte buf[2048];
  uint32_t length = 0;

  length = myCAM.read_fifo_length();
  Serial.println("Length:");
  Serial.println(length);
  if (length >= 8000000 ) // 384kb
  {
    Serial.println("Over size.");
    return;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println("Size is 0.");
    return;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  SPI.transfer(0x00);
  //Read JPEG data from FIFO
  while ( (temp != 0xD9) | (temp_last != 0xFF))
  {
    temp_last = temp;
    temp = SPI.transfer(0x00);

    //Write image data to buffer if not full
    if (i < 2048)
    {
      buf[i++] = temp;
      yield();
    }
    else
    {
      if (first_packet == 1)
      {
        sendClientData((char*)buf,length,0x02,2048);//First frame
        first_packet = 0;
      }
      else
      {
        sendClientData((char*)buf,length,0x00, 2048);//Continuation
      }
      i = 0;
      buf[i++] = temp;

    }
    yield();
  }
  //Write the remain bytes in the buffer
  if (i > 0)
  {
    Serial.println("write last 256 bytes");
    sendClientData((char*)buf, length, 0x80,i);//FIN Frame
    //delay(10);
    //Serial.println("finish write last 256 bytes");
    i = 0;
    first_packet = 1;
  }
  yield();
  myCAM.CS_HIGH();
}
