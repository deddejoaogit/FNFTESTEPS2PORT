# 0.2.161 — espera explícita pelo pendrive no boot

## A causa que esta build ataca

A PROVA 1 rodou no console e devolveu, da esquerda para a direita:

`verde, verde, verde, VERMELHO, verde, verde, VERMELHO, verde, verde`

As barras são, por ordem: 1 IOP, 2 módulos USB, 3 vsync, 4 VRAM, 5 RAM,
6 banda de RAM, 7 **leitura do pendrive**, 8 áudio, 9 controle.

O vermelho 4 é **falso negativo do programa de teste** (media VRAM com um
framebuffer de 640×448 já alocado e exigia 4 MiB inteiros). A PROVA 2 mede
com `graph_vram_clear()` antes e confere o layout real do jogo.

O vermelho 7 é o que importa: a leitura do pendrive foi testada **imediatamente
depois do `SifIopReset`**, e o reset desmonta o `mass:` — o usbd/usbhdfsd que
vinha do lançador deixa de existir e o volume só reaparece depois de o driver
ser recarregado e o dispositivo enumerado de novo. Esse tempo não é zero.

O jogo fazia exatamente isso: `reset_iop()` → `telemetry_init()` (que carrega
os IRX e tenta abrir o log com repetição) → `load_metadata()`, que lê
`mass:/FNFPS2/assets/font.rgba`. Se o volume ainda não estivesse montado,
`load_metadata()` falhava e a build morria antes de desenhar — **tela preta**,
que é o sintoma relatado.

## O que muda

Uma função nova, `espera_pendrive()`, chamada entre `telemetry_init()` e
`load_metadata()`:

| tempo | ação |
|---|---|
| 0 s | tenta `open("mass:/FNFPS2/assets/font.rgba")` e `open("mass0:/…")` a cada 250 ms |
| 5 s | recarrega `usbd` + `usbhdfsd` (`SifExecModuleBuffer`), marca `USB_RECARGA 1` |
| 12 s | recarrega outra vez, marca `USB_RECARGA 2` |
| 25 s | desiste e chama `boot_fail()` com a mensagem `PENDRIVE` |

Detalhes que valem registo:

- A sonda é um **asset de verdade**, não o diretório: `opendir` num volume
  recém-montado pode responder antes de o FAT estar legível.
- `mass0:` é tentado porque alguns lançadores montam com esse nome.
- A recarga é feita com os IRX **embutidos no ELF** (`src/usbd_irx.c`,
  `src/usbhdfsd_irx.c`), os mesmos que o telemetry já carrega — recarregar um
  módulo já residente é seguro no IOP e resolve o estado deixado pelo lançador.
- O código não espera nada quando o pendrive já responde: no caminho normal a
  função custa **um `open`+`close`**.
- A espera máxima foi fixada em 25 s (a PROVA 2 usa o mesmo valor) porque abaixo
  disso o console do usuário não deu sinal de montagem.

## O que esta build prova e o que não prova

- **Provado**: compila sem erros; cartão estático com 9 itens e 0 fora do
  limite (RAM 22,66 MB de 32; VRAM 1,73 MB de 4; 0 escritas bloqueantes por
  quadro); a versão `0.2.161` está dentro do ELF; o caminho de falha termina em
  `boot_fail()` com tela vermelha e `FNF_ERRO.txt` — não em tela preta.
- **Hipótese**: a tela preta das builds anteriores era o `mass:` a montar
  depois do reset do IOP. É a única causa que a PROVA 1 isolou com vermelho.
- **Falta**: o console dizer se o `USB_PRONTO` chegou no primeiro segundo ou
  depois de uma recarga (o valor vai para a telemetria e para a tela de falha),
  e a PROVA 2 medir se `mass:` responde no tempo que o jogo precisa.
