## Dados Brutos
A pasta possui todas amostras obtidas em 15 dias completos de captura. Os dados são acompanhados do momento em que a captura foi feita. Esses dados permitem maior flexibilidade para mudança entre paradigmas de classificação.

## Banco para treinamento
A pasta possui todas versões de bancos de treinamento e testes de um classificador. O classificador usado foi um MLP com 5 entradas (janela de 5 amostras sequenciais) e uma saída (0, 1 ou 2).

## Workflow para treinamento do classificador

* Um arquivo de dados brutos (ou varios) deve ser analisado e os eventos de ligamento e desligamento de cada carga devem ser inseridos na coluna "ClasseAparelho". Eventos de ligamento devem ser sinalizados com números ímpares, começando por 1, e os de desligamento do mesmo aparelho, como o seu sucessor. Ficando o valor 0 reservado como nenhum evento conhecido detectado.

* Após a identificação dos eventos das cargas monitoradas no arquivo, esses arquivos de dados brutos devem ser "janelados". Uma janela de tamanho fixo que avança uma amostra por vez, a cada 500 ms (intervalo entre amostras). Para visualização, considere o seguinte vetor de amostras [1.0, 1.0, 1.0, 1.0, 1.0, 1.2, 1.2, 1.2, 1.0]. Considerando para demonstração uma janela de tamanho fixo de 3 amostras, o seu janelamento ocorre da seguinte forma.

[__1.0, 1.0, 1.0__, 1.0, 1.0, 1.2, 1.2, 1.2, 1.0]
[1.0,__ 1.0, 1.0, 1.0__, 1.0, 1.2, 1.2, 1.2, 1.0]
