import math
import pandas as pd


pesos_rede = [[[-2.21595734,  0.13902692, -0.77176005, -0.34096085, -1.8570363 ],
                [ 0.33239099, -0.91424223, -0.23843466, -0.19490422, -0.99810873],
                [-0.30775306, -0.36917993, -0.45628827,  0.40991321, -1.68630635],
                [-0.4026412 , -1.01636876,  0.09060966, -0.72841166, -0.7430336 ],
                [ 2.99476762, -0.75755223, -0.28804942,  0.16319673,  1.30222625]],

                [[-0.62010158,  0.50114589,  0.92234769, -0.20217932,  2.23636883],     
                [-0.28482048,  0.29105352,  0.19711757, -0.74399458,  0.49794197],
                [ 0.75473956,  0.38313632, -0.33896665,  0.44661062, -0.61256876],
                [-0.08044579,  0.6414196 , -0.42731671, -0.32764798, -0.48729865],
                [-0.7420365 ,  0.80040399, -0.54578971, -0.36181863,  1.71091415]],

                [[-0.73380492, -0.15859089],
                [ 0.33740235, -0.70982787],
                [-0.61637897,  0.66683674],
                [ 0.2132334 ,  0.00978615],
                [ 0.98322924, -2.29419219]]]

bias_rede = [[ 1.31951695, -1.17376615, -0.71409326, -0.55870824, -0.1274779 ],
            [-0.69192777, -0.39505221,  0.02265227, -0.17027953, -0.87033633],
            [-6.00660515,  2.97287126]]

pesos_para_camada_1 = pesos_rede[0]
pesos_para_camada_2 = pesos_rede[1]
pesos_para_camada_3 = pesos_rede[2]

# entrada = [2.575,2.572,2.572,2.571,2.580]
csv_entradas = pd.read_csv('./teste_classificador.csv')
colunas = csv_entradas.columns.tolist()
coluna_de_eventos = ['ligando_1', 'desligando_1']

[colunas.remove(nome_evento) for nome_evento in coluna_de_eventos]

entradas_df = csv_entradas[colunas]
entradas = []

def prepara_entrada():
        for index, row in entradas_df.iterrows():
            entradas.append([float(f"{item:5.3f}") for item in row])

def funcao_de_ativacao(valor):
   return max([0, valor])

def funcao_de_ativacao_saida(valor):
    if (valor >= 10):
        return 1
    elif (valor<= -10):
        return 0
    else: 
        resultado = (1/(1 + math.exp(-valor)))
        if (resultado >= 0.5):
            return 1
        else:
            return 0

def propagar_entrada_para_saida(entrada, saida, pesos, layer, funcao=False):
    for i in range(len(entrada)):
        for j in range(len(saida)):
            saida[j] += entrada[i] * pesos[i][j]

    for i in range(len(saida)):
        saida[i] += bias_rede[layer][i]

    if (not funcao):
        return [funcao_de_ativacao(saida[i]) for i in range(len(saida))]
    else:
        return [funcao_de_ativacao_saida(saida[i]) for i in range(len(saida))]

prepara_entrada()
saidas = []

for entrada in entradas:
    camada_1 = [0] * 5
    camada_2 = [0] * 5
    camada_3 = [0] * 2

    saida_1 = propagar_entrada_para_saida(entrada, camada_1, pesos_para_camada_1, 0)
    saida_2 = propagar_entrada_para_saida(saida_1, camada_2, pesos_para_camada_2, 1)
    saida = propagar_entrada_para_saida(saida_2, camada_3, pesos_para_camada_3, 2, True)

    saidas.append(saida)
    print(saida)