from sklearn.neural_network import MLPClassifier
from sklearn.metrics import confusion_matrix
import pandas as pd
import random
from entradateste import aaa



# def classificar():
def prepara_x_y():
    for index, row in test_x.iterrows():
        X.append([float(f"{item:5.3f}") for item in row])

    for index, row in test_y.iterrows():
        y.append(int(row))

def prepara_teste():
    for index, row in teste_rede.iterrows():
        z.append([float(f"{item:5.3f}") for item in row])

def pretty_print_matriz_confusao(matrix):
    for i in range(len(matriz)):
        for j in range(len(matriz[i])):
            print('Amostras de classe {0} e classificadas como classe {1}: {2} '.format(i, j, matriz[i][j]))

    for i in range(len(matriz)):
        tp = 0
        fp = 0
        for j in range(len(matriz[i])):
            if (i == j):
                tp = matriz[i][j]
            else:
                fp += matriz[i][j]
        print()
        print('Número de Verdadeiros Positivos da classe {0}: {1}'.format(i, tp))
        print('Número de Falsos Positivos da classe {0}: {1}'.format(i, fp))

entrada_rede = pd.read_csv('./entrada_classificador.csv')
teste_rede = pd.read_csv('./teste_classificador.csv')

colunas = entrada_rede.columns.tolist()

resposta_esperada_teste = teste_rede[colunas[-1:]]

entrada_teste = teste_rede[colunas[0:-1]]

test_x = entrada_rede[colunas[0:-1]]
test_y = entrada_rede[colunas[-1:]]

X = []
y = []
z = []

prepara_x_y()
prepara_teste() 
clf = MLPClassifier(solver='lbfgs', alpha=1e-3, hidden_layer_sizes=(5,8,8,5), random_state=1, activation='relu', max_iter=10000)

clf.fit(X,y)

predicao = clf.predict(entrada_teste)
predicao = predicao.tolist()
matriz = confusion_matrix(resposta_esperada_teste, predicao)

pretty_print_matriz_confusao(matriz)

# print("bias")
# print(clf.intercepts_)
# print()
# print("coefs")
# print(clf.coefs_)
