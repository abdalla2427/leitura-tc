from sklearn.neural_network import MLPClassifier
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
    teste_rede = teste_rede[colunas[0:-2]]
    test_x = entrada_rede[colunas[0:-2]]
    test_y = entrada_rede[colunas[-2:]]

    X = []
    y = []
    z = []

    prepara_x_y()
    prepara_teste()
    clf = MLPClassifier(solver='lbfgs', alpha=1e-3, hidden_layer_sizes=(5,5), random_state=1, activation='relu')

    clf.fit(X, y)
    predicao = clf.predict(aaa)
    #predicao = clf.predict([[2.575,2.572,2.572,2.571,2.580]])
    #print(predicao)
    coefs = clf.coefs_
    bias = clf.intercepts_
    return predicao
    
classificar()