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
#define tamanhoJanela 5
#define tamanhoSaidaRede 3
#define tamanhoCamadaInterna 5
#define debug 1
#define tamanhoVetorEventosDetectados 256
#define tamanhotimestamp 3+3+4+1+3+3+3 //"%d/%m/%Y-%H:%M:%S"

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
volatile uint32_t vetorEventosDetectadosIdx = 0;
volatile uint32_t vetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
volatile uint32_t tempoVetorEventosDetectados[tamanhoVetorEventosDetectados] = {0};
/*
 * Parâmetros da rede*
*/
float entradaRede[tamanhoJanela] = { 0 };
float camada1[tamanhoCamadaInterna] = { 0 };
float camada2[tamanhoCamadaInterna] = { 0 };
float saidaRede[tamanhoSaidaRede] = { 0 };
int rmsCalculados = 0;

float biasEntrada[tamanhoCamadaInterna] = { 0.5221433 , -0.28974283, -0.71409326, -0.53784864,  3.02889286 };
float biasCamada1[tamanhoCamadaInterna] = {-0.69192777,  1.06735616, -1.0011572 , -0.64673044, -1.91282453};
float biasCamada2[tamanhoSaidaRede] = {-3.19054419,  2.73373855,  0.35397143};

int numeroDeNeuroniosDeSaida;

//pesosParaCamdaX[NohOrigem][NohDestino]
float pesosParaCamada1[5][5] = {{-5.23753048, -1.55944794, -0.764496715,-0.321165609, -3.33818016},
       { 2.17744233, -1.10460068, -0.236190662, -0.176716655, -2.31777594},
       { 0.327893887,  0.00161291773, -0.451993962, 0.526103751,  0.559806677},
       {-4.45965651,  0.322068420,  0.0897568940, -0.601905498,  3.05674680},
       { 7.40254218,  2.71763668, -0.285338477, 0.219689785,  2.40873829}};

float pesosParaCamada2[5][5] = {{-0.61426557, -9.69862923, -0.67327078, -0.11079759, -0.38204163},
       {-0.28213993,  1.32910803,  0.32210186, -0.77492539, -0.35273337},
       { 0.74763641,  0.37953047, -0.3357765 ,  0.44240739, -0.60680364},
       {-0.07968868,  0.62307881, -0.31713313, -0.32456436, -0.56409033},
       {-0.73505291,  2.89521433, -1.74906174, -0.37461758, -0.04567016}};

float pesosParaCamada3[5][3] = {{-0.67995156, -0.14695204,  0.33239662},
       { 2.47090284, -3.83612717,  0.51034527},
       { 0.37085337,  0.41388947,  0.28097476},
       { 0.22258334,  0.5844932 , -0.58918653},
       { 0.58776962, -0.40017705, -0.45374138}};

float vetorEntradas[100][5] = {{0.7229986115623536, 0.19509200436247787, 0.7849556681279058, 0.28689232605514414, 0.26658855440176077}, {0.043698219159143736, 0.9361554925428931, 0.24846675337542068, 0.18447383141912288, 0.8904684998812215}, {0.8522545598720415, 0.01130143720816823, 0.5942671351774969, 0.013278580607276935, 0.1700987125671105}, {0.05164512557475154, 0.8329478660400417, 0.2827660257009533, 0.4459430088078461, 0.8819368071099962}, {0.22814936021458865, 0.7472829208139924, 0.1417284767488881, 0.4947327665460389, 0.8385423990339614}, {0.6390809642165746, 0.29583168752015965, 0.020686114347692963, 0.33475086091997197, 
0.8869762206301628}, {0.3197529637295854, 0.041364338793043354, 0.4811449898884489, 0.15923588838869607, 0.6738587415578814}, {0.07592023113780066, 0.47442054659073485, 0.4841024916621751, 0.27092442838117725, 0.4497546038931184}, {0.5314966528872712, 0.12579368144106362, 0.12158425433841102, 0.9494606032407407, 0.9741726427295516}, {0.42780041292712456, 0.044244120485722016, 0.9244765424622541, 0.6455439816863603, 0.4577173978346182}, {0.601363662048213, 0.10721959367980449, 0.6287706100644012, 0.32740806744231443, 0.28729493956133867}, {0.8165605520946445, 0.6657660588776074, 0.25352099921177673, 0.1316478168277354, 0.18890251079585518}, {0.09624151249314117, 0.41082172831835195, 0.8842401548079517, 0.5108660137460704, 0.8768370642517984}, {0.7266785754143681, 0.9001210722829299, 0.19802136001437853, 0.7801475141869488, 0.8136131462669184}, {0.20227451472147995, 0.9104900158550364, 0.7083585838128194, 0.9535532240421111, 0.32947544742382817}, {0.08260519030312297, 0.192602130838883, 0.34072534308755087, 0.19326191361026235, 0.6631122280452677}, {0.5963660304014429, 0.3786220796611779, 0.7120194110269549, 0.32693424795782655, 0.2747029360108012}, {0.31826732545961345, 0.08581852991698313, 0.1732902054258646, 0.29618741714580765, 0.5195157159112704}, {0.734776956486585, 0.00980064848701978, 0.3485734438313194, 0.3056734443902923, 0.20395336390263985}, {0.0003288397502297924, 0.5292149590849303, 0.5500315631779329, 0.12093032318123231, 0.7619321164820289}, {0.5114806255919389, 0.7244226062965278, 0.5637761520052962, 0.18149685144034522, 0.4271147825146717}, {0.344791429048949, 0.10380451641480226, 0.27857306216181255, 0.8767264260713948, 0.8782527210170734}, {0.16512611894490115, 0.020845914947697053, 0.17453257091978647, 0.39348658688294236, 0.6123102190276549}, {0.5328191437367358, 0.4989759425222976, 0.7894303674763073, 0.4246400131281378, 0.6973402353323148}, {0.6880364842935585, 0.08943258553432043, 0.0924657013191118, 0.5573654993965682, 0.7698033176422675}, {0.5456114473021266, 0.423110968644852, 0.9332421927766191, 0.3098023050381924, 0.7714732814830058}, {0.9972500136894974, 0.6861951657798295, 0.6648274842305769, 0.4755342956979258, 0.9012922293176373}, {0.37110665328879544, 0.01831262662123445, 0.7974024726193684, 0.18852936916189655, 0.5249517144334176}, {0.9006999143833088, 0.09225868834318252, 0.9007935564787328, 0.8477895952685686, 0.9274823307868338}, {0.34245810984653524, 0.20269507549270582, 0.5018637687424986, 0.28346070761216935, 0.017980464409224406}, {0.9187180737313579, 0.23550066036355133, 0.7584776664906967, 0.4206949482001152, 0.8621404916419314}, {0.3805503157382878, 0.23811184843871214, 0.0016085952910193102, 0.05743613247114632, 0.7883725095626586}, {0.26124716948343996, 0.7604232397687847, 0.1580724854182206, 0.4418481142673317, 0.3916632259679276}, {0.5539115492318624, 0.49005416143458147, 0.12212331903277196, 0.01079584184833049, 0.3097063895072424}, {0.5376876414227438, 0.9259637796071668, 0.6745782978609909, 0.9141927147190524, 0.6306150932098787}, {0.7662652075233148, 0.43372519775311813, 0.5556277360408403, 0.0009332413910201343, 0.027647383420100757}, {0.3721079605791088, 0.5435562862225712, 0.9245740836114428, 0.38846533605229616, 0.9110273268148348}, {0.663528497312518, 0.5712854472425416, 0.7804171700806865, 0.7471139589703745, 0.13444051758568298}, {0.8248013852391135, 0.4586860350935005, 0.26369503462603305, 0.17749058058249378, 0.03636338906794512}, {0.6591857917223642, 0.8324530519000009, 0.08822659830364332, 0.5424756924438496, 0.17555832609536093}, {0.5149428996593488, 0.442868163593808, 0.6567106081109215, 0.5771599750454011, 0.5686074608794264}, {0.7951415770289763, 0.3294629999563494, 0.6436621472011128, 0.4901220856869669, 0.262259308774729}, {0.27896705667915767, 0.11530465625177333, 0.3064356091894498, 0.7270313569229562, 0.09026296132855516}, {0.7274661962126978, 0.9366262262680991, 0.7288735478676448, 0.9740158243202394, 0.9085808764892913}, {0.6579987664205769, 0.24779092900595356, 0.23547295428321824, 0.35137487389438493, 0.7825105406509792}, {0.043692090786437654, 0.5266022794889291, 0.07917499579731613, 0.3623711978544466, 0.16497667248820247}, {0.473562523577181, 0.1916738014164503, 0.9244946118011862, 0.08803174444545447, 0.021315595941262977}, {0.6746265673608502, 0.5235239943502369, 0.8428493596495045, 0.23384428519267952, 0.3578619080821499}, {0.25010611033783814, 0.329062837288293, 0.6612994016039776, 0.0881159774372281, 0.9608248309244568}, {0.2284908799018215, 0.284873903764033, 0.9691302053178996, 0.7829077101266767, 0.6914220741475504}, {0.265702833489047, 0.06009787706590619, 0.6151270730831286, 0.39464917975662495, 0.042348333319330855}, {0.0981774073605497, 0.27051255814327335, 0.9977466031460803, 0.8599890865904374, 0.9891734959568468}, {0.11926143226456531, 0.20521088898978956, 0.23471876591883423, 0.24097952596168304, 0.737678090622041}, {0.0029097174291503602, 0.030586316792037938, 0.01972691848226138, 0.6352014803078572, 0.3749968430120715}, {0.8882149514283516, 0.6117895503331682, 0.8376233855341116, 0.7912960327300141, 0.4018032024626622}, {0.30928798244621836, 0.886041677245704, 0.5861468665254403, 0.8716188427625186, 0.8536817131566631}, {0.9723812150685279, 0.29851536210067575, 0.49055808707416404, 0.06078273425169711, 0.44548591258254555}, {0.5651310194871555, 0.2854512164269922, 0.5366643331482368, 0.8914729310639325, 0.9619970990447758}, {0.1628251794487302, 0.3508037072988307, 0.4257811488141233, 0.5589859492981033, 0.44660654135266664}, {0.3641297061254134, 0.6485146771673461, 0.07291885653842434, 0.1562457816013212, 0.19700645030880426}, {0.9565599792235693, 0.6403974936637516, 0.0897265005348753, 0.5100214565428908, 0.20932942305176772}, {0.2726039995608869, 0.40282764748295796, 0.6463328290689874, 0.2723624348291702, 0.7817899243794741}, {0.5003186632410821, 0.6400861625703486, 0.6019517852497119, 0.45556593830224845, 0.44996215736010703}, {0.2552202465634923, 0.10638681365887293, 0.34103976250855017, 0.009110052897442933, 0.046453713338720726}, {0.6480631499841739, 0.5045250579687436, 0.35380669409355814, 0.4133949568646783, 0.25704003256326724}, {0.3554279049064145, 0.7616151515828835, 0.3554521394695537, 0.23551003290704042, 
0.6958487990696357}, {0.6807853015206765, 0.8261801683088061, 0.9176599564044196, 0.13526575860468149, 0.13210620121799588}, {0.8065004704546004, 0.5442530333939728, 0.861664388489058, 0.9274237875619499, 0.7018163121697717}, {0.04715734482205591, 0.795747239703703, 0.9462231834910981, 0.8336295124539924, 0.49984899612715117}, {0.4011462638755654, 0.2470450600417361, 0.3167895812459187, 0.6365723418460438, 0.874846574053312}, {0.8376629951088317, 0.292555915316141, 0.19614805062810925, 0.6853949290886086, 0.9543378788529199}, 
{0.93340750393128, 0.42066168319966724, 0.4808877565884675, 0.2538562403384279, 0.1307733368203382}, {0.3685988129949901, 0.3448631730693428, 0.053620266086638835, 0.3793481997963971, 0.5582546039608292}, {0.9055637003083697, 0.996521288061948, 0.686892315680354, 0.2423705632789397, 0.3973474813788911}, {0.5665077797141588, 0.7678164194756638, 0.07812137248112239, 0.03597452601049911, 0.9090952785786335}, {0.7446723667634711, 0.30734814263186083, 0.76687769201766, 0.9507691761532717, 0.08146791880668858}, {0.4405196083391757, 
0.594891194284507, 0.5512000550052608, 0.3609862517046244, 0.623108073836846}, {0.2302014336037823, 0.3180619928898132, 0.789709933126555, 0.7681352095544275, 0.3117575531717216}, {0.9938584456494923, 0.40219103876036955, 0.6171152411094081, 0.49152949283126, 0.7991683347402624}, {0.1779282414620338, 0.23815149757822285, 0.301569780077101, 0.3273938772238322, 0.23398997781465303}, {0.018120730351668057, 0.8338266353556968, 0.6063279529861425, 0.7745194468965981, 0.3573618825154419}, {0.9003041066568912, 0.06888073075620327, 0.8708850736180216, 0.9980112052847634, 0.11670734632275104}, {0.6343182552385898, 0.5502679241840801, 0.40121661854275736, 0.7236407259105282, 0.7839863977182655}, {0.0753559626429755, 0.2769229590397687, 0.07970938900679791, 0.8556176983826719, 0.34811988422870077}, {0.6133701295586683, 0.7588771391092083, 0.056744121416272275, 0.7513593086379684, 0.09760573604349398}, {0.2201680995799733, 0.2614895625996657, 0.7389766047753202, 0.2977793997419582, 0.8824930964389649}, {0.788439348002537, 0.9311167246392605, 0.020884121675628142, 0.6031908121987365, 0.1553226883914295}, {0.7424614996502331, 0.39814430236268994, 0.404703924452369, 0.6346575274291303, 0.7602144072202227}, {0.9272732596343047, 0.0916199528756918, 0.5841864787477898, 0.8955537240510559, 
0.34970416382835734}, {0.9049616349079552, 0.8625427049740657, 0.8174044745723085, 0.8672480170398201, 0.463692653220273}, {0.1760882979950804, 0.7926054013010421, 0.8949636953037662, 0.6477844837100113, 0.06796421999932578}, {0.6335531511946588, 0.657060378417579, 0.29788103786548703, 0.9647957799892876, 0.33240232470798514}, {0.6898463545824535, 0.3587854247863732, 0.8571027029362112, 0.5843782973975831, 0.7232632810904829}, {0.35976253015412574, 0.022195650908530262, 0.7284854817010207, 0.6281754421433792, 0.8047792305517406}, {0.30999360506946094, 0.2770226702454224, 0.7507708093323664, 0.47804573370203673, 0.6912543728074184}, {0.6158073646634495, 0.29069746909278926, 0.6155299220902719, 0.5190458748564261, 0.7051396315258941}, {0.8856250559781482, 0.02922403018004971, 0.308929475790356, 0.47536727307249727, 0.6982927395632439}, {0.7511662485344881, 0.35145716740896227, 0.15793591029058096, 0.5382784723708949, 0.37766385743902353}, {0.8743548494982648, 0.35980753890263184, 0.009033301045301978, 0.8943989749158915, 0.5621244115149279}, {0.07413549919409868, 0.30627399476756567, 0.03265390885685604, 0.683155292383063, 0.43322586124967166}};

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

float reluFunction(float valor) {
    return maximo(0, valor);
}

float logisticFunction(float valor) {
    if (valor >= 10)
        return 1;
    if (valor < -10)
        return 0;
    else {
        float resultado = (1 / (1 + expf(valor * (-1.0) ) ) );

        if (resultado >= 0.5)
            return 1.0;
        else
            return 0.0;
    }
}

//esta função retorna a classe com maior probabilidade
//após a normalização exponencial
int softMax(float *vetorZ, int tamanhoVetorZ) {
  float soma = 0;
	float max = 0;
	float saida[tamanhoVetorZ];
  int i;
	int saidaMax=0;

    for (i = 0; i < tamanhoVetorZ; i++) {
        soma += expf(vetorZ[i]);
    }

    for (i = 0; i < tamanhoVetorZ; i++) {
        saida[i] = (expf(vetorZ[i])) / (soma);
    }
	
	max = saida[0];
	
    for (i = 1; i < tamanhoVetorZ; i++) {
      if (saida[i] > max) {
        max = saida[i];
        saidaMax = i;
      }
    }

    return saidaMax;
}

//função que faz a forward propagation de uma camada escondida ou de entrada
//para uma camada escondida
int propagarEntradaParaProximaCamada(float *entrada, int tamanhoEntrada, float *saida, int tamanhoSaida, float pesos[][tamanhoCamadaInterna], float biasRede[])
{
    int i;
    int j;
    float auxEntrada;
    float auxPesos;
    for (i = 0; i < tamanhoEntrada; i++) {
        for (j = 0; j < tamanhoSaida; j++) {
            auxEntrada = entrada[i];
            auxPesos = pesos[i][j];
            saida[j] += auxEntrada * auxPesos;
        }
    }

    for (i = 0; i < tamanhoSaida; i++) {
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
    for (i = 0; i < tamanhoEntrada; i++) {
        for (j = 0; j < tamanhoSaida; j++) {
            auxEntrada = entrada[i];
            auxPesos = pesos[i][j];
            saida[j] += auxEntrada * auxPesos;
        }
    }

    for (i = 0; i < tamanhoSaida; i++) {
        saida[i] = saida[i] + biasRede[i];
        //printf("%f ", saida[i])
    }

    return softMax(saida, tamanhoSaida);
}

 /*
 * Fim definição de funções utilizadas pela rede
 */
  
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

//callback for GET /events
//send all event log
void handle_events(Request &req, Response &res){
  int tamanhoVetorEnvio = ((2+1) * tamanhoVetorEventosDetectados) + 1 + ((tamanhotimestamp+1) * tamanhoVetorEventosDetectados) + 1 + 5;
  char envio[tamanhoVetorEnvio] = {'\0'};
  int posicaoPonteiro = 0;
  int retorno = 0;
  //insere vetor de classes de eventos
  for (int i = 0 ; i < tamanhoVetorEventosDetectados ;i++) 
  {
    if (i<(tamanhoVetorEventosDetectados-1))
      retorno = snprintf(&envio[posicaoPonteiro], 3, "%d,", vetorEventosDetectados[i]);
    else 
      retorno = snprintf(&envio[posicaoPonteiro], 3, "%d ", vetorEventosDetectados[i]);
  
    posicaoPonteiro += retorno; 
  }
  //insere vetor de timestamps de eventos em EPOCH
  for (int i = 0 ; i < tamanhoVetorEventosDetectados ;i++) 
  {
    if (i<(tamanhoVetorEventosDetectados-1))
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

void loop() {
  uint32_t isrCount = 0;
  int resultadoclassificador=0;
  int tuplaBaseTeste = 0;
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
	  //create classifier input vector
      // for (int i = 0; i < tamanhoJanela; i++) {
      //   if ( i < (tamanhoJanela - 1)) {
      //     entradaRede[i] = entradaRede[i + 1];
      //   }
      //   else {
      //     entradaRede[i] = i_rms_data[i_rms_data_idx];
      //   }
      // }

      if (tuplaBaseTeste < 100) {
        for (int amostra = 0; amostra < tamanhoJanela; amostra++) {
            entradaRede[amostra] = vetorEntradas[tuplaBaseTeste][amostra];
        }
      } 
	  //run classifier after tamanhoJanela rms samples
      if (rmsCalculados >= 0) {
        propagarEntradaParaProximaCamada(entradaRede, tamanhoJanela, camada1, tamanhoCamadaInterna, pesosParaCamada1, biasEntrada);
        propagarEntradaParaProximaCamada(camada1, tamanhoCamadaInterna, camada2, tamanhoCamadaInterna, pesosParaCamada2, biasCamada1);
        resultadoclassificador = propagarEntradaParaSaida(camada2, tamanhoCamadaInterna, saidaRede, tamanhoSaidaRede, pesosParaCamada3, biasCamada2);
        tuplaBaseTeste++;
    	//só registra nos eventos as classes diferentes de zero
    	if (resultadoclassificador >= 0)
    	{
    		vetorEventosDetectados[vetorEventosDetectadosIdx] = resultadoclassificador;
    		struct tm timeinfo;
    		if(!getLocalTime(&timeinfo))
    		  tempoVetorEventosDetectados[vetorEventosDetectadosIdx]=0;
    		else
    		  tempoVetorEventosDetectados[vetorEventosDetectadosIdx]=mktime(&timeinfo);
    		vetorEventosDetectadosIdx++;
    	}
		//garante que os neurônios são inicializados antes de uma nova classificação
		for (int i = 0; i < tamanhoCamadaInterna; i++)
		{
			camada1[i] = 0;
			camada2[i] = 0;
		}
		for (int i = 0; i < tamanhoSaidaRede; i++)
			saidaRede[i] = 0;
      }
	  else
		rmsCalculados++;
      if (vetorEventosDetectadosIdx >= tamanhoVetorEventosDetectados) vetorEventosDetectadosIdx=0;
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
