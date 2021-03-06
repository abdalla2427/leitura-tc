const express = require('express')
const bodyParser = require('body-parser')
const axios = require('axios')
const path = require('path');
const ObjectsToCsv = require('objects-to-csv');

const PORT = 5000

var idIntervalo
var capturando = false
var tempoEntreCapturas = 1000
var apiPort = 17890
var ipParaCaptura = 'http://192.168.25.17'
var ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data'
var ultimoValorDoContador = 0
var valoresRms = []
var dataCsv = []
var tempoReferencia
var primeiraCapturaDepoisDeResetar = true
var caminhoUltimoCsvGerado

const app = express()

app.use(bodyParser.json())

app.get('/', (req,res) => {
    res.sendFile(path.join(__dirname+'/index.html'));
})

//#region endpoints
app.get('/comecar_captura', (req, res) => {
    if (!capturando) {
        if (valoresRms.length) montarCsv()
        idIntervalo  = setInterval(() => {
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
    montarCsv();
	setTimeout(() => {res.download(caminhoUltimoCsvGerado, 'rms.csv')}, 2000)
})

app.get('/alterarTempo', (req, res) => {
    tempoEntreCapturas = req.query.tempo * 1000;
})

app.get('/alterarIp', (req, res) => {
    ipParaCaptura = `http://${req.query.ip}`
    ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data';
    res.send(ipApi)
})
//#endregion


const comecarCaptura = () => {
    console.log(ipApi)
    axios.get(ipApi)
      .then((result) => {
        let resposta = result.data;
        let proximaPosicao;
        let rmsAux = []

        resposta = resposta.toString().split(" ");

        rmsAux = resposta[0].split(",");
        rmsAux.pop();
        proximaPosicao = Number(resposta[1]);

        console.log('Antes dos Ifs', rmsAux, proximaPosicao);

        if (valoresRms.length == 0) tinhaZero = true;

        if (!(rmsAux[0] == 0) && !primeiraCapturaDepoisDeResetar) {
            primeiraCapturaDepoisDeResetar = true;
            console.log('Não é a primeira captura depois do reset, e primeiro valor do vetor ainda é 0.')
        }
        
        if ((rmsAux[0] == 0) && primeiraCapturaDepoisDeResetar) {
            montarCsv()
            console.log('Vetor Rms Quando o primeiro valor é 0, e é a primeira captura após o reset', rmsAux, primeiraCapturaDepoisDeResetar)
            rmsAux.forEach(element => {
                if (element != 0) {
                    valoresRms.push(element)
                    console.log('Valor introduzido no valoresRms ', element);
                }
            })
            primeiraCapturaDepoisDeResetar = false
        }
        else {
            console.log('Caso contrario')
            let delta;
            if (tinhaZero) {
              delta = rmsAux.length; // primeira vez
            }
            else {
              delta = proximaPosicao - ultimoValorDoContador;
              if (proximaPosicao <= ultimoValorDoContador) {
                delta = rmsAux.length - ultimoValorDoContador + proximaPosicao;
              }
            }
    
            rmsAux = rmsAux.slice(rmsAux.length - delta);
            console.log('Vetor que vai entrar no valoresRms', rmsAux)        
            rmsAux.forEach((element) => {
                valoresRms.push(element);
            });
        } 

        if (tinhaZero) {
            tempoReferencia = Date.now() - 500 * valoresRms.length;
            tinhaZero = false;
        }

        ultimoValorDoContador = proximaPosicao;
      })
      .catch(function (error) {
        console.log(error);
      })
}

const pausarCaptura = () => {
    clearInterval(idIntervalo)
    capturando = false;
}

const montarCsv = () => {
    
    caminhoUltimoCsvGerado = './rms/rms' + fileTimeStamp(new Date()) + '.csv'
    
    console.log(valoresRms)
    valoresRms.forEach((element, index) => dataCsv.push({
        ValoresRms: element,
        Tempo: timeStamp(new Date(500 * index + tempoReferencia)),
    }))
    
    try{
        const csv = new ObjectsToCsv(dataCsv).toDisk(caminhoUltimoCsvGerado, {append:false});
    }
    catch (e){
        console.log(e)
    }
    valoresRms = [];
    ultimoValorDoContador = 0;
    dataCsv = []
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
    return a.getDate().toString().padStart(2, "0") 
    + (a.getMonth() + 1).toString().padStart(2, "0") 
    + a.getFullYear()  
    + a.getHours().toString().padStart(2, "0")
    + a.getMinutes().toString().padStart(2, "0") 
    + a.getSeconds().toString().padStart(2, "0") 
    + a.getMilliseconds().toString().padStart(3, "0");
}

app.listen(PORT, function(){
    console.log('running on ' + PORT)
})