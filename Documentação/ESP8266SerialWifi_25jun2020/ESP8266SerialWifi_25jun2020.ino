#include <WiFi.h>
#include <aWOT.h>
#include "time.h"

#define I_RMS_VEC_SIZE 256
#define TAMANHO_VETOR_RECEBIDO 7 * I_RMS_VEC_SIZE + 5

char* ssid     = "cafofo2";
char* password = "0123456789012";

char arrayRecebido[TAMANHO_VETOR_RECEBIDO] = {'\0'};

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;//-3h GMT
const int   daylightOffset_sec = 0;//3600s if daylight saving time

bool deveReiniciar =false;
uint32_t oldfreeheap=0;

WiFiServer server(80);
Application app;

//callback for all others
void notFound(Request &req, Response &res) {
  if (res.statusSent()) {
    return;
  }

  res.status(404);
  res.set("Content-Type", "text/plain");
  res.print("Not found.");
}

//callback for GET /i_rms_data
//send i_rms vector and last value index
void handle_i_rms_data(Request &req, Response &res){
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(arrayRecebido);
}


void setup() {
  Serial.begin(115200); 
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  //connect to wifi AP (STA mode), verify connection state and inform IP address
  int wifiretries=0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      //restart if not connected after n retries
      wifiretries++;
      if (wifiretries>10) ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  oldfreeheap=ESP.getFreeHeap();
  
  //webserver app routing
//  app.get("/",&handle_alldata);
//  app.get("/freeheap", &handle_freeheap);
  app.get("/i_rms_data", &handle_i_rms_data);
//  app.get("/i_rms", &handle_i_rms);
//  app.get("/adc_data",&handle_adc_data);
  app.use(&notFound);
  
  server.begin();
}

void loop() {
  int i = 0;

  while ((Serial.available() > 0) && (Serial.available() <= I_RMS_VEC_SIZE))
  {
    arrayRecebido[i] = Serial.read();
    i++;
  }
}
