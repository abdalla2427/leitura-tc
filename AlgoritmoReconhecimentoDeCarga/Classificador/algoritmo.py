from sklearn.neural_network import MLPClassifier
from merge_csv import merge

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
- Valor da reta tangente que liga o primeiro ponto da janela ao ultimo;
- Moda dos valores
'''
a = merge()

chunk_size = 2
tamanho_csv = a.shape[0]
x = []
y = []

if (tamanho_csv > chunk_size):
      for n in range(tamanho_csv - chunk_size + 1):
           x.append(a[n: n + chunk_size]["ValoresRms"])
           y.append(a[n:n + chunk_size][["ligando_1", "desligando_1", "ligando_2", "desligando_2"]])


print(x[0])
print(y[0])