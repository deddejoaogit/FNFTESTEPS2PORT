# REGRAS DO PROJETO

Regras a serem seguidas em cada ação, sem exceção. Elas nasceram de erros reais
cometidos neste projeto — cada uma tem um motivo concreto, e o motivo está
escrito junto.

---

## 1. Nada, nunca, jamais é inventado

Absolutamente nada. Nenhum número, nenhum mecanismo, nenhum comportamento, nenhum
endereço, nenhum nome de função, nenhum resultado de medição.

Tudo o que for afirmado precisa ter uma fonte, e a fonte tem de ser uma destas:

- uma **linha de log** do console, com o quadro e o valor;
- uma **medição** feita sobre um arquivo real (captura, asset, binário);
- um **arquivo do código**, com caminho e número de linha;
- um **fato documentado** do ps2sdk ou do hardware, com referência;
- uma **declaração sua**, registrada tal como foi dita.

Se não houver fonte, a resposta é **"não sei"** — não uma estimativa, não um
"provavelmente", não um número plausível.

**Motivo:** nas builds 0.2.108, 0.2.109 e 0.2.110 eu afirmei três causas
diferentes para o jogo não abrir, todas ditas com convicção, e **nenhuma das
três era a causa real**. Eu tinha dados o tempo todo (`BOOT.txt` dizia o passo
30) e não os usei. Três builds perdidas, e a sua confianza gastada à toa.

---

## 2. Toda mudança tem plano, causa e lógica — escritos antes

Antes de alterar uma linha de código, em qualquer arquivo:

1. **Causa:** o que está errado, provado por dado. Não por sintoma.
2. **Plano:** o que vai ser alterado, em qual arquivo, e o efeito esperado.
3. **Lógica:** por que essa alteração resolve a causa, e não outra coisa.
4. **Verificação:** como se vai provar que resolveu, antes de entregar.

Se algum dos quatro não puder ser escrito, a mudança não está pronta para ser
feita.

**Motivo:** cada uma das três regressões veio de uma alteração feita "porque
parecia certa", sem plano escrito, e cada uma quebrou algo que funcionava.

---

## 3. O teste local tem de cobrir exatamente o que se afirma

Não se pode dizer "verificado" sobre algo que o teste não exercita.

Antes de afirmar que uma build funciona, é preciso saber **o que o teste local
finge** e **o que ele não finge**. Se o teste não cobre um caminho, isso é dito
explicitamente, e o caminho fica marcado como não verificado.

**Motivo:** o harness de E/S finge o `write()` e o `fsync()` do PS2 — no PC são
instantâneos. Eu corrigi um problema de E/S no pendrive, rodei o harness, vi
"ALL SUITES PASSED" e afirmei que estava verificado. O harness nunca podia pegar
aquele problema, porque no PC `fsync()` não custa nada. Três builds quebradas
foram enviadas com o selo "verificado".

**Regra derivada:** quando a correção é sobre I/O, tempo, sincronização ou
hardware, o harness local **não** é prova. A prova vem do console.

---

## 4. Um sintoma sem dado não é um diagnóstico

"O jogo não abriu" pode significar: tela preta, tela congelada, jogo rodando sem
resposta, launcher que não achou o ELF, pasta no lugar errado, assets ausentes.

Nunca se escolhe uma causa e se conserta às cegas. Pede-se o dado que distingue
as causas — e enquanto ele não chega, a build entregue tem de **coletar** esse
dado, não adivinhar.

**Motivo:** gastei três builds tentando três causas diferentes para o mesmo
sintoma. A quarta build só acertou porque passou a coletar o dado
(`BOOT.txt`), e o dado apontou a causa na hora.

**Regra derivada:** se não há dado, a próxima build serve para **obter** dado —
não para tentar um conserto.

---

## 5. Nenhuma regressão, nunca

Uma versão entregue tem de funcionar. Se algo que funcionava quebrou, isso é o
pior resultado possível — pior que uma função nova que não existe.

Antes de entregar: comparar o comportamento da build nova com o da build anterior
nos caminhos que já funcionavam.

**Motivo:** regra sua, e ela foi violada três vezes seguidas. A 0.2.106 funcionava
no console; a 0.2.108, 0.2.109 e 0.2.110 não abriram. Cada uma dessas builds
custou mais que qualquer função nova.

---

## 6. Nenhuma build é entregue sem compilar e passar a suite

Ordem obrigatória, sem pular etapa:

1. compilar **sem warnings**;
2. rodar a suite inteira;
3. provar a correção específica (o log, a medição, o número);
4. reconstruir do código commitado e confirmar que o binário é idêntico;
5. só então entregar.

**Motivo:** a 0.2.107 foi escrita e commitada sem nunca ter sido compilada. E em
outra ocasião o ELF entregue veio de uma compilação diferente da que foi testada.

---

## 7. O diagnóstico nunca pode travar o jogo

Log, marcador, captura, telemetria: nenhum deles pode impedir o jogo de rodar.

Regras concretas:

- escrita em volume **acumula**, não escreve por item;
- `fsync` tem frequência limitada, nunca por quadro;
- falha de escrita **desliga o log**, não trava o jogo;
- `write()` retornando zero é falha, não "tente de novo" — senão o laço não termina;
- espera de dispositivo é limitada, para não virar tela preta de 20 segundos.

**Motivo:** o log foi a causa de três builds não abrirem. Um instrumento de
diagnóstico que quebra o que ele deveria diagnosticar é pior que não existir.

---

## 8. Mudança mínima, e uma coisa por vez

Uma build corrige **uma** coisa, ou um conjunto de coisas de uma mesma causa.
Nunca várias causas diferentes na mesma build.

Se uma build não funciona, a próxima build **reverte** o suspeito antes de tentar
outro — não empilha outra hipótese em cima.

**Motivo:** empilhei três hipóteses em três builds sem reverter nenhuma. Quando
finalmente chegaram os dados, eu tinha três variáveis diferentes e nenhum
controle.

---

## 9. O que não se sabe fica escrito como não se sabe

No fim de cada resposta: o que foi provado, o que foi apenas hipótese, e o que
falta. Sem disso.

**Motivo:** nas três respostas em que eu errei a causa, eu não separei o que
sabia do que supunha. Se tivesse escrito "hipótese, não verificada", você teria
sido poupado de três builds.

---

## 10. Porte exato, nada excluído

Aspecto, tempo de cada animação, posição, proporção, todos os eventos, todo o
código: porte exato do original. A única redução permitida é a textura, para o
tamanho exato que ocupa na tela do PS2, respeitando o ajuste de tamanho de tela
original do jogo.

Qualquer divergência do original precisa de justificativa medida, não estética.

---

## Resumo operacional

| Antes de codar | Escrever causa + plano + lógica + verificação |
| Antes de afirmar | Ter a fonte. Sem fonte, dizer "não sei" |
| Antes de entregar | Compilar limpo, suite, prova da correção, binário idêntico |
| Se não há dado | A build coleta dado, não tenta conserto |
| Se quebrou | Reverter o suspeito antes da próxima hipótese |
| Sempre | Uma build funciona, ou não é build |
