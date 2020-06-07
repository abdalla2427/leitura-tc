//#region Importação das dependências e arquivo de configuração
const express = require('express');
const bodyParser = require('body-parser');
const axios = require('axios');
const path = require('path');
const ejs = require('ejs');
const cors = require('cors')
const fs = require('fs');
const ObjectsToCsv = require('objects-to-csv');
const apiConfig =  JSON.parse(fs.readFileSync("api-config.json"))
//#endregion

//#region Classe para Log da Aplicação
class Logger {
    erro(mensagem) {
        let mensagemDeLog = `${timeStamp(new Date())} [ERROR] ${mensagem}`
        console.log(mensagemDeLog);
        fs.appendFileSync(apiConfig.caminhoArquivoDeLog, `${mensagemDeLog}\n`);
    }
    debug(mensagem) {
        let mensagemDeLog = `${timeStamp(new Date())} [DEBUG] ${mensagem}`
        console.log(mensagemDeLog);
        fs.appendFileSync(apiConfig.caminhoArquivoDeLog, `${mensagemDeLog}\n`);
    }
}
//#endregion

//#region Setup das variaveis
var caminhoArquivoDeLog = apiConfig.caminhoArquivoDeLog
var PORT = apiConfig.PORT
var tempoEntreCapturas = apiConfig.tempoEntreCapturas
var pathBase = apiConfig.pathBase
var nodeServerIp = apiConfig.nodeServerIp
var apiPort = apiConfig.apiPort
var ipParaCaptura = apiConfig.ipParaCaptura
var endPointCaptura = apiConfig.endPointCaptura
var ipApi = `${ipParaCaptura}:${apiPort}/${endPointCaptura}`
var logger = new Logger()
//#endregion

//#region Parêmetros dinâmicos
var idIntervalo = null
var capturando = false
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
//#endregion

//#region Definição de parâmetros da engine do Servidor
const app = express()
app.set('view-engine', 'ejs')
app.use(bodyParser.json())
app.use(cors())
//#endregion

//#region Endpoints
app.get(pathBase + '/', (req, res) => {
    res.render("index.ejs", {
        ipAtual: ipParaCaptura.replace('http://', ''),
        pathBase: pathBase,
        tempoEntreCapturas: tempoEntreCapturas / 1000,
        nodeServerIp: nodeServerIp,
        port: PORT
    });
})

app.get(pathBase + '/log', (req, res) => {
    res.sendFile(__dirname + '/' + caminhoArquivoDeLog);
})

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
    if (capturando) pausarCaptura()
    res.send('parou')
})

app.get(pathBase + '/valores_capturados', (req, res) => {
    res.send(valoresRms)
})

app.get(pathBase + '/montar_csv', (req, res) => {
    logger.debug("O usuario Pediu para montar o csv")
    pausarCaptura()
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
//#endregion

//#region Funções para captura e salvamento do arquivo
const comecarCaptura = () => {
    axios.get(ipApi, { timeout: Math.floor(tempoEntreCapturas * 0.7) })
        .then((result) => {
            let resposta = result.data;
            let proximaPosicao;
            let rmsAux = []
            contadorDeTimeouts = 0

            resposta = resposta.toString().split(" ");

            rmsAux = resposta[0].split(",");

            timeStampDaUltimaCaptura = Date.now();

            proximaPosicao = Number(resposta[1]);
            tamanhoVetorRmsQueChegou = rmsAux.length;

            let numeroDeZerosAtual = rmsAux.filter(x => x === "0.000").length;

            if ((numeroDeZerosAtual > numeroDeZerosAnterior) && (valoresRms.length > 0)) {
                logger.debug("O ESP3 Reiniciou")
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
                logger.erro(`[ERROR] Ocorreram mais de ${numTimeouts} Timeouts ao consultar o ESP32`)
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

        caminhoUltimoCsvGerado = './rms/rms' + fileTimeStamp(new Date(timeStampDaUltimaCaptura)) + '.csv'

        var tempoMediOEntreAmostras = calcularTempoMedioEntreAmostras()

        valoresRms.forEach((element, index) => dataCsv.push({
            ValoresRms: element,
            Tempo: timeStamp(new Date(Math.trunc(tempoMediOEntreAmostras * index + tempoReferencia))),
            TimeStamp: Math.trunc((tempoMediOEntreAmostras * index + tempoReferencia))
        }))

        logger.debug(`O timestamp da última amostra foi: ${timeStamp(new Date(timeStampDaUltimaCaptura))}. Ou em EPOCH: ${timeStampDaUltimaCaptura}`);

        try {
            if (dataCsv.length) {
                const csv = new ObjectsToCsv(dataCsv).toDisk(caminhoUltimoCsvGerado, { append: false });
            }
        }
        catch (e) {
            logger.erro(`Erro ao escrever no csv`)
        }
        valoresRms = [];
        ultimoValorDoContador = 0;
        numeroDeZerosAnterior = 0;
    }
}

const calcularTempoMedioEntreAmostras = () => {
    return ((timeStampDaUltimaCaptura - tempoReferencia) / (valoresRms.length - 1))
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
//#endregion

//#region Inicialização do servidor
app.listen(PORT, function () {
    logger.debug(`Servidor iniciado na porta: ${PORT}`)
})
//#endregion
