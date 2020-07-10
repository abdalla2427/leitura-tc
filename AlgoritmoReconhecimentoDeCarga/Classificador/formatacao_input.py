import pandas as pd
import glob


class Lista:
      def __init__(self):
            self.x_final = []
            self.y_final = []
            self.z_final = []

glob.glob("./paraOBanco/*.csv")
lista = Lista()
def criar_dataframe(caminho):
      csv_api = pd.read_csv(caminho, delimiter=',')
      chunk_size = 5
      tamanho_csv = csv_api.shape[0]
      df = pd.DataFrame()
      x = []
      y =[]

      if (tamanho_csv > chunk_size):
            for n in range(tamanho_csv - chunk_size + 1):
                  x.append(csv_api[n: n + chunk_size]["ValoresRms"])
                  y.append(csv_api[n:n + chunk_size][["ligando_1", "desligando_1"]])
      
      cont = 0
      X=[]
      for i in x:
            X.append([f'{j:5.3f}' for j in i])
            cont += 1

      cont = 0
      for i in y:
            bol1 = False
            bol2 = False
            for j in i["ligando_1"]:
                  bol1 = bol1 or bool(j)
            for j in i["desligando_1"]:
                  bol2 = bol2 or bool(j)
            y[cont] = [int(bol1), int(bol2)]
            lista.z_final.insert(0, X[cont] + y[cont])
            cont += 1
      
      lista.x_final = X + lista.x_final
      lista.y_final = y + lista.y_final
      
arquivos = glob.glob("./paraOBanco/*.csv")

for nome in arquivos:
      criar_dataframe(nome)

header_csv = [str(i + 1) + "a amostra" for i in range(len(lista.x_final[0]))] + ["ligando_1", "desligando_1"]
saida=pd.DataFrame(lista.z_final, columns=header_csv)
saida.to_csv('./saida_teste.csv', header=True, index=False)
# X = [[0., 0.], [1., 1.]]
# y = [0, 1]
# clf = MLPClassifier(solver='lbfgs', alpha=1e-5,hidden_layer_sizes=(5, 2), random_state=1)