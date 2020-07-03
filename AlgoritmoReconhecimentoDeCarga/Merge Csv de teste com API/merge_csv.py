import pandas as pd


class Constantes:
    def __init__(self):
        self.nomeApi = 'rms01_07_2020____17_38_58_837.csv'
        self.tagInicioDataApi = '____'

        self.nomeTeste = 'registro_de_eventos_07_06_2020.csv'
        self.tagInicioDataTeste = 'registro_de_eventos_'

        self.tipoAparelho = "Tipo de aparelho"
        self.tipoEvento = "Tipo de evento"
        self.horaEvento = "Hora do evento (HH:MM)"
        self.timeStamp = "TimeStamp"

constantes = Constantes()

nome_api = constantes.nomeApi
tag_inicio_data_api = constantes.tagInicioDataApi

nome_teste = constantes.nomeTeste
tag_inicio_data_teste = constantes.tagInicioDataTeste

csv_api = pd.read_csv(nome_api, delimiter=',')
csv_teste = pd.read_csv(nome_teste, delimiter=';')

index_dia_evento_teste = nome_teste.find(tag_inicio_data_teste)
dia_evento_teste = nome_teste[index_dia_evento_teste + len(tag_inicio_data_teste):-4]

index_dia_evento_api = nome_api.find(tag_inicio_data_api)
dia_evento_api = nome_api[3:index_dia_evento_api]

hora_dos_eventos_1 = []
hora_dos_eventos_2 = []

classe_1 = []
classe_2 = []

def formatar_hora(hora):
    if len(hora) < 8:
        return (hora + ":00")
    else:
        return (hora[:8])

def timestamp_para_hora(timestamp):
    return (timestamp[11:])

def converter_string_para_tempo(hora):
    lista_tempo = hora.split(":")
    lista_tempo[0] = int(lista_tempo[0]*60*60)
    lista_tempo[1] = int(lista_tempo[1]*60)
    lista_tempo[2] = int(lista_tempo[2])
    return sum(lista_tempo)

def verificar_se_ocorreu_um_evento(hora_formatada, id_carga, ultimo_valor):
    if ((id_carga == 1) and (len(hora_dos_eventos_1) >= 1)):
        tempo_api = converter_string_para_tempo(hora_formatada)
        tempo_teste = converter_string_para_tempo(formatar_hora(hora_dos_eventos_1[0][1]))

        if (tempo_api > tempo_teste):
            classe_1.append(hora_dos_eventos_1[0][0])
            return hora_dos_eventos_1.pop(0)[0]
            
        classe_1.append(ultimo_valor)


    elif ((id_carga == 2) and (len(hora_dos_eventos_2) >= 1)):
        tempo_api = converter_string_para_tempo(hora_formatada)
        tempo_teste = converter_string_para_tempo(formatar_hora(hora_dos_eventos_2[0][1]))

        if (tempo_api > tempo_teste):
            classe_2.append(hora_dos_eventos_2[0][0])
            return hora_dos_eventos_2.pop(0)[0]
        classe_2.append(ultimo_valor)

    return ultimo_valor

if (True):#dia_evento_api == dia_evento_teste):
    ultimo_evento_de_1 = 0
    ultimo_evento_de_2 = 0

    for index, row in csv_teste.iterrows():
        if row[constantes.tipoAparelho] == 1:
            hora_dos_eventos_1.append([row[constantes.tipoEvento], formatar_hora(row[constantes.horaEvento])])
            ultimo_evento_de_1 = index
        elif row[constantes.tipoAparelho] == 2:
            hora_dos_eventos_2.append([row[constantes.tipoEvento], formatar_hora(row[constantes.horaEvento])])
            ultimo_evento_de_2 = index
        else:
            if (ultimo_evento_de_1 > ultimo_evento_de_2):
                hora_dos_eventos_1.append([row[constantes.tipoEvento], formatar_hora(row[constantes.horaEvento])])
                ultimo_evento_de_1 = index
            else:
                hora_dos_eventos_2.append([row[constantes.tipoEvento], formatar_hora(row[constantes.horaEvento])])
                ultimo_evento_de_2 = index

    ultimo_estado_1 = 0
    ultimo_estado_2 = 0

    for index, row in csv_api.iterrows():
        hora = timestamp_para_hora(row[constantes.timeStamp])
        ultimo_estado_1 = verificar_se_ocorreu_um_evento(formatar_hora(hora), 1, ultimo_estado_1)
        ultimo_estado_2 = verificar_se_ocorreu_um_evento(formatar_hora(hora), 2, ultimo_estado_2)

    print(classe_1, classe_2)