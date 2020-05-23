const express = require('express');
const bodyParser = require('body-parser');
const axios = require('axios');
const path = require('path');
const ejs = require('ejs');
const cors = require('cors')

const ObjectsToCsv = require('objects-to-csv');


const PORT = 80
const caminhoArquivoDeLog = '/root/.pm2/logs/web-api-out.log';

var idIntervalo
var capturando = false
var tempoEntreCapturas = 5000
var apiPort = 80
//var nodeServerIp = 'localhost'
var nodeServerIp = '162.214.93.72'
var ipParaCaptura = 'http://a413511d.ngrok.io'
var ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data'
var ultimoValorDoContador = 0
var valoresRms = []
var dataCsv = []
var tempoReferencia
var caminhoUltimoCsvGerado
var numeroDeZerosAnterior = 0;
var contadorDeTimeouts = 0;

const app = express()


app.set('view-engine', 'ejs')

app.use(bodyParser.json())
app.use(cors())


app.get('/', (req, res) => {
    res.render("index.ejs", {
        ipAtual: ipParaCaptura.replace('http://', ''),
        tempoEntreCapturas: tempoEntreCapturas,
        nodeServerIp: nodeServerIp,
        port: PORT
    });
})

app.get('/log', (req, res) => {
    res.sendFile(caminhoArquivoDeLog);
})

//#region endpoints
app.get('/comecar_captura', (req, res) => {
    if (!capturando) {
        idIntervalo = setInterval(() => {
            comecarCaptura()
        }, tempoEntreCapturas)
        capturando = true
    }
    res.send('capturando')
})

app.get('/pausar_captura', (req, res) => {
    if (capturando) {
        pausarCaptura()
    }
    res.send('parou')
})

app.get('/valores_capturados', (req, res) => {
    res.send(valoresRms)
})

app.get('/montar_csv', (req, res) => {
    pausarCaptura()
    console.log(`O usuario Pediu para montar o csv [DEBUG] ${timeStamp(new Date())}`);
    montarCsv();
    res.send(dataCsv)
})

app.get('/alterarTempo', (req, res) => {
    tempoEntreCapturas = req.query.tempo * 1000;
})

app.get('/alterarIp', (req, res) => {
    ipParaCaptura = `http://${req.query.ip}`
    ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data';
})
//#endregion


const comecarCaptura = () => {
    axios.get(ipApi)
        .then((result) => {
            let resposta = result.data;
            let proximaPosicao;
            let rmsAux = []
            contadorDeTimeouts = 0

            resposta = resposta.toString().split(" ");

            rmsAux = resposta[0].split(",");
            // console.log('Antes do split', rmsAux.length, rmsAux[rmsAux.length -1]);
            // console.log('tamanho', rmsAux.length);
            // console.log('As que chegaram', rmsAux)
            // console.log('Posicao anterior', ultimoValorDoContador)

            proximaPosicao = Number(resposta[1]);
            tamanhoVetorRmsQueChegou = rmsAux.length;

            // console.log('Proxima posicao', proximaPosicao)

            let numeroDeZerosAtual = rmsAux.filter(x => x == 0).length;

            if (numeroDeZerosAtual > numeroDeZerosAnterior && valoresRms.length) {
                console.log(`O ESP3 Reiniciou [DEBUG] ${timeStamp(new Date())}`);
                montarCsv();
            }

            if (valoresRms.length == 0) {//primeira captura
                rmsAux.forEach((amostra, index) => {
                    if (amostra != 0) {
                        valoresRms.push(amostra);
                        // console.log('primeiras amostras que entram', amostra)
                    }
                })
                if (valoresRms.length) {
                    tempoReferencia = Date.now() - 500 * valoresRms.length;
                }
            }
            else {
                let delta;

                if (proximaPosicao > ultimoValorDoContador) {
                    delta = proximaPosicao - ultimoValorDoContador
                }
                else {
                    delta = tamanhoVetorRmsQueChegou - ultimoValorDoContador + proximaPosicao
                }

                rmsAux = rmsAux.slice(tamanhoVetorRmsQueChegou - delta);

                rmsAux.forEach((amostra, index) => {
                    if (amostra != 0) {
                        valoresRms.push(amostra);
                        // console.log('amostras que entram', amostra)
                    }
                })

            }
            ultimoValorDoContador = proximaPosicao;
            numeroDeZerosAnterior = numeroDeZerosAtual;
        })
        .catch(function (error) {
            console.log(error);
            contadorDeTimeouts++;
            if (contadorDeTimeouts > 4) {
                console.log(`Ocorreram mais de 4 Timeouts ao consultar o ESP32 [DEBUG] ${timeStamp(new Date())}`);
                montarCsv();
                contadorDeTimeouts = 0;
            }
        })
}

const pausarCaptura = () => {
    // console.log(valoresRms)
    clearInterval(idIntervalo)
    capturando = false;
}

const montarCsv = () => {
    dataCsv = []

    caminhoUltimoCsvGerado = './rms/rms' + fileTimeStamp(new Date()) + '.csv'

    valoresRms.forEach((element, index) => dataCsv.push({
        ValoresRms: element,
        Tempo: timeStamp(new Date(500 * index + tempoReferencia)),
        TimeStamp: (500 * index + tempoReferencia)
    }))

    try {
        if (dataCsv.length) {
            const csv = new ObjectsToCsv(dataCsv).toDisk(caminhoUltimoCsvGerado, { append: false });
        }
    }
    catch (e) {
        console.log(e)
    }
    valoresRms = [];
    ultimoValorDoContador = 0;
    numeroDeZerosAnterior = 0;
}

const timeStamp = (a) => {
    return a.getDate().toString().padStart(2, "0") + "/"
        + (a.getMonth() + 1).toString().padStart(2, "0") + "/"
        + a.getFullYear() + " "
        + a.getHours().toString().padStart(2, "0") + ":"
        + a.getMinutes().toString().padStart(2, "0") + ":"
        + a.getSeconds().toString().padStart(2, "0") + "."
        + a.getMilliseconds().toString().padStart(3, "0");
}

const fileTimeStamp = (a) => {
    return a.getDate().toString().padStart(2, "0") + "_"
        + (a.getMonth() + 1).toString().padStart(2, "0") + "_"
        + a.getFullYear() + "____"
        + a.getHours().toString().padStart(2, "0") + "_"
        + a.getMinutes().toString().padStart(2, "0") + "_"
        + a.getSeconds().toString().padStart(2, "0") + "_"
        + a.getMilliseconds().toString().padStart(3, "0");
}

app.listen(PORT, function () {
    console.log(`Servidor iniciado na porta: ${PORT} [DEBUG] ${timeStamp(new Date())}`);
})