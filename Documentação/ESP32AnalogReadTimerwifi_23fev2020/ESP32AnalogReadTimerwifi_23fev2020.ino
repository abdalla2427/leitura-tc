/*
Sketch para leitura de sensor de corrente SCT013 em valor rms, usando ESP32
UFF - PIBIC - 2019-2020
Lucas Abdalla Menezes e Rainer Zanghi
*/

//TODO: incluir vetor para armazenar últimos valores rms lidos - qual periodicidade?

#include <WiFi.h>
#include <WebServer.h>

//const char* ssid     = "GVT-596D";
//const char* password = "laion123";

const char* ssid     = "TEE_420A";
const char* password = "#L@BTEE#";

WebServer server(80);

#define ADC_SAMPLES 256

hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile DRAM_ATTR float i_rms;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES];
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES];
volatile uint32_t buffer_adc_data[ADC_SAMPLES];
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES];

//timer0 callback - ADC sampling timer
void IRAM_ATTR onTimer(){
  // Read analog sample, save sample time and increment ISR sample counter
  portENTER_CRITICAL_ISR(&timerMux);
  adc_data[isrCounter] = analogRead(34);
  sampletime_us[isrCounter]=micros();
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
  //44 to 210 corresponds to 167 samples (1 period) in the middle of the vector
  for (int i = 44; i <= 210 ;i++) {
    //aux converts ADC sample (in counts) to A, where:
    //1845 - DC bias in counts
    //6.0 - Imax / turns - 30 A / 5 wires
    //1.637 - DC bias in V
    aux = ((arr[i]/1845.0) - 1.0) * 6.0 * 1.637;
    square += aux*aux;
  }
  //use int ((1/f)/timer0_t) = 167 samples from ADC_SAMPLES for 1 period
  return sqrt(square/167);
}

void setup() {
  Serial.begin(115200); 
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  //connect to wifi AP (STA mode), verify connection state and inform IP address
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  server.onNotFound(handle_NotFound);
  server.on("/", handle_OnConnect);
  server.on("/i_rms", handle_OnConnect_i_rms);
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
  // Set alarm to call onTimer function every 100 us.
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer0, 100, true);
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

//callback for GET /
//send all values
void handle_OnConnect() {
  String ptr = "";
  //append adc samples vector (in counts)
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    ptr += String(buffer_adc_data[i]);
    if (i<(ADC_SAMPLES-1))
          ptr += ",";
  }
  //add separator between vectors
  ptr += "MUDOU";
  
  //append sample time vector (in us)
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    ptr += String(buffer_sampletime_us[i]);
    if (i<(ADC_SAMPLES-1))
          ptr += ",";
  }
  //append rms current (in A)
  ptr += String(" ") + String(i_rms,3);
  //send string to http client as text/plain
  server.send(200, "text/plain", ptr); 
}

//callback for GET /i_rms
//send i_rms only
void handle_OnConnect_i_rms() {
  String ptr = "";
  //append rms current (in A)
  ptr += String(i_rms,3);
  //send string to http client as text/plain
  server.send(200, "text/plain", ptr); 
}

//callback for all others
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
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
      //print sampling results
      Serial.print("Foram lidas ");
      Serial.print(isrCount);
      Serial.println(" amostras.");
      Serial.print("i_rms = ");
      i_rms = calcularRms(buffer_adc_data);
      Serial.print(i_rms, 3);
      Serial.println(" A");
      //ensures no reading/writing to variables used by ISR
      portENTER_CRITICAL(&timerMux); 
      isrCounter = 0;
      for (int i=0;i<ADC_SAMPLES;i++)
      {
        buffer_adc_data[i] = adc_data[i]; 
        buffer_sampletime_us[i] = sampletime_us[i];
      }
      portEXIT_CRITICAL(&timerMux);
    }
  }
  //web client processing via server.on functions
  server.handleClient();
}
