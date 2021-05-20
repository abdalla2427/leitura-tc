import pandas as pd
from datetime import datetime, timedelta
from matplotlib import pyplot as plt
from matplotlib import dates as mpl_dates
import matplotlib.dates as mdates


data = pd.read_csv('saida.csv', delimiter=";")
print(data)
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
ax.xaxis.set_major_locator(mdates.MinuteLocator(interval=30))
# set fontsize of ticks
ax.tick_params(axis='x', which='major')
# rotate ticks for x axis
plt.xticks(rotation=30)


ax.scatter(horaEventos1, eventos1, color='green', marker="s")
ax.scatter(horaEventos2, eventos2, color="red")
ax.plot(horaDiaCompleto, valoresRmsDiaCompleto)

# plt.plot_date(horaEvento, tipoEvento, linestyle='solid')

plt.title('Gráfico RMS')
plt.xlabel('Hora do Evento')
plt.ylabel('Tipo de Evento')

plt.tight_layout()
plt.show()


# horaEvento.xaxis.set_major_locator(hours)

# ax = plt.gcf().autofmt_xdate()

plt.title('ASSA')
plt.xlabel('Hora do Evento')
plt.ylabel('Tipo de Evento')

plt.tight_layout()

# plt.show()