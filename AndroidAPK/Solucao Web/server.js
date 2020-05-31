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
var tempoEntreCapturas = 15000
//var nodeServerIp = 'localhost'
var nodeServerIp = '162.214.93.72' //ip da maquina onde está rodando o servidor node 
var apiPort = 80 //porta TCP que o ESP está rodando
var ipParaCaptura = 'http://3e97a7e1d77e.ngrok.io' // ip do ESP
var ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data'
var ultimoValorDoContador = 0
var valoresRms = []
var dataCsv = []
var tempoReferencia
var caminhoUltimoCsvGerado
var numeroDeZerosAnterior = 0;
var contadorDeTimeouts = 0;
var k = 2; //coef de seguranca
var numTimeouts = Math.ceil(((500 * 256) / (tempoEntreCapturas * k)));
var timeStampDaUltimaCaptura = Date.now();
//num timeouts = ((500ms * tamanho do buffer) / tempo de requisição) / k

const app = express()


app.set('view-engine', 'ejs')

app.use(bodyParser.json())
app.use(cors())


app.get('/', (req, res) => {
    res.render("index.ejs", {
        ipAtual: ipParaCaptura.replace('http://', ''),
        tempoEntreCapturas: tempoEntreCapturas / 1000,
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
    numTimeouts = Math.ceil(((500 * 256) / (tempoEntreCapturas * k)));
    res.send(`Tempo alterado para: ${req.query.tempo} s`)
})

app.get('/alterarIp', (req, res) => {
    ipParaCaptura = `http://${req.query.ip}`
    ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data';
    res.send(`IP alterado para: ${req.query.ip}`)
})
//#endregion


const comecarCaptura = () => {
    axios.get(ipApi, { timeout: Math.floor(tempoEntreCapturas * 0.7) })
        .then((result) => {
            let resposta = result.data;
            let proximaPosicao;
            let rmsAux = []
            contadorDeTimeouts = 0

            resposta = resposta.toString().split(" ");

            rmsAux = resposta[0].split(",");
            //#region Comentarios
            // console.log('Antes do split', rmsAux.length, rmsAux[rmsAux.length -1]);
            // console.log('tamanho', rmsAux.length);
            // console.log('As que chegaram', rmsAux)
            // console.log('Posicao anterior', ultimoValorDoContador)
            //#endregion
            timeStampDaUltimaCaptura = Date.now();

            proximaPosicao = Number(resposta[1]);
            tamanhoVetorRmsQueChegou = rmsAux.length;

            let numeroDeZerosAtual = rmsAux.filter(x => x === "0.000").length;

            if ((numeroDeZerosAtual > numeroDeZerosAnterior) && (valoresRms.length > 0)) {
                console.log(`O ESP3 Reiniciou [DEBUG] ${timeStamp(new Date())}`);
                montarCsv();
            }

            if (valoresRms.length == 0) {//primeira captura
                rmsAux.forEach((amostra, index) => {
                    if (amostra != 0) {
                        valoresRms.push(amostra);
                    }
                })
                if (valoresRms.length) {
                    tempoReferencia = Date.now() - 500 * (valoresRms.length - 1);
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
                    }
                })

            }
            ultimoValorDoContador = proximaPosicao;
            numeroDeZerosAnterior = numeroDeZerosAtual;
        })
        .catch(function (error) {
            contadorDeTimeouts++;
            console.log(`Erro na requisição [ERROR] com código ${error.code} em ${timeStamp(new Date())}`)
            if (contadorDeTimeouts > numTimeouts) {
                contadorDeTimeouts = 0;
                console.log(`Ocorreram mais de ${numTimeouts} Timeouts ao consultar o ESP32 [DEBUG] ${timeStamp(new Date())}`);
                montarCsv();
            }
        })
}

const pausarCaptura = () => {
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

    console.log(`A útlima amostra lida foi em [DEBUG] ${timeStamp(new Date(timeStampDaUltimaCaptura))} ou em EPOCH ${timeStampDaUltimaCaptura}`);

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
