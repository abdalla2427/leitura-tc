const express = require('express');
const bodyParser = require('body-parser');
const axios = require('axios');
const path = require('path');
const ejs = require('ejs');
const cors = require('cors')
const fs = require('fs');


const ObjectsToCsv = require('objects-to-csv');


const PORT = 80
const caminhoArquivoDeLog = '/root/.pm2/logs/web-api-out.log';
const caminhoArquivoProxPosicaoLog = __dirname + '/prox-posicao.log';


var proxPosicaoLog = fs.createWriteStream(caminhoArquivoProxPosicaoLog, {flags: 'a'})
proxPosicaoLog.write(`\n`)
var idIntervalo
var capturando = false
var tempoEntreCapturas = 5000
//var nodeServerIp = 'localhost'
var pathBase = '/leitura-tc'
var nodeServerIp = 'abdalla2427.com' + pathBase//ip da maquina onde está rodando o servidor node 
var apiPort = 80 //porta TCP que o ESP está rodando
var ipParaCaptura = 'http://3e97a7e1d77e.ngrok.io' // ip do ESP
var ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data'
var ultimoValorDoContador = 0
var valoresRms = []
var dataCsv = []
var tempoReferencia = 0
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

// app.use(pathBase + '/arquivos', express.static(__dirname + '/rms'));

app.get(pathBase + '/', (req, res) => {
    res.render("index.ejs", {
        ipAtual: ipParaCaptura.replace('http://', ''),
        tempoEntreCapturas: tempoEntreCapturas / 1000,
        nodeServerIp: nodeServerIp,
        port: PORT
    });
})

app.get(pathBase + '/log', (req, res) => {
    res.sendFile(caminhoArquivoDeLog);
})

//#region endpoints
app.get(pathBase + '/comecar_captura', (req, res) => {
    if (!capturando) {
        idIntervalo = setInterval(() => {
            comecarCaptura()
        }, tempoEntreCapturas)
        capturando = true
    }
    res.send('capturando')
})

app.get(pathBase + '/pausar_captura', (req, res) => {
    if (capturando) {
        pausarCaptura()
    }
    res.send('parou')
})

app.get(pathBase + '/valores_capturados', (req, res) => {
    res.send(valoresRms)
})

app.get(pathBase + '/montar_csv', (req, res) => {
    pausarCaptura()
    console.log(`O usuario Pediu para montar o csv [DEBUG] ${timeStamp(new Date())}`);
    montarCsv();
    res.send(dataCsv)
})

app.get(pathBase + '/alterarTempo', (req, res) => {
    tempoEntreCapturas = req.query.tempo * 1000;
    numTimeouts = Math.ceil(((500 * 256) / (tempoEntreCapturas * k)));
    res.send(`Tempo alterado para: ${req.query.tempo} s`)
})

app.get(pathBase + '/alterarIp', (req, res) => {
    ipParaCaptura = `http://${req.query.ip}`
    ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data';
    res.send(`IP alterado para: ${req.query.ip}`)
})

app.get(pathBase + '/proxPosicaoLog', (req, res) => {
    res.sendFile(caminhoArquivoProxPosicaoLog);
})
//#endregion

const listarArquivos = () => {
    var arquivosNoDiretorio = []
    var read = fs.readdirSync(__dirname + '/rms')
    read.forEach(file => {
        arquivosNoDiretorio.push(file)
    })
    return arquivosNoDiretorio
}

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

            proxPosicaoLog.write(proximaPosicao.toString() + ',')

            let numeroDeZerosAtual = rmsAux.filter(x => x === "0.000").length;

            if ((numeroDeZerosAtual > numeroDeZerosAnterior) && (valoresRms.length > 0)) {
                console.log(`${timeStamp(new Date())} [DEBUG] O ESP3 Reiniciou`);
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
            if (contadorDeTimeouts > numTimeouts) {
                contadorDeTimeouts = 0;
                console.log(`${timeStamp(new Date())} [ERROR] Ocorreram mais de ${numTimeouts} Timeouts ao consultar o ESP32`);
                montarCsv();
            }
        })
}

const pausarCaptura = () => {
    clearInterval(idIntervalo)
    capturando = false;
}

const montarCsv = () => {
    if (valoresRms.length) {
        dataCsv = []
    
        caminhoUltimoCsvGerado = './rms/rms' + fileTimeStamp(new Date()) + '.csv'

        var tempoMediOEntreAmostras = calcularTempoMedioEntreAmostras()
    
        valoresRms.forEach((element, index) => dataCsv.push({
            ValoresRms: element,
            Tempo: timeStamp(new Date(tempoMediOEntreAmostras * index + tempoReferencia)),
            TimeStamp: (500 * index + tempoReferencia)
        }))
    
        console.log(`${timeStamp(new Date(timeStampDaUltimaCaptura))} [DEBUG] Foi o timestamp da ultima amostra. Ou em EPOCH: ${timeStampDaUltimaCaptura}`);
    
        try {
            if (dataCsv.length) {
                const csv = new ObjectsToCsv(dataCsv).toDisk(caminhoUltimoCsvGerado, { append: false });
                proxPosicaoLog.write(timeStamp(new Date(timeStampDaUltimaCaptura)) + `\n`)
            }
        }
        catch (e) {
            console.log(e)
        }
        valoresRms = [];
        ultimoValorDoContador = 0;
        numeroDeZerosAnterior = 0;
    }
}

const calcularTempoMedioEntreAmostras = () => {
    return ((tempoReferencia - timeStampDaUltimaCaptura) / (valoresRms.length - 1))
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
    console.log(`${timeStamp(new Date())} [DEBUG] Servidor iniciado na porta: ${PORT}`);
})
