from flask import Flask, Response
from json import dumps
import math
import random
from lista import lista_moq

atual = 0;
app = Flask(__name__)

class Timer:
    def __init__(self, lista):
        self.t = 0
        self.lista_moq = lista
        self.anterior = 0

timer = Timer(lista_moq)
@app.route('/', methods=['GET'])
def home_page():
    timer.t = timer.t + 1
    resposta = dumps({
        "ValorRms": str(random.random() - 0.5),
        "Tempo": timer.t
    })
    resp = Response(resposta)
    resp.headers['Access-Control-Allow-Origin'] = '*'
    return resp

@app.route('/resetTimer')
def reset_timer():
    timer.t = 0
    return str(timer.t)

@app.route('/i_rms_data')
def vetor_rms():
    ptr = ''
    delta = random.randint(1, 9)
    for i in range(10):
        ptr += str(timer.lista_moq[i]) + ','

    timer.lista_moq = timer.lista_moq[delta:]
    prt = ptr[:-1]  
    ptr += " "
    ptr += str(delta)

    resposta = Response(ptr)
    resposta.headers['Access-Control-Allow-Origin'] = '*'
    return resposta
    

app.run(port=80, debug=True, host="0.0.0.0")
