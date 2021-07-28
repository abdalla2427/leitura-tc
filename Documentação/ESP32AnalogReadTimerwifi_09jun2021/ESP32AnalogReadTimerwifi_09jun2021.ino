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
#include <ArduinoJson.h>


//const char* ssid     = "GVT-596D";
//const char* password = "laion123";
const char* ssid = "VIVOFIBRA-8CDC";
const char* password = "EAEA7A8CDC";
//const char* ssid     = "iPhone de lucas";
//const char* password = "carine123";

//const char* ssid     = "TEE_420A";
//const char* password = "#L@BTEE#";

//const char *ssid = "cafofo2";
//const char *password = "0123456789012";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; //-3h GMT
const int daylightOffset_sec = 0;  //3600s if daylight saving time
const size_t CAMADAS = JSON_ARRAY_SIZE(2);
const size_t ENTRADAS_BIAS = JSON_ARRAY_SIZE(3);
const size_t JSON_OB_CAP = JSON_OBJECT_SIZE(100);

StaticJsonDocument<JSON_OB_CAP> doc; 
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

float biasEntrada[tamanhoCamada1] = { 1.56473463, -2.41900121, -0.71409326, -0.86229673,  0.38022143};
float biasCamada1[tamanhoCamada2] = { 0.58236053, -2.05713332, -0.24978616, -0.56665858, -0.2459753,-0.38273451,  2.44016488,  0.54417541};
float biasCamada2[tamanhoCamada3] = { 0.3217876 , -0.52095973, -0.7227467 , -0.48708937, -0.33317859,-0.07583713, -0.22334892, -1.56304241};
float biasCamada3[tamanhoCamada4] = {-0.63054077,  0.17083555, -0.65796861,  0.35497908, -0.56029508};
float biasCamada4[tamanhoSaidaRede] = { 0.33743533, -4.86697777, 4.58513054};

//pesosParaCamdaX[NohOrigem][NohDestino]
float pesosParaCamada1[tamanhoJanela][tamanhoCamada1] = {{-0.92620594,  1.01562079, -0.77379216, -2.2714845 , -0.72407906},
       {-1.69888604,  0.02079684, -0.23906248, -2.798543  , -0.20891169},
       {-0.1817855 , -0.00680469, -0.45748971, -0.10939818, -0.93311538},
       { 0.06559673, -0.00751158,  0.09084824, -0.03260378, -0.73069978},
       { 2.19092162,  2.1987533 , -0.28880788,  2.56751848, -0.02454715}};

float *teste = &pesosParaCamada1[0][0];

float pesosParaCamada2[tamanhoCamada1][tamanhoCamada2] = {{-0.58165555, -0.32874991,  1.02053924,  1.23754336, -0.48618627,        
        -1.55563246,  1.33949388,  1.76026702},     
       {-0.57410005,  0.99924976,  0.69339302,  0.20263727, -0.67853892,
         0.69275031, -1.16808397,  0.30713057},     
       { 0.55472225, -0.28019599, -0.28812295, -0.50228497, -0.6525227 ,
         0.24279281, -0.39150284, -0.31830131},     
       {-0.01147117, -1.0053759 ,  0.5381578 , -0.96510053,  0.1158694 ,
         1.12802715, -2.08879119, -1.74391513},     
       { 0.27031404, -0.14256809, -0.56552498, -0.00870312,  0.18379136,
         0.05890488,  0.41171845,  0.14061717}};

float pesosParaCamada3[tamanhoCamada2][tamanhoCamada3] = {{ 0.32205287,  0.26704306,  0.46907189,  0.1513442 ,  0.32550585,
        -0.00847013, -0.26782039,  0.48446685},     
       {-0.96319019,  0.56884929, -0.55806996,  0.14140462, -0.47145568,
         2.1888254 ,  0.59354789,  0.15580696},     
       { 0.33312346, -0.34630749,  1.39014693,  0.03529309, -0.68678346,
         0.27161496,  0.2474282 ,  0.73469791},     
       { 1.24561535, -0.19949282,  1.03855829,  0.12101474, -0.63150929,
         0.16985733,  0.26185992, -0.80840438},     
       {-0.38010276, -0.44405554,  0.45572587,  0.24085822, -0.53110755,
         0.19784653,  0.28929097,  0.51767536},     
       {-0.68273969, -0.45979945, -1.20618854, -0.58302714, -0.57723256,
         0.41417199,  1.0072964 ,  1.58317993},     
       {-0.35863299,  0.24245244, -1.84819275, -0.27033584,  0.10908209,
         2.35163947,  0.27009599, -0.61970653},     
       { 0.80301257, -0.40702173,  1.68166481, -0.15898675,  0.34381733,
         1.26716527,  0.08410271, -1.34704735}};

float pesosParaCamada4[tamanhoCamada3][tamanhoCamada4] = {{ 0.28336933,  0.22631087, -0.3610096 , -0.59634947, -0.33627689},
       { 0.33425516, -0.40239042,  0.11045677,  0.63811408,  0.47086665},
       {-3.25513739, -0.5225701 , -0.35081541,  0.19016811, -0.46941526},
       {-0.64678997, -0.58668935, -0.01853831,  0.14435626,  0.0934749 },
       {-0.33974474,  0.64418979,  0.10826465, -0.16272415,  0.06916892},
       { 0.64053085,  2.23584397, -1.19202   , -0.64495404, -1.12660483},
       { 0.40715633, -0.00543783, -0.18775376, -0.59229383, -0.49741797},
       { 1.08569654, -1.65224705, -0.02805037,  0.0334932 ,  0.57664293}};

float pesosParaSaida[tamanhoCamada4][tamanhoSaidaRede] = {{ 1.31293312, -3.22789441,  1.36164851},
       { 0.66059888,  0.94020811,  0.15748059},     
       { 0.66992455,  0.25933025,  0.13306134},     
       {-0.15978986, -0.04171538,  0.16742682},     
       { 0.68847162,  0.25010245,  0.60942594}};
       
float *listaBias[dimensaoCamadasRede]={biasEntrada, biasCamada1, biasCamada2, biasCamada3, biasCamada4};
float topologiaRede[dimensaoCamadasRede] = {tamanhoCamada1, tamanhoCamada2, tamanhoCamada3, tamanhoCamada4, tamanhoSaidaRede};
float *listaPesos[dimensaoCamadasRede]={*pesosParaCamada1, *pesosParaCamada2, *pesosParaCamada3, *pesosParaCamada4, *pesosParaSaida};


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

//teste fun


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

//callback for POST /handle_update_weights
//update classifier weigths
void handle_update_weights(Request &req, Response &res)
{
  res.set("Content-Type", "application/json");
  String streamTeste = req.readString();

  deserializeJson(doc, streamTeste);
  JsonObject object = doc.as<JsonObject>();
  
  for(int camadaAtual = 0; camadaAtual < dimensaoCamadasRede; camadaAtual++) {
    int dimensaoCamadaAtual = topologiaRede[camadaAtual];
    JsonArray biasCamadaAtual = object["bias"][camadaAtual].as<JsonArray>();
    copyArray(biasCamadaAtual, listaBias[camadaAtual], dimensaoCamadaAtual);
  }

  res.status(200);
}

       
//float *listaBias[dimensaoCamadasRede]={biasEntrada, biasCamada1, biasCamada2, biasCamada3, biasCamada4};
//float topologiaRede[dimensaoCamadasRede] = {tamanhoCamada1, tamanhoCamada2, tamanhoCamada3, tamanhoCamada4, tamanhoSaidaRede};
//float *listaPesos[dimensaoCamadasRede]={pesosParaCamada1, pesosParaCamada2, pesosParaCamada3, pesosParaCamada4, pesosParaSaida};


//callback for GET /handle_current_weights
//get classifier weigths
void handle_get_weights(Request &req, Response &res)
{
  Serial.println(listaPesos[0][0]);
  res.set("Content-Type", "application/json");
//  for(int camadaAtual = 0; camadaAtual < dimensaoCamadasRede; camadaAtual++) {
//    int dimensaoCamadaAtual = topologiaRede[camadaAtual]; 
//    for(int j= 0; j < dimensaoCamadaAtual ; j++) {
//      Serial.println(listaBias[camadaAtual][j]);
//    }
//  }

Serial.println("comecou");
//    for(int camadaAtual = 0; camadaAtual < dimensaoCamadasRede; camadaAtual++) {
//    if (camadaAtual < (dimensaoCamadasRede + 1)) {
      int camadaAtual = 0;
      int dimensaoCamadaAtual = 5;
      int dimensaoProximaCamada = topologiaRede[0];
  
      for (int noDe = 0; noDe < dimensaoCamadaAtual; noDe++) {
        for (int noPara = 0; noPara < dimensaoProximaCamada; noPara++) {
             Serial.print(*(teste + (dimensaoCamadaAtual * noDe) + noPara), 3);
             Serial.print(",");
        }
        Serial.println("");
      }
//    }
//  }
  Serial.println("cabou");
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
  app.post("/AtualizarClassificador", &handle_update_weights);
  app.get("/PesosAtuais", &handle_get_weights);
  
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
