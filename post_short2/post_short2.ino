#include <ArduinoJson.h>
#include <ESP32Servo.h>
/*
    This sketch sends a message to a TCP server

*/
#define OV5642_CHIPID_HIGH 0x300a
#define OV5642_CHIPID_LOW 0x300b
#define VSYNC_LEVEL_MASK     0x02  // 0 = High active ,    1 = Low active
#define ARDUCHIP_FRAMES     0x01  // FRAME control register, Bit[2:0] = Number of frames to be captured

#include <WiFi.h>
#include <WiFiMulti.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <HTTPClient.h>

#include "memorysaver.h"

// Enabe debug tracing to Serial port.
#define DEBUGGING
// Here we define a maximum framelength to 64 bytes. Default is 256.
#define MAX_FRAME_LENGTH 64
#define CALLBACK_FUNCTIONS 1
#ifdef ESP82
const int CS = 16;
#endif
#ifdef ESP32
const int CS = 5;
#endif

HTTPClient http;
Servo myservo;
int pos = 0;  

const uint16_t port = 80;//8085;
const char * host = "www.dali.com.co"; // ip or dns

void read_fifo_burst(ArduCAM myCAM);
ArduCAM myCAM(OV5642, CS);

uint32_t length = 0;

WiFiMulti WiFiMulti;
WiFiClient client;
int Frame = 0;

int sendClientData(char* buf, int length1, int code, int l) {
  int n = 0;
  if (code == 0x02) { //Initial frame
    client.write(0xFF);
    //Serial.print("Frame:");
    //Serial.println(Frame++);
    n = client.write(buf, l);
    return n;
  }
  if (code == 0x00) { // continuation
    n = client.write(buf, l);
    //Serial.print("Frame:");
    //Serial.println(Frame++);
    return n;
  }


}

void start_capture()
{
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}


void handleClientData(String dataString)
{
  String h = "Hola";
  Serial.print("Data received");
  Serial.println(dataString);
  if (dataString.startsWith("capture"))
  {
    Serial.println(dataString);
    myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
    //myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
    //webSocketServer.sendData(h);
    start_capture();
    //webSocketServer.disconnectStream();
  }
  if (dataString.startsWith("HD"))
  {
    Serial.println(dataString);
    //myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
    myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
    //webSocketServer.sendData(h);
    start_capture();
    //webSocketServer.disconnectStream();
  }

}


void setup()
{
  Serial.begin(115200);
  delay(10);
  myservo.setPeriodHertz(50);    // standard 50 hz servo
  myservo.attach(15, 1000, 2000); // attaches the servo on pin 18 to the servo object


  uint8_t vid, pid;
  uint8_t temp;

#if defined(__AVR__) || defined(ESP32)
  Wire.begin();
#endif

#if defined(__arm__)
  Wire1.begin();
#endif

  Serial.println("ESP32 CS = 5");
  Serial.println(CS);
  Serial.println("ArduCAM Start!");
  // set the CS as an output:
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  //SPI.setFrequency(8000000); //4MHz

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
  Serial.print("VID:"); Serial.println(vid);

  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  Serial.print("PID:"); Serial.println(pid);
  if ((vid != 0x56) || (pid != 0x42))
    Serial.println("Can't find OV5642 module!");
  else
    Serial.println("OV5642 detected.");

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  //myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
  myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
  //myCAM.OV5642_set_JPEG_size(OV5642_320x240);
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.clear_fifo_flag();
  myCAM.write_reg(ARDUCHIP_FRAMES, 0x00);

  // We start by connecting to a WiFi network
  //    WiFiMulti.addAP("Casa", "Dali12345");

  Serial.println();
  Serial.println();
  Serial.print("Waiting for WiFi... ");

  /*  while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }*/
  WiFi.begin("Casa", "Dali12345");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);



  Serial.print("Connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections


  if (!client.connect(host, port)) {
    Serial.println("Connection failed.");
    Serial.println("Waiting 5 seconds before retrying...");
    delay(5000);
    return;
  }

  myCAM.clear_fifo_flag();
  start_capture();
}


void loop()
{
  int total_time = 0;
  if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK) &&client.connected() )
  {
    Serial.println("CAM Capture Done!");
    total_time = millis();
    client.println("POST /fileUpload.php HTTP/1.1");
    client.println("Host: www.dali.com.co");
    client.println("Connection: keep-alive");
    client.print("Content-Length: ");
    client.println(myCAM.read_fifo_length() + 287);
    client.println("Cache-Control: max-age=0");
    client.println("Upgrade-Insecure-Requests: 1");
    client.println("Content-Type: multipart/form-data; boundary=----WebKitFormBoundarydCfMBvt4BkYBFF5j");
    client.println("User-Agent: Arduino/1.0");
    client.println("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3");
    client.println("Accept-Encoding: gzip, deflate");
    client.println("Accept-Language: es-US,es;q=0.9,es-419;q=0.8,en;q=0.7");
    client.println();
    client.println("------WebKitFormBoundarydCfMBvt4BkYBFF5j");
    client.println("Content-Disposition: form-data; name=\"myfile\"; filename=\"test.jpg\"");
    client.println("Content-Type: image/jpeg");
    client.println();
    read_fifo_burst(myCAM);
    client.println("\r\n------WebKitFormBoundarydCfMBvt4BkYBFF5j");
    client.println("Content-Disposition: form-data; name=\"submit\"");
    client.println("Upload Image");
    client.println("\r\n------WebKitFormBoundarydCfMBvt4BkYBFF5j--");
    total_time = millis() - total_time;
   /* Serial.print("total_time used (in miliseconds):");
    Serial.println(total_time, DEC);
    Serial.println("CAM send Done!");*/
    //Clear the capture done flag
    //myCAM.clear_fifo_flag();
      // Read all the lines of the reply from server and print them to Serial
  /* delay(1000);
    Serial.println(client.available());
    String line = client.readStringUntil('\n');
    //Serial.println(line);
    if (line.length() > 0) 
    {
      Serial.print(line);
    }*/
    
  }

Serial.println("Waiting 20 seconds before restarting...");

  client.stop();
  start_capture();
  Serial.print("[HTTP] begin...\n");    
  http.begin("http://www.dali.com.co/camera.php"); //HTTP
  Serial.print("[HTTP] GET...\n");
 // start connection and send HTTP header
   int httpCode = http.GET();
// httpCode will be negative on error
        if(httpCode > 0) {
          const size_t capacity = JSON_OBJECT_SIZE(3) + 60;
          DynamicJsonBuffer jsonBuffer(capacity);
          
          const char* json = "{\"right\":180,\"left\":180,\"resolution\":\"OV5642_1024x1768\"}";
          //JsonObject& root = jsonBuffer.parseObject(json);
          JsonObject& root = jsonBuffer.parseObject(http.getString());
          
          
          int right = root["right"]; // 180
          int left = root["left"]; // 180
          const char* resolution = root["resolution"]; // "OV5642_1024x1768"
          Serial.print("Right ");
          Serial.println(right);
          myservo.write(right);  
          Serial.print("Left ");
          Serial.println(left);
          Serial.print("Resolution ");
          Serial.println(resolution);
          if(strcmp(resolution,"OV5642_1024x768")==0)
            myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
          if(strcmp(resolution,"OV5642_320x240")==0)
            myCAM.OV5642_set_JPEG_size( OV5642_320x240);  
        }

        http.end();
  
  client.connect("www.dali.com.co", 80);
  
  delay(2000);
  
}


void read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp, temp_last;
  static int i = 0;
  static uint8_t first_packet = 1;
  byte buf[2048];
  length = 0;
  buf[i] = 0xFF;

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
        sendClientData((char*)buf, length, 0x02, 2048); //First frame
        first_packet = 0;
      }
      else
      {
        sendClientData((char*)buf, length, 0x00, 2048); //Continuation
      }
      i = 0;
      buf[i++] = temp;
    }
    yield();
  }
  //Write the remain bytes in the buffer
  if (i > 0)
  {
    Serial.println("write last bytes");
    sendClientData((char*)buf, length, 0x80, i); //FIN Frame
    //delay(10);
    Serial.println("finish write last 256 bytes");
    i = 0;
    first_packet = 1;
  }
  yield();
  myCAM.CS_HIGH();
}
