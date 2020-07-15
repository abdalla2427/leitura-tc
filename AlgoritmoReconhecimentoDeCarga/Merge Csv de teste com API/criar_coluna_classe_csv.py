import pandas as pd
import matplotlib.pyplot as plt
import glob

#script para criacao de coluna nos csv da pasta "entrada"

arquivos = glob.glob("./entrada/*.csv")

for nome in arquivos:
    nome_arquivo = nome.replace('./entrada', '')
    csv_api = pd.read_csv(nome, delimiter=',')
    numero_de_linhas = csv_api.shape[0]

    vetor_em_branco = [0 for i in range(numero_de_linhas)]
    csv_api["classe"] = vetor_em_branco
    csv_api['ValoresRms'] = csv_api['ValoresRms'].map('{:,.3f}'.format)

    csv_api.to_csv('./saida/' + nome_arquivo, index=False)
 