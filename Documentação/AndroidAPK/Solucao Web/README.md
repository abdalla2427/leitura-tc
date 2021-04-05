# leitura-tc

## 1. Bibliotecas e Frameworks utilizados para Constrção da API:

#### Back-End
    1. Node.js
    2. Express.js
    3. Axios
    4. ejs

#### Front-End
    1. HTML5
    2. JavaScript
    3. Canvas.js

## 2. Preparação do ambiente para rodar a API

### 2.1. Instalando o Interprete de JavaScript
* Vamos utilizar o Node.js para interpretar o código da nossa Web API.
> https://nodejs.org/


### 2.2. Clonando o Repositório da API
* Utilizaremos um repositório no  _Github_ para manter as atualizações da aplicação.

> https://github.com/abdalla2427/leitura-tc/tree/master/AndroidAPK/Solucao%20Web

* O código para consultar o microcontrolador encontra-se aqui, junto com o arquivo das dependências necessárias para o funcionamento da API. 

### 2.3. Instalar as dependências do projeto baixado
* Após entrar na pasta baixada do repositório que tem o arquivo 'package.json', abra uma janela no prompt de comando com permissões de administrador, e rode o comando:

>  __npm install__

## 3. Iniciar o serviço da Web API

* Após a instalção das dependências no passo anterior, vamos usar o node para interpretar o script em JavaScript.

* Abra uma janela no prompt de comando com permissões de administrador, e rode o comando:

> __node server.js__


## 4. Acessando o serviço

* Após rodar o comando anterior, um aviso na janela de comando aparecerá, informando que o serviço está rodando na porta 5000 por padrão. Acessando o endereço a seguir por meio de um browser, terá acesso ao painel para fazer a captura dos valores lidos pelo microcontrolador.
> http://localhost:5000

## 5. Funções do serviço

* Alterar o IP Monitorado do Microcontrolador;
* Alterar Tempo entre as requisições feitas ao Microcontrolador;
* Iniciar a Captura;
* Pausar a Captura;
* Exportar o arquivo __.csv__ com os valores das amostras capturadas;
* Acessar o log da Web Api;

O serviço de captura funciona enquanto a janela de comando do item 3 estiver rodando.