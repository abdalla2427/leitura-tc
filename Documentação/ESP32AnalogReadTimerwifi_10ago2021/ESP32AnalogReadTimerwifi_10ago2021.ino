/*
  Sketch para leitura de sensor de corrente SCT013 em valor rms, usando ESP32
  UFF - PIBIC - 2019-2020
  Lucas Abdalla Menezes e Rainer Zanghi
*/

//TODO: incluir vetor para armazenar últimos valores rms lidos - qual periodicidade?

#include <WiFi.h>
#include <aWOT.h>
#include "time.h"
#include <ArduinoJson.h>

 const char* ssid = "VIVOFIBRA-8CDC";
 const char* password = "EAEA7A8CDC";

//  const char* ssid = "NET_2G41237E";
//  const char* password = "B341237E";

//const char* ssid     = "ele nao ";
//const char* password = "h24i26d23";

//const char* ssid     = "Laura";
//const char* password = "helena1997";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; //-3h GMT
const int daylightOffset_sec = 0;  //3600s if daylight saving time
bool deveReiniciar = false;
uint32_t oldfreeheap = 0;

WiFiServer server(80);
Application app;

#define ADC_SAMPLES 512
#define I_RMS_VEC_SIZE 256
#define tamanhoJanela 5
#define tamanhoCamada1 5
#define tamanhoCamada2 8
#define tamanhoCamada3 8
#define tamanhoCamada4 5
#define tamanhoSaidaRede 3
#define dimensaoCamadasRede 5
#define tamanhoMaximoCamdas 8
#define debug 1
#define tamanhoVetorEventosDetectados 256
#define tamanhotimestamp 3 + 3 + 4 + 1 + 3 + 3 + 3 //"%d/%m/%Y-%H:%M:%S"

hw_timer_t *timer0 = NULL;
hw_timer_t *timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile DRAM_ATTR float i_rms;
volatile DRAM_ATTR float CONSUMO_TOTAL = 0.00;
volatile DRAM_ATTR float i_rms_data[I_RMS_VEC_SIZE] = {0};
volatile DRAM_ATTR uint32_t i_rms_data_idx = 0;
volatile DRAM_ATTR uint32_t i_rms_data_idx_anterior = 0;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES] = {0};
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES] = {0};
volatile uint32_t buffer_adc_data[ADC_SAMPLES] = {0};
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES] = {0};
volatile uint32_t timer0_t = 80;
volatile uint32_t wificheckcount = 0;
volatile uint32_t vetorEventosDetectadosIdx = 0;
volatile DRAM_ATTR uint32_t vetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
volatile DRAM_ATTR uint32_t tempoVetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
volatile DRAM_ATTR float variacaoDeCorrenteNoEvento[tamanhoVetorEventosDetectados] = {0};
volatile DRAM_ATTR float capturasRealizadas[tamanhoVetorEventosDetectados] = {0};
volatile int rmsCalculados = 0;
bool estadoAtualCaptura = false;
/*
   Parâmetros da rede
*/
float topologiaRede[dimensaoCamadasRede] = {tamanhoCamada1, tamanhoCamada2, tamanhoCamada3, tamanhoCamada4, tamanhoSaidaRede};
DRAM_ATTR float ***listaPesos = new float**[tamanhoMaximoCamdas];
DRAM_ATTR float **listaBias = new float*[dimensaoCamadasRede];
DRAM_ATTR float **valorDosNos = new float*[dimensaoCamadasRede + 1];
int indexCapturas = 0;

/*
   Fim definição dos parâmetros da rede
*/

/*
   Funções utilizadas pela rede
*/
float maximo(float a, float b)
{
  return ((a >= b) ? a : b);
}

float reluFunction(float valor)
{
  return maximo(0, valor);
}

float logisticFunction(float valor)
{
  if (valor >= 10)
    return 1;
  if (valor < -10)
    return 0;
  else
  {
    float resultado = (1 / (1 + expf(valor * (-1.0))));

    if (resultado >= 0.5)
      return 1.0;
    else
      return 0.0;
  }
}

//esta função retorna a classe com maior probabilidade
//após a normalização exponencial
int softMax(float *vetorZ, int tamanhoVetorZ)
{
  float soma = 0;
  float max = 0;
  float saida[tamanhoVetorZ];
  int i;
  int saidaMax = 0;

  for (i = 0; i < tamanhoVetorZ; i++)
  {
    soma += expf(vetorZ[i]);
  }
  
  for (i = 0; i < tamanhoVetorZ; i++)
  {
    saida[i] = (expf(vetorZ[i])) / (soma);
  }

  max = saida[0];

  for (i = 1; i < tamanhoVetorZ; i++)
  {
    if (saida[i] > max)
    {
      max = saida[i];
      saidaMax = i;
    }
  }

  return saidaMax;
}

//teste fun


//função que faz a forward propagation de uma camada escondida ou de entrada
//para uma camada escondida
int propagarEntradaParaProximaCamada(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float **pesos, float biasRede[])
{
  int i;
  int j;
  float auxEntrada;
  float auxPesos;
  for (i = 0; i < tamanhoEntrada; i++)
  {
    for (j = 0; j < tamanhoSaida; j++)
    {
      auxEntrada = entrada[i];
      auxPesos = pesos[i][j];
      saida[j] += auxEntrada * auxPesos;
    }
  }

  for (i = 0; i < tamanhoSaida; i++)
  {
    saida[i] = reluFunction(saida[i] + biasRede[i]);
  }

  return 0;
}


//função que faz a forward propagation da última camada escondida para a saída
int propagarEntradaParaSaida(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float **pesos, float biasRede[])
{
  int i;
  int j;
  float auxEntrada;
  float auxPesos;
  for (i = 0; i < tamanhoEntrada; i++)
  {
    for (j = 0; j < tamanhoSaida; j++)
    {
      auxEntrada = entrada[i];
      auxPesos = pesos[i][j];
      saida[j] += auxEntrada * auxPesos;
    }
  }

  for (i = 0; i < tamanhoSaida; i++)
  {
    saida[i] = saida[i] + biasRede[i];
  }

  return softMax(saida, tamanhoSaida);
}

/*
   Fim definição de funções utilizadas pela rede
*/

//timer0 callback - ADC sampling timer
void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  //check for wild pointers
  if (isrCounter < ADC_SAMPLES)
  {
    // Read analog sample, save sample time and increment ISR sample counter
    adc_data[isrCounter] = analogRead(34);
    sampletime_us[isrCounter] = micros();
    //adjust sample's timestamp referenced to the first sample
    if (isrCounter)
      sampletime_us[isrCounter] = sampletime_us[isrCounter] - sampletime_us[0];
    if (isrCounter == (ADC_SAMPLES - 1))
      sampletime_us[0] = 0;
  }
  isrCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}

//timer1 callback - restart ADC sampling timer for a new rms calculation
void IRAM_ATTR funcaoTimer1()
{
  timerRestart(timer0);
}

//rms calculation function - using count values
float calcularRms(volatile uint32_t arr[])
{
  float square = 0;
  float aux;
  //use int ((1/f)/timer0_t) = 167 samples from ADC_SAMPLES for 1 period
  //where: f = 60Hz, timer0_t = ADC sampling time = 100us
  //44 to 210 corresponds to 167 samples (1 period) in the middle of a vector with 256 elements
  //generalizing :
  int min = int(ADC_SAMPLES / 2) - int((1 / (timer0_t * 60 * .000001)) / 2);
  int max = min + int(1 / (timer0_t * 60 * .000001));
  for (int i = min; i < max; i++)
  {
    //aux converts ADC sample (in counts) to A, where:
    //1845 - DC bias in counts
    //6.0 - Imax / turns - 30 A / 5 wires
    //1.637 - DC bias in V
    aux = ((arr[i] / 1845.0) - 1.0) * 6.0 * 1.637;
    square += aux * aux;
  }
  //use int ((1/f)/timer0_t) = 167 samples from ADC_SAMPLES for 1 period
  //generalizing - number of samples is (max-min)

  return sqrt(square / (max - min));
}


//callback for all others
void notFound(Request &req, Response &res)
{
  if (res.statusSent())
  {
    return;
  }

  res.status(404);
  res.set("Content-Type", "text/plain");
  res.print("Not found.");
}


//callback for GET /freeheap
//send old and new ESP.freeheap result in bytes only
void handle_freeheap(Request &req, Response &res)
{
  int tamanhoVetorEnvio = 22 + 1; //uint32_t max has 10+1 chars
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  uint32_t freeheap = 0;
  freeheap = ESP.getFreeHeap();
  posicaoPonteiro = snprintf(&envio[0], 12, "%d ", oldfreeheap);
  snprintf(&envio[posicaoPonteiro], 11, "%d", freeheap);
  oldfreeheap = freeheap;
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200);
  res.print(envio);
}

//callback for GET /events
//send all event log
void handle_events(Request &req, Response &res)
{
  StaticJsonDocument<1024> objeto;  

  JsonArray vetorVariacao = objeto.createNestedArray("variacao");
  JsonArray vetorEvento = objeto.createNestedArray("evento");
  JsonArray vetorTempo = objeto.createNestedArray("tempo");
  
  objeto["consumoTotal"] = CONSUMO_TOTAL;

  for(int i = 0; i < tamanhoVetorEventosDetectados; i++)
  {
    vetorVariacao.add(variacaoDeCorrenteNoEvento[i]);
    vetorTempo.add(tempoVetorEventosDetectados[i]);
    vetorEvento.add(vetorEventosDetectados[i]);
  }

  String json;
  serializeJson(objeto, json);
  
  res.set("Content-Type", "application/json");
  res.status(200);
  res.print(json);
}

/*
TODO: Implementar o tratamento de erro do JSONdeserialize
garantindo a integridade dos dados.
*/

//callback for POST /handle_update_weights
//update classifier weigths
void handle_update_weights(Request &req, Response &res)
{
  res.set("Content-Type", "application/json");
  String streamTeste = req.readString();
  StaticJsonDocument<1024> doc;

  deserializeJson(doc, streamTeste);
  JsonObject object = doc.as<JsonObject>();
  
  for (int camadaAtual = 0; camadaAtual < dimensaoCamadasRede; camadaAtual++) {
    int dimensaoCamadaAtual = topologiaRede[camadaAtual];
    JsonArray biasCamadaAtual = object["bias"][camadaAtual].as<JsonArray>();
    copyArray(biasCamadaAtual, listaBias[camadaAtual], dimensaoCamadaAtual);
  }

  int camadaEntrada = tamanhoJanela;
  for (int camadaAtual = 0; camadaAtual < dimensaoCamadasRede; camadaAtual++) {
    int dimensaoProximaCamada = topologiaRede[camadaAtual];
    for (int noDe = 0; noDe < camadaEntrada; noDe++) {
       JsonArray coefsCamdaAtual = object["coefs"][camadaAtual][noDe].as<JsonArray>();
       copyArray(coefsCamdaAtual, listaPesos[camadaAtual][noDe], dimensaoProximaCamada);
    }
    camadaEntrada = dimensaoProximaCamada;
  }
  
  res.status(200);
}

void handle_resultado_captura(Request &req, Response &res)
{
  StaticJsonDocument<1024> objeto;
  JsonArray vetorRms = objeto.to<JsonArray>();

  for(int i =0; i < indexCapturas; i++)
  {
    vetorRms.add(capturasRealizadas[i]);
    capturasRealizadas[i] = 0;
  }

  String json;
  serializeJson(objeto, json);

  res.set("Content-Type", "application/json");
  res.status(200);
  res.print(json);

  estadoAtualCaptura = false;
}

void handle_timestamps(Request &req, Response &res)
{
  StaticJsonDocument<512> objeto;
  JsonArray smptime = objeto.to<JsonArray>();

  for(int i =0; i < ADC_SAMPLES; i++)
  {
    smptime.add(sampletime_us[i]);
  }

  String json;
  serializeJson(objeto, json);

  res.set("Content-Type", "application/json");
  res.status(200);
  res.print(json);

  estadoAtualCaptura = false;
}


void handle_realizar_captura(Request &req, Response &res)
{
  estadoAtualCaptura = true;
  indexCapturas = 0;
  
  res.status(200);
  res.print(estadoAtualCaptura);

  indexCapturas = 0;
}

//using time.h tm struct to print time
//refer to: http://www.cplusplus.com/reference/ctime/tm/
//and https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
//modified to return timestamp string
void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //connect to wifi AP (STA mode), verify connection state and inform IP address
  int wifiretries = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
    //restart if not connected after n retries
    wifiretries++;
    if (wifiretries > 20)
      ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  for(int i = 0; i<tamanhoMaximoCamdas; i++){
     listaPesos[i] = new float*[tamanhoMaximoCamdas];
     for(int j = 0; j<tamanhoMaximoCamdas; j++){
         listaPesos[i][j] = new float[tamanhoMaximoCamdas];
         for(int k = 0; k<tamanhoMaximoCamdas;k++){
            listaPesos[i][j][k] = 0;
         }
     }
  }
  
  valorDosNos[0] =  new float[tamanhoJanela];
  for(int i = 0; i < dimensaoCamadasRede; i++) {
    int camadaAtualRede = topologiaRede[i];
    valorDosNos[i + 1] =  new float[camadaAtualRede];
    for(int j = 0; j < camadaAtualRede; j++) {
      valorDosNos[i][j] = 0;
    }
  }

  for(int i = 0; i < dimensaoCamadasRede ; i++) {
    int camadaAtualRede = topologiaRede[i];
    listaBias[i] =  new float[camadaAtualRede];
    for(int j = 0; j < camadaAtualRede; j++) {
      listaBias[i][j] = 0;
    }
  }
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  oldfreeheap = ESP.getFreeHeap();

  app.get("/freeheap", &handle_freeheap);
  app.get("/events", &handle_events);
  app.post("/AtualizarClassificador", &handle_update_weights);
  app.get("/RealizarCaptura", &handle_realizar_captura);
  app.get("/ObterResultadoCaptura", &handle_resultado_captura);
  app.get("/Tempos", &handle_timestamps);

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

void loop()
{
  uint32_t isrCount = 0;
  int resultadoclassificador = 0;
  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE)
  {
    // Read the interrupt count
    //ensures no reading/writing to variables used by ISR
    portENTER_CRITICAL(&timerMux);
    isrCount = isrCounter;
    portEXIT_CRITICAL(&timerMux);
  }
  // If all ADC_SAMPLES values were sampled
  if (isrCount > ADC_SAMPLES - 1)
  {
    // If timer is still running
    if (timer0)
    {
      // Stop and keep timer
      timerStop(timer0);
      //ensures no reading/writing to variables used by ISR
      portENTER_CRITICAL(&timerMux);  
      isrCounter = 0;
      for (int i = 0; i < ADC_SAMPLES; i++)
      {
        buffer_adc_data[i] = adc_data[i];
        buffer_sampletime_us[i] = sampletime_us[i];
      }
      portEXIT_CRITICAL(&timerMux);

      i_rms = calcularRms(buffer_adc_data);
      
      if (estadoAtualCaptura && (indexCapturas < tamanhoVetorEventosDetectados)) {
        capturasRealizadas[indexCapturas] = i_rms;
        indexCapturas++;
      }
      
      i_rms_data[i_rms_data_idx] = i_rms;
      
      //prevent saving zeros or invalid values to rms vector
      if ((i_rms <= 0.0) || (i_rms > 99.999))
      if ((i_rms_data_idx) == 0) //first element
        i_rms_data[i_rms_data_idx] = 0.001;
      else //not first element
        i_rms_data[i_rms_data_idx] = i_rms_data[i_rms_data_idx - 1];
      

      //create classifier input vector
      for (int i = 0; i < tamanhoJanela; i++)
      {
        if (i < (tamanhoJanela - 1))
        {
          valorDosNos[0][i] = valorDosNos[0][i + 1];
        }
        else
        {
          valorDosNos[0][i] = i_rms_data[i_rms_data_idx];
          CONSUMO_TOTAL = CONSUMO_TOTAL + valorDosNos[0][i];
        }
      }

      //run classifier after tamanhoJanela rms samples
      if (rmsCalculados >= tamanhoJanela)
      {

        int camadaAtualRede = tamanhoJanela;
        for(int i = 0; i < dimensaoCamadasRede - 1; i++) {
          int proximaCamadaRede = topologiaRede[i];
          propagarEntradaParaProximaCamada(valorDosNos[i], camadaAtualRede, valorDosNos[i + 1], proximaCamadaRede, listaPesos[i], listaBias[i]);
          camadaAtualRede = proximaCamadaRede;
        }
        resultadoclassificador = propagarEntradaParaSaida(valorDosNos[4], tamanhoCamada4, valorDosNos[5], tamanhoSaidaRede, listaPesos[4], listaBias[4]);
        
        //só registra nos eventos as classes diferentes de zero
        if (resultadoclassificador > 0)
        {
          vetorEventosDetectados[vetorEventosDetectadosIdx] = resultadoclassificador;
          struct tm timeinfo;
          if (!getLocalTime(&timeinfo))
            tempoVetorEventosDetectados[vetorEventosDetectadosIdx] = 0;
          else
            tempoVetorEventosDetectados[vetorEventosDetectadosIdx] = mktime(&timeinfo);
            
          variacaoDeCorrenteNoEvento[vetorEventosDetectadosIdx] =  i_rms_data[i_rms_data_idx_anterior] - i_rms_data[i_rms_data_idx];

          vetorEventosDetectadosIdx++;
        }

        //garante que os neurônios são inicializados antes de uma nova classificação
        camadaAtualRede = tamanhoJanela;
        for(int i = 0; i < dimensaoCamadasRede; i++) {
          for(int j = 0; j < camadaAtualRede; j ++)
            {
              valorDosNos[i][j] = 0;
            }
          camadaAtualRede = topologiaRede[i];
        }
      }
      else rmsCalculados++;
      
      i_rms_data_idx_anterior = i_rms_data_idx;
      if (vetorEventosDetectadosIdx >= tamanhoVetorEventosDetectados)
        vetorEventosDetectadosIdx = 0;
      i_rms_data_idx++;
      if (i_rms_data_idx >= I_RMS_VEC_SIZE)
        i_rms_data_idx = 0;
      //wifi check counter increases every rms sample
      wificheckcount++;
    }
  }
  //verify wifi connection and reboot
  //allow half buffer to be filled before checking
  if (wificheckcount > (I_RMS_VEC_SIZE / 2))
  {
    wificheckcount = 0;
    int wifistatus = WL_CONNECTED;
    wifistatus = WiFi.status();
    switch (wifistatus)
    {
      case WL_NO_SHIELD:
        Serial.println("WL_NO_SHIELD");
        break;
      case WL_IDLE_STATUS:
        Serial.println("WL_IDLE_STATUS");
        break;
      case WL_NO_SSID_AVAIL:
        Serial.println("WL_NO_SSID_AVAIL");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("WL_SCAN_COMPLETED");
        break;
      case WL_CONNECTED:
        Serial.println("WL_CONNECTED");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("WL_CONNECT_FAILED");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("WL_CONNECTION_LOST");
        break;
      case WL_DISCONNECTED:
        Serial.println("WL_DISCONNECTED");
        break;
      default:
        Serial.println(wifistatus);
    }
    if (wifistatus != WL_CONNECTED)
      deveReiniciar = true;
  }
  //webserver code to process a connected client aWOT
  WiFiClient client = server.available();

  if (client.connected())
  {
    app.process(&client);
  }

  if (deveReiniciar)
  {
    ESP.restart();
  }
}
