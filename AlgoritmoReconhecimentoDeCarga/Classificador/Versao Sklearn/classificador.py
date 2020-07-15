from sklearn.neural_network import MLPClassifier
from sklearn.metrics import confusion_matrix
import pandas as pd
import random
from entradateste import aaa



def classificar():
    def prepara_x_y():
        for index, row in test_x.iterrows():
            X.append([float(f"{item:5.3f}") for item in row])

        for index, row in test_y.iterrows():
            y.append([b for b in row])

    def prepara_teste():
        for index, row in teste_rede.iterrows():
            z.append([float(f"{item:5.3f}") for item in row])



    entrada_rede = pd.read_csv('./teste_entrada_algoritmo/saida_teste.csv')
    teste_rede = pd.read_csv('./teste_classificador.csv')

    colunas = entrada_rede.columns.tolist()
    resposta_esperada_teste = teste_rede[colunas[-2:]]
    entrada_teste = teste_rede[colunas[0:-2]]
    test_x = entrada_rede[colunas[0:-2]]
    test_y = entrada_rede[colunas[-2:]]

    X = []
    y = []
    z = []

    prepara_x_y()
    prepara_teste()
    clf = MLPClassifier(solver='lbfgs', alpha=1e-3, hidden_layer_sizes=(5,5), random_state=1, activation='relu')

    clf.fit(X, y)
    predicao = clf.predict(entrada_teste)
    lista_predicao = predicao.tolist();

    lista_esperada = [[int(row['ligando_1']), int(row['desligando_1'])]for index, row in resposta_esperada_teste.iterrows()]

    total = 0
    iguais = 0
    for i in range(len(lista_predicao)):
        if (lista_predicao[i] == lista_esperada[i]):
            iguais += 1
        total += 1

    lista_esperada_classe1 = [i[0] for i in lista_esperada]
    lista_esperada_classe2 = [i[1] for i in lista_esperada]
    lista_predicao_classe1 = [i[0] for i in lista_predicao]
    lista_predicao_classe2 = [i[1] for i in lista_predicao]

    tn1, fp1, fn1, tp1 = confusion_matrix(lista_esperada_classe1, lista_predicao_classe1).ravel()
    precision1, recall1 = [tp1/(tp1 + fp1), tp1/(tp1 + fn1)]
    tn2, fp2, fn2, tp2 = confusion_matrix(lista_esperada_classe2, lista_predicao_classe2).ravel()
    precision2, recall2 = [tp2/(tp2 + fp2), tp2/(tp2 + fn2)]

    erradas = total - iguais
    porcentagem_acertos = '{:.5}'.format(str(float(iguais/total) * 100))
    porcentagem_erros = '{:.5}'.format(str(float(erradas/total) * 100))
    print(f'======== Summary ========\n')
    print(f'Instâncias classificadas corretamente:   {iguais}  {porcentagem_acertos}%')
    print(f'Instâncias classificadas incorretamente:  {erradas}  {porcentagem_erros}%\n')
    print(f'======== Detailed Accuracy By Class ========')
    print(f'TP Rate FP Rate Precision Recall Class')
    print(f'{tp1} {fp1} {precision1} {recall1} 1')
    print(f'{tp2} {fp2} {precision2} {recall2} 2\n')
    print(f'======== Confusion Matrix ========')


    coefs = clf.coefs_
    bias = clf.intercepts_
    return predicao
    
classificar()