/*

 */
#include <WiFi.h>

const char* ssid     = "GVT-596D";
const char* password = "laion123";

WiFiServer server(80);

#define ADC_SAMPLES 256

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static DRAM_ATTR uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;
static DRAM_ATTR uint32_t adc_data[ADC_SAMPLES];
static DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES];

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

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 100, true);

  // Start an alarm
  timerAlarmEnable(timer);
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
    if (timer) {
      // Stop and free timer
      timerEnd(timer);
      timer = NULL;
      // Print it
      Serial.print("Timer executado ");
      Serial.print(isrCounter);
      Serial.println(" vezes.");
      for (int i=0;i<ADC_SAMPLES;i++)
      {
        Serial.print(adc_data[i]);
        if (i<(ADC_SAMPLES-1))
          Serial.print(",");
      }
      Serial.println();
      for (int i=0;i<ADC_SAMPLES;i++)
      {
        Serial.print(sampletime_us[i]);
        if (i<(ADC_SAMPLES-1))
          Serial.print(",");
      }
      Serial.println();
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
              client.print(adc_data[i]);
              if (i<(ADC_SAMPLES-1))
                client.print(",");
            }
          //  client.print("<br>");
            client.print("MUDOU");
            for (int i=0;i<ADC_SAMPLES;i++)
            {
              client.print(sampletime_us[i]);
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
