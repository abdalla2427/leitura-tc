/*
Sketch para leitura de sensor de corrente SCT013 em valor rms, usando ESP32
UFF - PIBIC - 2019-2020
Lucas Abdalla Menezes e Rainer Zanghi
*/

//TODO: incluir vetor para armazenar últimos valores rms lidos - qual periodicidade?

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

//const char* ssid     = "GVT-596D";
//const char* password = "laion123";

//const char* ssid     = "TEE_420A";
//const char* password = "#L@BTEE#";

const char* ssid     = "cafofo2";
const char* password = "0123456789012";

AsyncWebServer server(80);

#define ADC_SAMPLES 512
#define I_RMS_VEC_SIZE 256

hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile DRAM_ATTR float i_rms;
volatile DRAM_ATTR float i_rms_data[I_RMS_VEC_SIZE];
volatile DRAM_ATTR uint32_t i_rms_data_idx = 0;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES];
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES];
volatile uint32_t buffer_adc_data[ADC_SAMPLES];
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES];
volatile uint32_t timer0_t=80;
volatile uint32_t wificheckcount=0;
  
//timer0 callback - ADC sampling timer
void IRAM_ATTR onTimer(){
  // Read analog sample, save sample time and increment ISR sample counter
  portENTER_CRITICAL_ISR(&timerMux);
  adc_data[isrCounter] = analogRead(34);
  sampletime_us[isrCounter]=micros();
  //adjust sample's timestamp referenced to the first sample
  if (isrCounter)
    sampletime_us[isrCounter]=sampletime_us[isrCounter]-sampletime_us[0];
  if (isrCounter==(ADC_SAMPLES-1))
    sampletime_us[0]=0;
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

//not found webserver callback
void handle_NotFound(AsyncWebServerRequest *request){
  request->send(404, "text/plain", "Not found");
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
  
  server.onNotFound(handle_NotFound);
  
  //callback for GET /
  //send all values
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String ptr = "";
    //append adc samples vector (in counts)
    for (int i=0;i<ADC_SAMPLES;i++)
    {
      ptr += String(buffer_adc_data[i]);
      if (i<(ADC_SAMPLES-1))
            ptr += ",";
    }
    //add separator between vectors
    ptr += "!!";
    
    //append sample time vector (in us)
    for (int i=0;i<ADC_SAMPLES;i++)
    {
      //append sample timestamp referenced to the first sample
      ptr += String(buffer_sampletime_us[i]);
      if (i<(ADC_SAMPLES-1))
            ptr += ",";
    }
    //append rms current (in A)
    ptr += String(" ") + String(i_rms,3);
    //send string to http client as text/plain
    request->send(200, "text/plain", ptr); 
  });
  
  //callback for GET /adc_data
  //send only adc values used on rms calculation
  server.on("/adc_data", HTTP_GET, [](AsyncWebServerRequest *request){
    String ptr = "";
    //append adc samples vector (in counts)
    int min = int(ADC_SAMPLES/2) - int((1/(timer0_t*60*.000001))/2);
    int max = min + int(1/(timer0_t*60*.000001));
    for (int i = min ; i < max ;i++) 
    {
      ptr += String(buffer_adc_data[i]);
      if (i<(max-1))
            ptr += ",";
    }
    //add separator between vectors
    ptr += "!!";
    
    //append sample time vector (in us)
    for (int i = min ; i < max ;i++) 
    {
      //append sample timestamp referenced to the first sample
      ptr += String(buffer_sampletime_us[i]);
      if (i<(max-1))
            ptr += ",";
    }
    //append rms current (in A)
    ptr += String(" ") + String(i_rms,3);
    //send string to http client as text/plain
    request->send(200, "text/plain", ptr); 
  });

  //callback for GET /i_rms
  //send i_rms only
  server.on("/i_rms", HTTP_GET, [](AsyncWebServerRequest *request){
    String ptr = "";
    //append rms current (in A)
    ptr += String(i_rms,3);
    //send string to http client as text/plain
    request->send(200, "text/plain", ptr); 
  });

  //callback for GET /i_rms_data
  //send i_rms vector and last value index
  server.on("/i_rms_data", HTTP_GET, [](AsyncWebServerRequest *request){
    String ptr = "";
    //append rms current vector (in A) 
    //arranging the cyclic vector in chronological order
    for (int i=i_rms_data_idx;i<I_RMS_VEC_SIZE;i++)
    {
      ptr += String(i_rms_data[i],3);
      if (i!=(i_rms_data_idx-1))
            ptr += ",";
    }
    if (i_rms_data_idx)
      for (int i=0;i<i_rms_data_idx;i++)
      {
          ptr += String(i_rms_data[i],3);
          if (i!=(i_rms_data_idx-1))
              ptr += ",";
      }
    ptr += String(" ") + String(i_rms_data_idx);
    //send string to http client as text/plain
    request->send(200, "text/plain", ptr); 
  });

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
      ESP.restart(); 
  }
}
