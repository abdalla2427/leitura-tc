// in microseconds; should give 0.5Hz toggles

/*
Sketch para leitura de sensor de corrente SCT013 em valor rms, usando ESP32
UFF - PIBIC - 2019-2020
Lucas Abdalla Menezes e Rainer Zanghi
*/

//TODO: incluir vetor para armazenar últimos valores rms lidos - qual periodicidade?
#include <stdint.h>

#define ADC_SAMPLES 512
#define I_RMS_VEC_SIZE 256
#define TEMPO_TIMER_2 100
#define TEMPO_TIMER_3 500000
#define ANALOG_PORT 34
#define TAMANHO_VETOR_ENVIO 7 * I_RMS_VEC_SIZE + 5

float i_rms;
float i_rms_data[I_RMS_VEC_SIZE] = {0};
uint32_t i_rms_data_idx = 0;
uint32_t isrCounter = 0;
uint32_t adc_data[ADC_SAMPLES] = {0};
uint32_t sampletime_us[ADC_SAMPLES] = {0};
uint32_t buffer_adc_data[ADC_SAMPLES] = {0};
uint32_t buffer_sampletime_us[ADC_SAMPLES] = {0};
char envio[TAMANHO_VETOR_ENVIO] = {'\0'};
int posicaoPonteiro = 0;

void funcaoTimer2(void);
void funcaoTimer3(void);

//timer1 callback - ADC sampling timer
void funcaoTimer2()
{
  //Impede o armazenamento fora do vetor.
  if (isrCounter < ADC_SAMPLES)
  {
    // Read analog sample, save sample time and increment ISR sample counter
    adc_data[isrCounter] = analogRead(ANALOG_PORT);
    sampletime_us[isrCounter] = micros();
    //adjust sample's timestamp referenced to the first sample
    if (isrCounter)
      sampletime_us[isrCounter] = sampletime_us[isrCounter] - sampletime_us[0];
    if (isrCounter == (ADC_SAMPLES - 1))
      sampletime_us[0] = 0;
  }
  isrCounter++;
}

//timer2 callback - restart ADC sampling timer for a new rms calculation
void funcaoTimer3()
{
  Timer2.pause();
  Timer2.refresh();
  Timer2.resume();
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
  int min = int(ADC_SAMPLES / 2) - int((1 / (TEMPO_TIMER_2 * 60 * .000001)) / 2);
  int max = min + int(1 / (TEMPO_TIMER_2 * 60 * .000001));
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

void criaVetorParaEnvio()
{
  //append rms current vector (in A)
  //arranging the cyclic vector in chronological order
  int retorno;
  for (int i = i_rms_data_idx; i < I_RMS_VEC_SIZE; i++)
  {
    if (i != (i_rms_data_idx - 1))
      retorno = snprintf(&envio[posicaoPonteiro], 8, "%.3f,", i_rms_data[i]);
    else
      retorno = snprintf(&envio[posicaoPonteiro], 8, "%.3f", i_rms_data[i]);

    posicaoPonteiro += retorno;
  }
  if (i_rms_data_idx)
    for (int i = 0; i < i_rms_data_idx; i++)
    {
      if (i != (i_rms_data_idx - 1))
        retorno = snprintf(&envio[posicaoPonteiro], 8, "%.3f,", i_rms_data[i]);
      else
        retorno = snprintf(&envio[posicaoPonteiro], 8, "%.3f", i_rms_data[i]);

      posicaoPonteiro += retorno;
    }
  snprintf(&envio[posicaoPonteiro], 5, " %d", i_rms_data_idx);
}

void setup()
{
  Serial.begin(115200);

  Timer2.setPeriod(TEMPO_TIMER_2);
  Timer2.attachInterrupt(TIMER_CH1, funcaoTimer2);

  Timer3.setPeriod(TEMPO_TIMER_3);
  Timer3.attachInterrupt(TIMER_CH2, funcaoTimer3);
}

void loop()
{
  // If all ADC_SAMPLES values were sampled
  if (isrCounter > ADC_SAMPLES - 1)
  {
    // If timer is still running
    // Stop and keep timer
    Timer2.pause();
    //ensures no reading/writing to variables used by ISR
    isrCounter = 0;
    for (int i = 0; i < ADC_SAMPLES; i++)
    {
      buffer_adc_data[i] = adc_data[i];
      buffer_sampletime_us[i] = sampletime_us[i];
    }

    i_rms = calcularRms(buffer_adc_data);

    i_rms_data[i_rms_data_idx] = i_rms;

    if ((i_rms <= 0.0) || (i_rms > 99.999))
      if ((i_rms_data_idx) == 0) //first element
        i_rms_data[i_rms_data_idx] = 0.001;
      else //not first element
        i_rms_data[i_rms_data_idx] = i_rms_data[i_rms_data_idx - 1];
    i_rms_data_idx++;
    if (i_rms_data_idx >= I_RMS_VEC_SIZE)
      i_rms_data_idx = 0;
  }
  criaVetorParaEnvio();
  int bytesSent = Serial.write(envio);
}
