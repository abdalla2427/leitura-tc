/*
Sketch para leitura de sensor de corrente SCT013 em valor rms, usando ESP32
UFF - PIBIC - 2019-2020
Lucas Abdalla Menezes e Rainer Zanghi
*/

//TODO: incluir vetor para armazenar últimos valores rms lidos - qual periodicidade?

#include <WiFi.h>
//#include <ESPAsyncWebServer.h>
#include <aWOT.h>
#include "time.h"

//const char* ssid     = "GVT-596D";
//const char* password = "laion123";

//const char* ssid     = "TEE_420A";
//const char* password = "#L@BTEE#";

const char* ssid     = "cafofo2";
const char* password = "0123456789012";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;//-3h GMT
const int   daylightOffset_sec = 0;//3600s if daylight saving time

bool deveReiniciar =false;
uint32_t oldfreeheap=0;

WiFiServer server(80);
Application app;

#define ADC_SAMPLES 512
#define I_RMS_VEC_SIZE 256

hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile DRAM_ATTR float i_rms;
volatile DRAM_ATTR float i_rms_data[I_RMS_VEC_SIZE]={0};
volatile DRAM_ATTR uint32_t i_rms_data_idx = 0;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES]={0};
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES]={0};
volatile uint32_t buffer_adc_data[ADC_SAMPLES]={0};
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES]={0};
volatile uint32_t timer0_t=80;
volatile uint32_t wificheckcount=0;
  
//timer0 callback - ADC sampling timer
void IRAM_ATTR onTimer(){
  portENTER_CRITICAL_ISR(&timerMux);
  //check for wild pointers
  if (isrCounter<ADC_SAMPLES){
    // Read analog sample, save sample time and increment ISR sample counter
    adc_data[isrCounter] = analogRead(34);
    sampletime_us[isrCounter]=micros();
    //adjust sample's timestamp referenced to the first sample
    if (isrCounter)
      sampletime_us[isrCounter]=sampletime_us[isrCounter]-sampletime_us[0];
    if (isrCounter==(ADC_SAMPLES-1))
      sampletime_us[0]=0;
  }
    isrCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}

//timer1 callback - restart ADC sampling timer for a new rms calculation
void IRAM_ATTR funcaoTimer1(){
      timerRestart(timer0);
}

//rms calculation function - using count values
float calcularRms(volatile uint32_t arr[]) {
  float square = 0;
  float aux;
  //use int ((1/f)/timer0_t) = 167 samples from ADC_SAMPLES for 1 period
  //where: f = 60Hz, timer0_t = ADC sampling time = 100us
  //44 to 210 corresponds to 167 samples (1 period) in the middle of a vector with 256 elements
  //generalizing : 
  int min = int(ADC_SAMPLES/2) - int((1/(timer0_t*60*.000001))/2);
  int max = min + int(1/(timer0_t*60*.000001));
  for (int i = min ; i < max ;i++) {
    //aux converts ADC sample (in counts) to A, where:
    //1845 - DC bias in counts
    //6.0 - Imax / turns - 30 A / 5 wires
    //1.637 - DC bias in V
    aux = ((arr[i]/1845.0) - 1.0) * 6.0 * 1.637;
    square += aux*aux;
  }
  //use int ((1/f)/timer0_t) = 167 samples from ADC_SAMPLES for 1 period
  //generalizing - number of samples is (max-min)
  return sqrt(square/(max-min));
}

//callback for all others
void notFound(Request &req, Response &res) {
  if (res.statusSent()) {
    return;
  }

  res.status(404);
  res.set("Content-Type", "text/plain");
  res.print("Not found.");
}

//callback for GET /
//send all values
void handle_alldata(Request &req, Response &res){
  int tamanhoVetorEnvio = (6 * ADC_SAMPLES) + 2 + (12 * ADC_SAMPLES) + 8 + 1;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;

  //append adc samples vector (in counts)
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    if (i<(ADC_SAMPLES-1))
      retorno = snprintf(&envio[posicaoPonteiro], 6, "%d,", buffer_adc_data[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 5, "%d", buffer_adc_data[i]);

    posicaoPonteiro += retorno;
  }
  //add separator between vectors
  retorno = snprintf(&envio[posicaoPonteiro], 3, "%s", "!!");
  posicaoPonteiro += retorno;
  
  //append sample time vector (in us)
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    //append sample timestamp referenced to the first sample
    if (i<(ADC_SAMPLES-1))
      retorno = snprintf(&envio[posicaoPonteiro], 12, "%d,", buffer_sampletime_us[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 11, "%d", buffer_sampletime_us[i]);

    posicaoPonteiro += retorno;
  }
  //append rms current (in A)
  retorno = snprintf(&envio[posicaoPonteiro], 8, " %.3f", i_rms);
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(envio);
}

//callback for GET /freeheap
//send old and new ESP.freeheap result in bytes only
void handle_freeheap(Request &req, Response &res){
  int tamanhoVetorEnvio = 22 + 1;//uint32_t max has 10+1 chars
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  uint32_t freeheap=0;
  freeheap=ESP.getFreeHeap();
  posicaoPonteiro = snprintf(&envio[0], 12, "%d ", oldfreeheap);
  snprintf(&envio[posicaoPonteiro], 11, "%d", freeheap);
  oldfreeheap = freeheap;
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(envio);
}

//callback for GET /i_rms_data
//send i_rms vector and last value index
void handle_i_rms_data(Request &req, Response &res){
  int tamanhoVetorEnvio = 10 * I_RMS_VEC_SIZE + 5;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;

  //append rms current vector (in A) 
  //arranging the cyclic vector in chronological order
  int retorno;
  for (int i = i_rms_data_idx; i < I_RMS_VEC_SIZE; i++)
  {   
    if (i != (i_rms_data_idx - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 10, "%.3f,", i_rms_data[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 9, "%.3f", i_rms_data[i]);
    
    posicaoPonteiro += retorno; 
  }
  if (i_rms_data_idx)
    for (int i=0;i<i_rms_data_idx;i++)
    {
      if (i != (i_rms_data_idx - 1))
        retorno = snprintf(&envio[posicaoPonteiro], 10, "%.3f,", i_rms_data[i]);
      else
        retorno = snprintf(&envio[posicaoPonteiro], 9, "%.3f", i_rms_data[i]);
      
      posicaoPonteiro += retorno; 
    }

  snprintf(&envio[posicaoPonteiro], 5, " %d", i_rms_data_idx);

  //send char vector to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(envio);
}

//callback for GET /i_rms
//send i_rms only
void handle_i_rms(Request &req, Response &res){
  int tamanhoVetorEnvio = 9;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  snprintf(&envio[posicaoPonteiro], 9, "%.3f", i_rms);
  //append rms current (in A)
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(envio); 
}

//callback for GET /adc_data
//send only adc values used on rms calculation
void handle_adc_data(Request &req, Response &res){
  int min = int(ADC_SAMPLES/2) - int((1/(timer0_t*60*.000001))/2);
  int max = min + int(1/(timer0_t*60*.000001));

  int tamanhoVetorEnvio = (6 * (max - min)) + 2 + ((12 * (max - min)) + 10 + 1);
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;

  //append adc samples vector (in counts)
  for (int i = min ; i < max ;i++) 
  {
    if (i<(max-1))
      retorno = snprintf(&envio[posicaoPonteiro], 6, "%d,", buffer_adc_data[i]);
    else 
      retorno = snprintf(&envio[posicaoPonteiro], 5, "%d", buffer_adc_data[i]);
  
    posicaoPonteiro += retorno; 
  
  }
  //add separator between vectors
  retorno = snprintf(&envio[posicaoPonteiro], 3, "%s", "!!");
  posicaoPonteiro += retorno;
  
  //append sample time vector (in us)
  for (int i = min ; i < max ;i++) 
  {
    if (i<(max-1))
      retorno = snprintf(&envio[posicaoPonteiro], 12, "%d,", buffer_sampletime_us[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 11, "%d", buffer_sampletime_us[i]);
    posicaoPonteiro += retorno;
  }
  //append rms current (in A)
  retorno = snprintf(&envio[posicaoPonteiro], 10, " %.3f", i_rms);
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200); 
  res.print(envio); 
}


//using time.h tm struct to print time
//refer to: http://www.cplusplus.com/reference/ctime/tm/
//and https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
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
  printLocalTime();
  oldfreeheap=ESP.getFreeHeap();
  
  //webserver app routing
  app.get("/",&handle_alldata);
  app.get("/freeheap", &handle_freeheap);
  app.get("/i_rms_data", &handle_i_rms_data);
  app.get("/i_rms", &handle_i_rms);
  app.get("/adc_data",&handle_adc_data);
  app.use(&notFound);
  
  server.begin();
  
  //warm up A/D configuration before interruption
  int temp = analogRead(34);

  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer0 = timerBegin(0, 80, true);
  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer0, &onTimer, true);
  // Set alarm to call onTimer function every sampletimeinterval us.
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer0, timer0_t, true);
  // Enable/start an alarm
  timerAlarmEnable(timer0);
  // Stop alarm - timer1 will start it
  timerStop(timer0);
  
  // Use 2nd timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer1 = timerBegin(1, 80, true);
  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer1, &funcaoTimer1, true);
  // Set alarm to call onTimer function every 0.5 s.
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer1, 500000, true);
  // Start an alarm
  timerAlarmEnable(timer1);
}

void loop() {
  uint32_t isrCount = 0;
  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    // Read the interrupt count
    //ensures no reading/writing to variables used by ISR
    portENTER_CRITICAL(&timerMux);
    isrCount = isrCounter;
    portEXIT_CRITICAL(&timerMux);
  }
  // If all ADC_SAMPLES values were sampled
  if (isrCount >ADC_SAMPLES-1) {
    // If timer is still running
    if (timer0) {
      // Stop and keep timer
      timerStop(timer0);
      //ensures no reading/writing to variables used by ISR
      portENTER_CRITICAL(&timerMux); 
      isrCounter = 0;
      for (int i=0;i<ADC_SAMPLES;i++)
      {
        buffer_adc_data[i] = adc_data[i]; 
        buffer_sampletime_us[i] = sampletime_us[i];
      }
      portEXIT_CRITICAL(&timerMux);
      //print sampling results
      //Serial.print("Foram lidas ");
      //Serial.print(isrCount);
      //Serial.println(" amostras.");
      //Serial.print("i_rms = ");
      i_rms = calcularRms(buffer_adc_data);
      //Serial.print(i_rms, 3);
      //Serial.println(" A");
      //i_rms last values vector handling
      i_rms_data[i_rms_data_idx]=i_rms;
      //prevent saving zeros or invalid values to rms vector
      if ((i_rms<=0.0)||(i_rms>99.999))
        if ((i_rms_data_idx)==0)//first element
          i_rms_data[i_rms_data_idx]=0.001;
        else//not first element
          i_rms_data[i_rms_data_idx] = i_rms_data[i_rms_data_idx-1];
      i_rms_data_idx++;
      if (i_rms_data_idx>=I_RMS_VEC_SIZE) i_rms_data_idx=0;
      //wifi check counter increases every rms sample
      wificheckcount++;
    }
  }
  //verify wifi connection and reboot
  //allow half buffer to be filled before checking
  if (wificheckcount>(I_RMS_VEC_SIZE/2)) {
    wificheckcount=0;
    int wifistatus = WL_CONNECTED;
    wifistatus = WiFi.status();
    switch (wifistatus) {
      case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
      case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
      case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
      case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
      case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
      case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
      case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
      case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
      default: Serial.println(wifistatus);
    }    
    if(wifistatus != WL_CONNECTED)
      deveReiniciar=true; 
  }
  //webserver code to process a connected client aWOT
  WiFiClient client = server.available();

  if (client.connected()) {
    app.process(&client);
  }

    if(deveReiniciar){
    ESP.restart();
  }
}
