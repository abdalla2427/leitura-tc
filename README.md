## Dados Brutos
A pasta possui todas amostras obtidas em 15 dias completos de captura. Os dados são acompanhados do momento em que a captura foi feita. Esses dados permitem maior flexibilidade para mudança entre paradigmas de classificação.

## Banco para treinamento
A pasta possui todas versões de bancos de treinamento e testes de um classificador. O classificador usado foi um MLP com 5 entradas (janela de 5 amostras sequenciais) e uma saída (0, 1 ou 2).

## Workflow para treinamento do classificador

* Um arquivo de dados brutos (ou varios) deve ser analisado e os eventos de ligamento e desligamento de cada carga devem ser inseridos na coluna "ClasseAparelho". Eventos de ligamento devem ser sinalizados com números ímpares, começando por 1, e os de desligamento do mesmo aparelho, como o seu sucessor. Ficando o valor 0 reservado como nenhum evento conhecido detectado.

* Após a identificação dos eventos das cargas monitoradas no arquivo, esses arquivos de dados brutos devem ser "janelados". Uma janela de tamanho fixo que avança uma amostra por vez, a cada 500 ms (intervalo entre amostras). Para visualização, considere o seguinte vetor de amostras 1.0 1.0 1.0 1.0 1.0 1.2 1.2 1.2 1.0. Considerando para demonstração uma janela de tamanho fixo de 3 amostras, o seu janelamento ocorre da seguinte forma.

[__1.0 1.0 1.0__ ]1.0 1.0 1.2 1.2 1.2 1.0

1.0 [__1.0 1.0 1.0__] 1.0 1.2 1.2 1.2 1.0

1.0 1.0 [__1.0 1.0 1.0__] 1.2 1.2 1.2 1.0

1.0 1.0 1.0 [__1.0 1.0 1.2__] 1.2 1.2 1.0

1.0 1.0 1.0 1.0 [__1.0 1.2 1.2__] 1.2 1.0

1.0 1.0 1.0 1.0 1.0 [__1.2 1.2 1.2__] 1.0

1.0 1.0 1.0 1.0 1.0 1.2 [__1.2 1.2 1.0__]

* A lista de janelas de eventos, para o exemplo dado ficaria no formato:

x1, x2, x3,

1.0 1.0 1.0

1.0 1.0 1.0

1.0 1.0 1.0

1.0 1.0 1.2

1.0 1.2 1.2

1.2 1.2 1.2

1.2 1.2 1.0

* Para cada janela criada, deve ser inserido qual tipo de evento ocorreu naquele instante. Se considerarmos que a mudanca do valor de 1.0 para 1.2 foi o ligamento de um aparelho conhecido, e de 1.2 para 1.0 foi o seu desligamento, a tabela ficará:

x1, x2, x3, ClasseAparelho

1.0 1.0 1.0, 0

1.0 1.0 1.0, 0

1.0 1.0 1.0, 0

1.0 1.0 1.2, 1

1.0 1.2 1.2, 1

1.2 1.2 1.2, 0

1.2 1.2 1.0, 2


* Após feito o janelamento de todos os arquivos de dados brutos com eventos identificados, esses foram usados para treinamento e teste do classificador MLP. Onde os valores x1,..., xn  (janela de tamanho n) são os valores de entrada do classificador, e ClasseAparelhoa resposta desejada pela rede para aqueles valores de entrada.

Os arquivos janelados para reconhecimento de um microondas encontram-se na pasta 'Banco para Treinamento".

## Configuração do Ambiente

Para fazer a configuração do ambiente precisaremos 1. iniciar uma API para consulta dos valores do ESP 32 e 2. inserir o codigo no ESP32 para captura e classificação.

### Confgiuração API

[Para configurar a API](https://github.com/abdalla2427/leitura-tc/tree/master/Documenta%C3%A7%C3%A3o/AndroidAPK/Solucao%20Web) deve-se baixar os conteudo contido na pasta, configurar os parametros do arquivo api-config.json e seguir o tutorial do link.

### Configuração ESP 32
[Para configurar o ESP 32](https://github.com/abdalla2427/leitura-tc/tree/master/Documenta%C3%A7%C3%A3o/ESP32AnalogReadTimerwifi_06ago2020) deve-se baixar o arquivo .ino da pasta e compilá-lo para o dispositivo por meio da IDE do Arduino.

Antes de compilar o código, deve-se alterar as credenciais para acesso à rede sem fio.

