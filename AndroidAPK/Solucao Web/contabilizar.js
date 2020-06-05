var ultimoValorDoContador = 8
var somaTotal = 256

var a = [36,67,97,126,156,186,216,246,20,50,80,110,140]

a.forEach((proximaPosicao, index) => {
    let delta;
    if (proximaPosicao > ultimoValorDoContador) {
        delta = proximaPosicao - ultimoValorDoContador
    }
    else {
        delta = 256 - ultimoValorDoContador + proximaPosicao
    }

    ultimoValorDoContador = proximaPosicao;
    somaTotal += delta
})

console.log(somaTotal);