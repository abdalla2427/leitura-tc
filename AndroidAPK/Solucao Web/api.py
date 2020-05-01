from flask import Flask, Response
from json import dumps
import math
import random


app = Flask(__name__)

class Timer:
    def __init__(self):
        self.t = 0

timer = Timer()
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
    for i in range(10):
        ptr += str(random.random() + 1) + ','
    ptr = ptr[:-1]
    ptr+= " "
    ptr+= str(random.randint(1,9));
    resposta = Response(ptr)
    resposta.headers['Access-Control-Allow-Origin'] = '*'
    return resposta
    

app.run(port=4200, debug=True, host="0.0.0.0")
