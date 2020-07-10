from sklearn.neural_network import MLPClassifier
import pandas as pd


entrada_rede = pd.read_csv('./teste_entrada_algoritmo/saida_teste.csv')
teste_rede = pd.read_csv('./teste_classificador.csv')

colunas = entrada_rede.columns.tolist()
teste_rede = teste_rede[colunas[0:-2]]
test_x = entrada_rede[colunas[0:-2]]
test_y = entrada_rede[colunas[-2:]]

X = []
y = []
z = []

#region x e y
for index, row in test_x.iterrows():
    X.append([float(f"{item:5.3f}") for item in row])

for index, row in test_y.iterrows():
    y.append([b for b in row])
#endregion

#region teste_rede
for index, row in teste_rede.iterrows():
    z.append([float(f"{item:5.3f}") for item in row])
#endregion

clf = MLPClassifier(solver='lbfgs', alpha=1e-3,
                        hidden_layer_sizes=(5, 5), random_state=1, verbose=True)

clf.fit(X, y)
print('\n'*10)
print(clf.coefs_)
print()
print(clf.intercepts_)
predicao = clf.predict(z)

'''
hidden_layer_sizes=(7,) if you want only 1 hidden layer with 7 hidden units.
length = n_layers - 2 is because you have 1 input layer and 1 output layer.
X = [[0., 0.], [1., 1.]]
y = [0, 1]
clf = MLPClassifier(solver='lbfgs', alpha=1e-5,
                        hidden_layer_sizes=(2, 2), random_state=1) #usa funcao de ativacao para a resposta 
'''

'''
o o o

o o o
      o
o o o


clf.fit(X, y)
print(clf.coefs_)
print(clf.intercepts_)
'''

'''
Features:
- Delta variação de corrente;
- Nova area adicionada ao grafico: Somatorio de todos os pontos - valor minino * numero de pontos;
- Variacao de desvio padrão em relacao a janela de amostras anterior;
- Valor da reta secante que liga o primeiro ponto da janela ao ultimo;
- Moda dos valores;
'''
