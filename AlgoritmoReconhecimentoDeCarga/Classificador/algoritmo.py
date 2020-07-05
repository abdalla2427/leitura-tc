from sklearn.neural_network import MLPClassifier


'''
hidden_layer_sizes=(7,) if you want only 1 hidden layer with 7 hidden units.
length = n_layers - 2 is because you have 1 input layer and 1 output layer.
'''
X = [[0., 0.], [1., 1.]]
y = [0, 1]
clf = MLPClassifier(solver='lbfgs', alpha=1e-5,
                        hidden_layer_sizes=(2, 2), random_state=1) #usa funcao de ativacao para a resposta 

'''
o o o

o o o
      o
o o o
'''

clf.fit(X, y)
print(clf.coefs_)
print(clf.intercepts_)