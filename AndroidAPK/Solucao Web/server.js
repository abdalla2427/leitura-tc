const express = require('express')
const bodyParser = require('body-parser')
const axios = require('axios')
const path = require('path');
const ObjectsToCsv = require('objects-to-csv');

const PORT = 5000

var idIntervalo
var capturando = false
var tempoEntreCapturas = 1000
var apiPort = 4200
var ipParaCaptura = 'http://192.168.25.17'
var ipApi = ipParaCaptura + ':' + apiPort + '/i_rms_data'
var ultimoValorDoContador = 0
var tempoReferencia = []
var valoresRms = []
var dataCsv = []

const app = express()

app.use(bodyParser.json())

app.get('/', (req,res) => {
    res.sendFile(path.join(__dirname+'/index.html'));
    //res.render('index')
})

//#region endpoints
app.get('/comecar_captura', (req, res) => {
    if (!capturando) {
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
    montarCsv();
    res.download('./rms.csv')
})

app.get('/alterarTempo', (req, res) => {
    console.log(req);
})
//#endregion


const comecarCaptura = () => {
    axios.get(ipApi)
      .then((result) => {
        let resposta = result.data;
        let proximaPosicao;
        let rmsAux = []
        resposta = resposta.toString().split(" ");

        rmsAux = resposta[0].split(",");
        console.log('vetor inteiro', rmsAux);
        console.log()
        proximaPosicao = Number(resposta[1]);

        let delta;
        if (valoresRms.length == 0) {
          delta = rmsAux.length; // primeira vez
          tempoReferencia.push(Date.now() - 500 * rmsAux.length);
          tempoReferencia.push(rmsAux.length - 1);
        }
        else {
          delta = proximaPosicao - ultimoValorDoContador;
          if (proximaPosicao <= ultimoValorDoContador) {
            delta = rmsAux.length - ultimoValorDoContador + proximaPosicao;
          }
        }

        console.log('amostras', ultimoValorDoContador, proximaPosicao);
        rmsAux = rmsAux.slice(rmsAux.length - delta);      

        rmsAux.forEach((element) => {
          valoresRms.push(element);
          console.log(element);
        });
        console.log();
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

const timeStamp = (a) => {
    return a.getDate().toString().padStart(2, "0") + "/"
    + (a.getMonth() + 1).toString().padStart(2, "0") + "/"
    + a.getFullYear() + " " 
    + a.getHours().toString().padStart(2, "0") + ":" 
    + a.getMinutes().toString().padStart(2, "0") + ":" 
    + a.getSeconds().toString().padStart(2, "0") + "." 
    + a.getMilliseconds().toString().padStart(3, "0");
}

const montarCsv = async () => {
    pausarCaptura()
    valoresRms.forEach((element, index) => dataCsv.push({
        ValoresRms: element,
        Tempo: timeStamp(new Date(500 * index + tempoReferencia[0])),
    }))

    // var index = 0
    // for (var tempo = tempoReferencia[0] - (500 * tempoReferencia[1]); tempo < tempoReferencia[0] + (500 * (valoresRms.length - tempoReferencia[1])); tempo+= 500) {
    //     dataCsv[index].Tempo = timeStamp(new Date(tempo))
    //     index++
    // }
    try{
        const csv = await new ObjectsToCsv(dataCsv).toDisk('./rms.csv');
       
        //await console.log(csv.toString());
    }
    catch (e){
        console.log(e)
    }
      
}

app.listen(PORT, function(){
    console.log('running on ' + PORT)
})