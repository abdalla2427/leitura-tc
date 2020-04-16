import React, { useState, useEffect, Component } from 'react';
import { View, StyleSheet } from 'react-native';
import axios from 'react-native-axios';
import RNFetchBlob from 'rn-fetch-blob';
import Share from 'react-native-share';
import { Button, Card, TextInput, Text, Switch } from 'react-native-paper';


export default function YourApp() {
  const [valoresRms, setValoresRms] = useState([]);
  const [valoresDeTempo, setValoresDeTempo] = useState([]);
  const [idIntervalo, setIdIntervalo] = useState();
  const [ipParaCaptura, setIpParaCaptura] = useState('http://192.168.25.17:5000/i_rms_data');
  const [leituraEmAndamento, setLeituraEmAndamento] = useState(false);
  const [ultimoValorDoContador, setUltimoValorDoContador] = useState(0);
  const [tempoReferencia, setTempoReferencia] = useState([]);

  const startRmsCapture = () => {
    if (!leituraEmAndamento) {
      setIdIntervalo(setInterval(() => {
        axios.get(ipParaCaptura)
          .then((result) => {
            let resposta = result.data;
            let proximaPosicao;
            let rmsAux = []

            resposta = resposta.toString().split(" ");

            rmsAux = resposta[0].split(",");
            proximaPosicao = Number(resposta[1]);
            
            let delta;

            if (valoresRms.length == 0) {
              delta = rmsAux.length; // primeira vez
              tempoReferencia.push(Date.now());
              tempoReferencia.push(rmsAux.length - 1);
            }
            else {
              delta = proximaPosicao - ultimoValorDoContador;
              if (proximaPosicaom < ultimoValorDoContador) {
                delta = rmsAux.length - ultimoValorDoContador + proximaPosicao;
              }
            }

            rmsAux = rmsAux.slice(rmsAux.length - delta);

            rmsAux.forEach((element, index) => {
              valoresRms.push(element);
            });
            setUltimoValorDoContador(proximaPosicao);
          })
          .catch(function (error) {
            console.log(error);
          })
      }, 1 * 1000));
    }
    setLeituraEmAndamento(true);
  }

  const stopRmsCapture = () => {
    if (leituraEmAndamento) {
      clearInterval(idIntervalo);
      setLeituraEmAndamento(false);
    }
  }

  const verificarEstado = () => {
    if (!leituraEmAndamento) {
      startRmsCapture();
    } 
    else {
      stopRmsCapture();
    }
  }

  const exportCsv = () => {
    stopRmsCapture();
    var dirs = RNFetchBlob.fs.dirs
    const lugar = dirs.DocumentDir + '/Rms.csv';
    RNFetchBlob.fs
      .writeFile(lugar, montarCsv(), 'utf8')
      .then(() => {
        RNFetchBlob.fs
          .readFile(lugar, 'utf-8')
          .then((data) => {
            Share.open({ url: `file://${lugar}` })
            .then((result) => console.log("salvo com sucesso"))
            .catch((error) => console.log(error));
          })
          .catch((error) => console.log(error));
      })
  }

  const montarCsv = () => {
    let csvRms, csvTempo, arquivoCsv;
    valoresDeTempo.map(x => x)
    csvRms = valoresRms.toString();
    arquivoCsv = 'Rms,' + csvRms + '\n' + 'Tempo,' //+ csvTempo;
    for (var tempo = tempoReferencia[0] - (500 * tempoReferencia[1]); tempo < tempoReferencia[0] + (500 * (valoresRms.length - 1 - tempoReferencia[1])); tempo+= 500) {
      arquivoCsv += timeStamp(new Date(tempo));
      arquivoCsv = arquivoCsv.slice(0, -1);
    }
    return arquivoCsv;
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

  const styles = StyleSheet.create({
    container: {
      flex: 1,
      justifyContent: "space-between",
      backgroundColor: "#fafafa",
    },
    botoesCaptura: {
      backgroundColor: "#bbcfff",
      marginVertical: 2}
  })

  function Texto(props) {
    return (
      <Card style={{width: "80%", alignSelf: "center"}}>
        <Card.Content style={{backgroundColor: "#a6b1e1"}}>
          <View style={{ flex: 1, alignSelf: "center", justifyContent: "center", paddingVertical: 40, alignItems: "center"}}>
            <Text style={{fontWeight: "700"}}>IP Monitorado</Text>
            <Text>{ipParaCaptura}</Text>
          </View>
        </Card.Content>
      </Card>
    )
  };

  function MudarIp() {
    let ipAtual;
    return (
      <View>
        <Card>
          <Card.Content>
            <TextInput placeholder="http://192.168.25.17:5000/i_rms_data" mode='outlined' disabled={leituraEmAndamento} onChangeText={texto => { ipAtual = texto }}></TextInput>
          </Card.Content>
          <Card.Actions style={{ alignSelf: "center" }}>
            <Button disabled={leituraEmAndamento} mode="outlined" style={{backgroundColor: "#f4eeff"}}onPress={() => {if (ipAtual) setIpParaCaptura(ipAtual)}}> Mudar a URL</Button>
          </Card.Actions>
        </Card>
      </View>
    )
  }

  return (
    <View style={styles.container}>
      <MudarIp></MudarIp>
      <Texto></Texto>
      <Card>
        <Card.Content>
          <Card.Actions>
            <View style={styles.container}>
              <View style={{ flex: 1, alignSelf: "center", justifyContent: "center", paddingVertical: 40, alignItems: "center", flexDirection: "row"}}>
                <Text>Estado da Captura</Text>
                <Switch
                  value={leituraEmAndamento}
                  onValueChange={()=> verificarEstado()}
                />
              </View>
              <Button mode="outlined" style={styles.botoesCaptura} onPress={() => exportCsv()}>Exportar .CSV</Button>
            </View>
          </Card.Actions>
        </Card.Content>
      </Card>
    </View>
  );
}
