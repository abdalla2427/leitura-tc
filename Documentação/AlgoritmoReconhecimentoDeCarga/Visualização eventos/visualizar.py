import pandas as pd
from datetime import datetime, timedelta
from matplotlib import pyplot as plt
from matplotlib import dates as mpl_dates
import matplotlib.dates as mdates


#data = pd.read_csv('data_grafico_2.csv', delimiter=",")
data = pd.read_csv('data_grafico_1.csv', delimiter=";")

#dia_completo = pd.read_csv('primeiro_evento.csv')
dia_completo = pd.read_csv('teste_mic.csv')

hours = mdates.HourLocator(interval = 1)
h_fmt = mdates.DateFormatter('%H:%M:%S')

data['TimeStamp'] = pd.to_datetime(data['TimeStamp'])
dia_completo['TimeStamp'] = pd.to_datetime(dia_completo['TimeStamp'])


horaEvento = data['TimeStamp']
tipoEvento = data['ClasseAparelho']

horaDiaCompleto = dia_completo['TimeStamp']
valoresRmsDiaCompleto = dia_completo['RMS']

dimTabela = len(tipoEvento)

eventos1 = []
eventos2 = []
horaEventos1 = []
horaEventos2 = []

for i in range(dimTabela):
    if (tipoEvento[i] == 1):
        horaEventos1.append(horaEvento[i])
        eventos1.append(5)

    elif (tipoEvento[i] == 2):
        horaEventos2.append(horaEvento[i])
        eventos2.append(0)

ax = plt.subplot()

xfmt = mdates.DateFormatter('%H:%M:%S')
ax.xaxis.set_major_formatter(xfmt)
# set ticks every 30 mins 
ax.xaxis.set_major_locator(mdates.MinuteLocator(interval=1))
# set fontsize of ticks
ax.tick_params(axis='x', which='major')
# rotate ticks for x axis
plt.xticks(rotation=30)


ligou = ax.scatter(horaEventos1, eventos1, color='black', marker="x", label="teste")
desligou = ax.scatter(horaEventos2, eventos2, color="black")
ax.plot(horaDiaCompleto, valoresRmsDiaCompleto, markersize=15)


# plt.plot_date(horaEvento, tipoEvento, linestyle='solid')

plt.title('')
plt.xlabel('Hora do Evento')
plt.ylabel('i_rms(A)')

plt.tight_layout()

plt.legend((ligou, desligou),
           ('Ligou', 'Desligou'),
           scatterpoints=1,
           loc='upper right',
           ncol=3,
           fontsize=15)
plt.show()


# horaEvento.xaxis.set_major_locator(hours)

# ax = plt.gcf().autofmt_xdate()

plt.title('ASSA')
plt.xlabel('Hora do Evento')
plt.ylabel('Tipo de Evento')

plt.tight_layout()

# plt.show()