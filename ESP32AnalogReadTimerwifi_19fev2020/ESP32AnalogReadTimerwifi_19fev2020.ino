/*

 */
#include <WiFi.h>
#include <WebServer.h>

//#include <math.h>

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
float i_rms;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES];
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES];
volatile uint32_t buffer_adc_data[ADC_SAMPLES];
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES];

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  adc_data[isrCounter] = analogRead(34);
  lastIsrAt = micros();
  sampletime_us[isrCounter]=lastIsrAt;
  isrCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

void IRAM_ATTR funcaoTimer1(){
      timerRestart(timer0);
}

float calcularRms(volatile uint32_t arr[]) {
  volatile float square = 0;
  volatile float aux;
  
  for (int i = 44; i <= 210 ;i++) {
    aux = (arr[i]/1845.0 - 1) * 6 * 1.637;
    square += aux*aux;
  }
  
  float mean = (square)/167;
  return sqrt(mean);
}

void setup() {
  Serial.begin(115200); 
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
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
  server.begin();

  int temp = analogRead(34);

  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  timer0 = timerBegin(0, 80, true);
  timerAttachInterrupt(timer0, &onTimer, true);
  timerAlarmWrite(timer0, 100, true);
  timerAlarmEnable(timer0);
  timerStop(timer0);
  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer1 = timerBegin(1, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer1, &funcaoTimer1, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer1, 500000, true);


  // Start an alarm
  timerAlarmEnable(timer1);
}

void handle_OnConnect() {
  String ptr = "";
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    ptr += String(buffer_adc_data[i]);
    if (i<(ADC_SAMPLES-1))
          ptr += ",";
  }
  ptr += "MUDOU";
  
  for (int i=0;i<ADC_SAMPLES;i++)
  {
    ptr += String(buffer_sampletime_us[i]);
    if (i<(ADC_SAMPLES-1))
          ptr += ",";
  }
  ptr += String(" ") + String(i_rms);
  server.send(200, "text/plain", ptr); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void loop() {
  uint32_t isrCount = 0;
  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    isrCount = isrCounter;
    portEXIT_CRITICAL(&timerMux);
  }
  // Se todas as ADC_SAMPLES amostras já foram lidas
  if (isrCount >ADC_SAMPLES-1) {
    // If timer is still running
    if (timer0) {
      // Stop and free timer
      timerStop(timer0);
      Serial.print("Foram lidas ");
      Serial.print(isrCount);
      Serial.println(" amostras.");
      i_rms = calcularRms(buffer_adc_data);
      Serial.println(i_rms, 4);
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
  server.handleClient();
}
