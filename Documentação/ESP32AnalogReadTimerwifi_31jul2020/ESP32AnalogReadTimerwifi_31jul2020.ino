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

const char *ssid = "cafofo2";
const char *password = "0123456789012";

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
#define debug 1
#define tamanhoVetorEventosDetectados 256
#define tamanhotimestamp 3 + 3 + 4 + 1 + 3 + 3 + 3 //"%d/%m/%Y-%H:%M:%S"

hw_timer_t *timer0 = NULL;
hw_timer_t *timer1 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile DRAM_ATTR float i_rms;
volatile DRAM_ATTR float i_rms_data[I_RMS_VEC_SIZE] = {0};
volatile DRAM_ATTR uint32_t i_rms_data_idx = 0;
volatile DRAM_ATTR uint32_t isrCounter = 0;
volatile DRAM_ATTR uint32_t adc_data[ADC_SAMPLES] = {0};
volatile DRAM_ATTR uint32_t sampletime_us[ADC_SAMPLES] = {0};
volatile uint32_t buffer_adc_data[ADC_SAMPLES] = {0};
volatile uint32_t buffer_sampletime_us[ADC_SAMPLES] = {0};
volatile uint32_t timer0_t = 80;
volatile uint32_t wificheckcount = 0;
volatile uint32_t vetorEventosDetectadosIdx = 0;
volatile uint32_t vetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
volatile uint32_t tempoVetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
volatile int rmsCalculados = 0;
/*
 * Parâmetros da rede*
*/
float entradaRede[tamanhoJanela] = {0};
float camada1[tamanhoCamada1] = {0};
float camada2[tamanhoCamada2] = {0};
float camada3[tamanhoCamada3] = {0};
float camada4[tamanhoCamada4] = {0};
float saidaRede[tamanhoSaidaRede] = {0};

float biasEntrada[tamanhoCamada1] = {0.87252535, -1.4449011, -0.71409326, -0.41537152, 1.94327072};
float biasCamada1[tamanhoCamada2] = {0.37252674, -0.64896769, -0.28967707, 0.28759288, 0.23504868, -1.50150698, 1.67144081, -0.05702661};
float biasCamada2[tamanhoCamada3] = {-0.64484844, -0.46371082, 0.07778641, -0.16427905, -0.33593608, 0.38072141, -0.25793961, -1.24004925};
float biasCamada3[tamanhoCamada4] = {-0.59348973, -0.81781, -0.30258274, 0.51263149, -0.06164602};
float biasCamada4[tamanhoSaidaRede] = {0.84878705, -2.4239189, 1.63071996};

//pesosParaCamdaX[NohOrigem][NohDestino]
float pesosParaCamada1[tamanhoJanela][tamanhoCamada1] = {{-1.18496418, 0.64140717, -0.77415539, -1.80123882, -0.79284077},
                                                         {0.0361029, -0.03059286, -0.2391747, -0.21706224, -0.01866877},
                                                         {-0.17562649, 0.02274802, -0.45770447, 0.25139512, 0.01339328},
                                                         {-0.17085257, -0.02332232, 0.09089089, -0.2866767, -0.02401249},
                                                         {1.72382899, 1.39589836, -0.28894345, 2.26179342, 0.66801019}};

float pesosParaCamada2[tamanhoCamada1][tamanhoCamada2] = {{-0.57208006, -0.71459997, 0.53334976, 0.30400255, 1.46413863,
                                                           0.25664911, -1.33726718, 0.1733101},
                                                          {-0.72927241, 0.59315821, 0.83850536, 0.32987695, 0.27058192,
                                                           0.87472067, -1.14416923, -0.67809612},
                                                          {0.55498264, -0.28032752, -0.2882582, -0.50252076, -0.65282901,
                                                           0.24290678, -0.39168662, -0.31845073},
                                                          {-0.17732716, -2.273507, 0.52543837, 0.0984306, 0.93372687,
                                                           0.63436607, -1.13622212, 0.04290656},
                                                          {0.16271146, 0.49008856, -0.65230918, 0.39288215, 0.90462116,
                                                           -0.52165762, 0.71444108, -0.62640621}};

float pesosParaCamada3[tamanhoCamada2][tamanhoCamada3] = {{0.3072209,
                                                           0.27671881, 0.40949783, 0.12304813, 0.30723565,
                                                           0.04334686, -0.23158595, 0.53572436},
                                                          {-0.74092151, 0.56911632, -0.32905662, 0.14951505, -0.47167699,
                                                           1.5105639, 0.31418601, 0.45663189},
                                                          {-0.16091196, -0.32196502, 0.49568121, -0.27904373, -0.60864939,
                                                           -0.69687481, -0.08720293, 0.6572301},
                                                          {-0.05322763, -0.17471868, 0.83765792, 0.42520967, -0.59279323,
                                                           0.25122721, -0.03854194, 0.45579294},
                                                          {-0.9588594, -0.44425346, 0.63718265, 0.12887983, -0.5313578,
                                                           0.31029002, 0.99065322, 0.64650958},
                                                          {0.24524909, -0.46001529, -0.7110056, -1.24076937, -0.57750721,
                                                           -0.62669345, 0.83141009, 0.71205155},
                                                          {-0.39442963, 0.4188236, -0.64677693, -0.32728291, 0.10499741,
                                                           1.7127485, 0.44548229, -0.78668666},
                                                          {-0.23714551, -0.32692505, 0.4107153, -0.141423, 0.44509419,
                                                           0.0274867, 0.23591364, -0.29724069}};

float pesosParaCamada4[tamanhoCamada3][tamanhoCamada4] = {{-0.25589515,
                                                           0.46481038, 0.12496253, -0.38831944, -0.33672602},
                                                          {0.33118558, -0.41368897, 0.11050732, 0.63841363, 0.47108684},
                                                          {-0.75739837, -0.28169628, 0.4571613, 0.2190336, -0.47012922},
                                                          {-1.17197907, -0.58405251, 0.22260131, 0.12900957, 0.09351878},
                                                          {-0.248071, 0.66367222, 0.10831547, -0.16280054, 0.06920139},
                                                          {1.40440067, -0.23237915, -0.92723858, -0.53774185, -0.71267686},
                                                          {0.21367013, -0.59068412, 0.80310772, -0.7207505, -0.42473666},
                                                          {1.06176005, -0.43954177, 0.89927976, -0.25400912, 0.57200242}};

float pesosParaSaida[tamanhoCamada4][tamanhoSaidaRede] = {{1.77166766, -1.36502699, -0.9602132},
                                                          {0.23083842, 0.97734312, 0.55093142},
                                                          {0.25140053, 1.33555441, -0.52414012},
                                                          {0.12605296, -0.4081541, 0.24800671},
                                                          {0.41534795, 0.47292666, 0.66045206}};
/*
 * Fim definição dos parâmetros da rede
 */

/*
 * Funções utilizadas pela rede
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

//função que faz a forward propagation de uma camada escondida ou de entrada
//para uma camada escondida
int propagarEntradaParaProximaCamada5(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float pesos[][5], float biasRede[])
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
    // printf("%f ", saida[i]);
  }
  //printf("\n");

  return 0;
}

//função que faz a forward propagation de uma camada escondida ou de entrada
//para uma camada escondida
int propagarEntradaParaProximaCamada8(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float pesos[][8], float biasRede[])
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
    // printf("%f ", saida[i]);
  }
  //printf("\n");

  return 0;
}

//função que faz a forward propagation da última camada escondida para a saída
int propagarEntradaParaSaida(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float pesos[][tamanhoSaidaRede], float biasRede[])
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
    //printf("%f ", saida[i])
  }

  return softMax(saida, tamanhoSaida);
}

/*
 * Fim definição de funções utilizadas pela rede
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

//callback for GET /
//send all values
void handle_alldata(Request &req, Response &res)
{
  int tamanhoVetorEnvio = (6 * ADC_SAMPLES) + 2 + (12 * ADC_SAMPLES) + 8 + 1;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;

  //append adc samples vector (in counts)
  for (int i = 0; i < ADC_SAMPLES; i++)
  {
    if (i < (ADC_SAMPLES - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 6, "%d,", buffer_adc_data[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 5, "%d", buffer_adc_data[i]);

    posicaoPonteiro += retorno;
  }
  //add separator between vectors
  retorno = snprintf(&envio[posicaoPonteiro], 3, "%s", "!!");
  posicaoPonteiro += retorno;

  //append sample time vector (in us)
  for (int i = 0; i < ADC_SAMPLES; i++)
  {
    //append sample timestamp referenced to the first sample
    if (i < (ADC_SAMPLES - 1))
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

//callback for GET /i_rms_data
//send i_rms vector and last value index
void handle_i_rms_data(Request &req, Response &res)
{
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
    for (int i = 0; i < i_rms_data_idx; i++)
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
void handle_i_rms(Request &req, Response &res)
{
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
void handle_adc_data(Request &req, Response &res)
{
  int min = int(ADC_SAMPLES / 2) - int((1 / (timer0_t * 60 * .000001)) / 2);
  int max = min + int(1 / (timer0_t * 60 * .000001));

  int tamanhoVetorEnvio = (6 * (max - min)) + 2 + ((12 * (max - min)) + 10 + 1);
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;

  //append adc samples vector (in counts)
  for (int i = min; i < max; i++)
  {
    if (i < (max - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 6, "%d,", buffer_adc_data[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 5, "%d", buffer_adc_data[i]);

    posicaoPonteiro += retorno;
  }
  //add separator between vectors
  retorno = snprintf(&envio[posicaoPonteiro], 3, "%s", "!!");
  posicaoPonteiro += retorno;

  //append sample time vector (in us)
  for (int i = min; i < max; i++)
  {
    if (i < (max - 1))
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

//callback for GET /events
//send all event log
void handle_events(Request &req, Response &res)
{
  int tamanhoVetorEnvio = ((2 + 1) * tamanhoVetorEventosDetectados) + 1 + ((tamanhotimestamp + 1) * tamanhoVetorEventosDetectados) + 1 + 5;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;
  //insere vetor de classes de eventos
  for (int i = 0; i < tamanhoVetorEventosDetectados; i++)
  {
    if (i < (tamanhoVetorEventosDetectados - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 3, "%d,", vetorEventosDetectados[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 3, "%d ", vetorEventosDetectados[i]);

    posicaoPonteiro += retorno;
  }
  //insere vetor de timestamps de eventos em EPOCH
  for (int i = 0; i < tamanhoVetorEventosDetectados; i++)
  {
    if (i < (tamanhoVetorEventosDetectados - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 15, "%d,", tempoVetorEventosDetectados[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 15, "%d ", tempoVetorEventosDetectados[i]);

    posicaoPonteiro += retorno;
  }

  retorno = snprintf(&envio[posicaoPonteiro], 4, "%d", vetorEventosDetectadosIdx);
  //send string to http client as text/plain
  res.set("Content-Type", "text/plain");
  res.status(200);
  res.print(envio);
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
    if (wifiretries > 10)
      ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  oldfreeheap = ESP.getFreeHeap();

  //webserver app routing
  app.get("/", &handle_alldata);
  app.get("/freeheap", &handle_freeheap);
  app.get("/i_rms_data", &handle_i_rms_data);
  app.get("/i_rms", &handle_i_rms);
  app.get("/adc_data", &handle_adc_data);
  app.get("/events", &handle_events);
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
      //print sampling results
      //Serial.print("Foram lidas ");
      //Serial.print(isrCount);
      //Serial.println(" amostras.");
      //Serial.print("i_rms = ");
      i_rms = calcularRms(buffer_adc_data);

      //Serial.print(i_rms, 3);
      //Serial.println(" A");
      //i_rms last values vector handling
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
          entradaRede[i] = entradaRede[i + 1];
        }
        else
        {
          entradaRede[i] = i_rms_data[i_rms_data_idx];
        }
      }
      //run classifier after tamanhoJanela rms samples
      if (rmsCalculados >= tamanhoJanela)
      {
        propagarEntradaParaProximaCamada5(entradaRede, tamanhoJanela, camada1, tamanhoCamada1, pesosParaCamada1, biasEntrada);
        propagarEntradaParaProximaCamada8(camada1, tamanhoCamada1, camada2, tamanhoCamada2, pesosParaCamada2, biasCamada1);
        propagarEntradaParaProximaCamada8(camada2, tamanhoCamada2, camada3, tamanhoCamada3, pesosParaCamada3, biasCamada2);
        propagarEntradaParaProximaCamada5(camada3, tamanhoCamada3, camada4, tamanhoCamada4, pesosParaCamada4, biasCamada3);
        resultadoclassificador = propagarEntradaParaSaida(camada4, tamanhoCamada4, saidaRede, tamanhoSaidaRede, pesosParaSaida, biasCamada4);
        //só registra nos eventos as classes diferentes de zero
        if (resultadoclassificador > 0)
        {
          vetorEventosDetectados[vetorEventosDetectadosIdx] = resultadoclassificador;
          struct tm timeinfo;
          if (!getLocalTime(&timeinfo))
            tempoVetorEventosDetectados[vetorEventosDetectadosIdx] = 0;
          else
            tempoVetorEventosDetectados[vetorEventosDetectadosIdx] = mktime(&timeinfo);
          vetorEventosDetectadosIdx++;
        }

        //garante que os neurônios são inicializados antes de uma nova classificação
        for (int i = 0; i < tamanhoCamada1; i++)
        {
          camada1[i] = 0;
        }
        for (int i = 0; i < tamanhoCamada2; i++)
        {
          camada2[i] = 0;
        }
        for (int i = 0; i < tamanhoCamada3; i++)
        {
          camada3[i] = 0;
        }
        for (int i = 0; i < tamanhoCamada4; i++)
        {
          camada4[i] = 0;
        }
        for (int i = 0; i < tamanhoSaidaRede; i++)
        {
          saidaRede[i] = 0;
        }
      }
        else rmsCalculados++;
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
