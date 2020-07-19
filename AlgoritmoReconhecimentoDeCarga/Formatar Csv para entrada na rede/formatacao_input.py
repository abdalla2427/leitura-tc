import pandas as pd
import glob
import matplotlib.pyplot as plt


class Lista:
      def __init__(self):
            self.x_final = []
            self.y_final = []
            self.z_final = []

lista = Lista()
chunk_size = 5
nome_pasta = 'eventos_2'
def criar_dataframe(caminho):
      csv_api = pd.read_csv(caminho, delimiter=',')
      tamanho_csv = csv_api.shape[0]
      df = pd.DataFrame()
      x = []
      y =[]

      if (tamanho_csv > chunk_size):
            for n in range(tamanho_csv - chunk_size + 1):
                  x.append(csv_api[n: n + chunk_size]["ValoresRms"])
                  y.append(csv_api[n:n + chunk_size]["ClasseAparelho"])
      
      X=[]
      for i in x:
            X.append([f'{j:5.3f}' for j in i])

      cont = 0
      for i in y:
            bol1 = 0
            for j in i:
                  if(j > bol1):
                        bol1 = j

                  
            y[cont] = [int(bol1)]
            lista.z_final.append(X[cont] + y[cont])
            cont += 1
      
arquivos = glob.glob("./"+ nome_pasta +"/*.csv")

for nome in arquivos:
      criar_dataframe(nome)

header_csv = [str(i + 1) + "a amostra" for i in range(chunk_size)] + ["ClasseAparelho"]
saida=pd.DataFrame(lista.z_final, columns=header_csv)
saida.to_csv('./'+ nome_pasta + '.csv', header=True, index=False)