import React, { useState, useEffect, Component } from 'react';
import { View, StyleSheet } from 'react-native';
import axios from 'react-native-axios';
import RNFetchBlob from 'rn-fetch-blob';
import Share from 'react-native-share';
import { Button, Card, TextInput, Text } from 'react-native-paper';


export default function YourApp() {
  const [valoresRms, setValoresRms] = useState([]);
  const [valoresDeTempo, setValoresDeTempo] = useState([]);
  const [idIntervalo, setIdIntervalo] = useState();
  const [ipParaCaptura, setIpParaCaptura] = useState('http://192.168.25.17:5000/i_rms_data');
  const [leituraEmAndamento, setLeituraEmAndamento] = useState(false);

  const startRmsCapture = () => {
    if (!leituraEmAndamento) {
      setIdIntervalo(setInterval(() => {
        axios.get(ipParaCaptura)
          .then((result) => {
            let resposta = result.data;
            let tamanhoVetor;
            let rmsAux = []
            let tempoAux = []

            resposta = resposta.toString().split(" ");

            rmsAux = resposta[0].split(",");
            tamanhoVetor = Number(resposta[1]);
            
            
            rmsAux = rmsAux.slice(0, tamanhoVetor);
            rmsAux.forEach((element, index) => {
              valoresRms.push(element);
              tempoAux.push(500 * index);
            });

            var deslocar;
            valoresDeTempo.length ? deslocar = valoresDeTempo[valoresDeTempo.length -1] + 500 : deslocar = 0;

            tempoAux = tempoAux.map(x => x + deslocar);
            tempoAux.map(x => valoresDeTempo.push(x));
            
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

  const exportCsv = () => {
    var dirs = RNFetchBlob.fs.dirs
    const lugar = dirs.DocumentDir + '/Rms.csv';
    RNFetchBlob.fs
      .writeFile(lugar, montarCsv(), 'utf8')
      .then(() => {
        RNFetchBlob.fs
          .readFile(lugar, 'utf-8')
          .then((data) => {
            Share.open({ url: `file://${lugar}` })
          })
          .catch((error) => console.log(error));
      })
  }

  const montarCsv = () => {
    let csvRms, csvTempo, arquivoCsv;
    csvRms = valoresRms.toString();
    csvTempo = valoresDeTempo.toString();
    arquivoCsv = 'Rms,' + csvRms + '\n' + 'Tempo,' + csvTempo;
    return arquivoCsv;
  }

  const styles = StyleSheet.create({
    container: {
      flex: 1,
      justifyContent: "space-between",
      backgroundColor: "#fff",
    },
    botoesCaptura: {
      backgroundColor: "#f4eeff",
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
            <Button mode="outlined" style={{backgroundColor: "#f4eeff"}}onPress={() => setIpParaCaptura(ipAtual)}> Mudar o Ip</Button>
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
              <Button mode="outlined" style={styles.botoesCaptura} onPress={() => startRmsCapture()}>Start</Button>
              <Button mode="outlined" style={styles.botoesCaptura} onPress={() => stopRmsCapture()}>Stop</Button>
              <Button mode="outlined" style={styles.botoesCaptura} onPress={() => exportCsv()}>Exportar .CSV</Button>
            </View>
          </Card.Actions>
        </Card.Content>
      </Card>
    </View>
  );
}
