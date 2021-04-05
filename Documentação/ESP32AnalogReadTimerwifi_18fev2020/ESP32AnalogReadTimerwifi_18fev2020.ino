/*

 */
#include <WiFi.h>
//#include <math.h>

const char* ssid     = "GVT-596D";
const char* password = "laion123";

//const char* ssid     = "TEE_420A";
//const char* password = "#L@BTEE#";

WiFiServer server(80);

#define ADC_SAMPLES 256

hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

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
  volatile uint32_t square = 0;
  
  for (int i = 43; i <= 167 ;i++) {
    square += arr[i]*arr[i];
  }
  double mean = ((double)square)/256;
  return (double)sqrt(mean);
}

void setup() {
  Serial.begin(115200);
  
  // We start by connecting to a WiFi network
  
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
 // timer0 = timerBegin(0, 80, true);
  timer1 = timerBegin(1, 80, true);

  // Attach onTimer function to our timer.
//  timerAttachInterrupt(timer0, &onTimer, true);
  timerAttachInterrupt(timer1, &funcaoTimer1, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
//  timerAlarmWrite(timer0, 100, true);
  timerAlarmWrite(timer1, 500000, true);


  // Start an alarm
  timerAlarmEnable(timer1);
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
//      timer0 = NULL;
      // Print it
      Serial.print("Timer executado ");
      Serial.print(isrCounter);
      Serial.println(" vezes.");
      Serial.println(calcularRms(buffer_adc_data));
      isrCounter = 0;
      for (int i=0;i<ADC_SAMPLES;i++)
      {
        buffer_adc_data[i] = adc_data[i]; 
        buffer_sampletime_us[i] = sampletime_us[i];
      }
//      for (int i=0;i<ADC_SAMPLES;i++)
//      {
//        Serial.print(adc_data[i]);
//        if (i<(ADC_SAMPLES-1))
//          Serial.print(",");
//      }
//      Serial.println();
//      for (int i=0;i<ADC_SAMPLES;i++)
//      {
//        Serial.print(sampletime_us[i]);
//        if (i<(ADC_SAMPLES-1))
//          Serial.print(",");
//      }
//      Serial.println();
    }
  }
  
  WiFiClient client = server.available();   // listen for incoming clients

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
            client.println("Content-type:text/plain");
            client.println("testeeee");
            client.println();

            // the content of the HTTP response follows the header:
//            client.print("Click <a href=\"/H\">here</a> to turn the LED on pin 5 on.<br>");
//            client.print("Click <a href=\"/L\">here</a> to turn the LED on pin 5 off.<br>");
           // client.print("Timer executado ");
           // client.print(isrCounter);
          //  client.print(" vezes.<br>");
            for (int i=0;i<ADC_SAMPLES;i++)
            {
              client.print(buffer_adc_data[i]);
              if (i<(ADC_SAMPLES-1))
                client.print(",");
            }
          //  client.print("<br>");
            client.print("MUDOU");
            for (int i=0;i<ADC_SAMPLES;i++)
            {
              client.print(buffer_sampletime_us[i]);
              if (i<(ADC_SAMPLES-1))
                client.print(",");
            }
          //  client.print("<br>");

            // The HTTP response ends with another blank line:
           // client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

//        // Check to see if the client request was "GET /H" or "GET /L":
//        if (currentLine.endsWith("GET /H")) {
//          digitalWrite(5, HIGH);               // GET /H turns the LED on
//        }
//        if (currentLine.endsWith("GET /L")) {
//          digitalWrite(5, LOW);                // GET /L turns the LED off
//        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
