@echo off
rem ===========================================================================
rem  FNF PS2 - compilar e mandar para o pendrive, com um clique.
rem
rem  DIVISAO DE TRABALHO
rem    Tudo que e' do Ubuntu e' feito no Ubuntu, em bash: achar o toolchain,
rem    conferir espaco, achar o pendrive, compilar, copiar e conferir arquivo
rem    por arquivo.
rem    Tudo que e' do Windows e' feito aqui: reiniciar o WSL quando preciso,
rem    achar a letra da unidade e, em ultimo caso, copiar com robocopy.
rem
rem  POR QUE O WSL NAO VE O PENDRIVE (e o que este arquivo faz)
rem    O WSL monta as unidades do Windows UMA VEZ, quando ele inicia. Pendrive
rem    conectado depois disso nao aparece em /mnt. A correcao e' reiniciar o
rem    WSL com o pendrive ja' conectado -- este arquivo faz isso sozinho antes
rem    de desistir, e so' entao cai para a copia feita pelo Windows.
rem
rem  Este arquivo e' ASCII puro e com fim de linha CRLF de proposito: o cmd.exe
rem  le mal acentos e le mal LF sozinho.
rem ===========================================================================

setlocal EnableExtensions EnableDelayedExpansion
title FNF PS2 - compilar e enviar para o pendrive
cd /d "%~dp0"

rem ---- ajustes, se o seu PC for diferente -----------------------------------
set "DISTRO=Ubuntu-22.04"
rem  Deixe FORCAR vazio: a letra e' descoberta sozinha. Se quiser fixar, ponha
rem  a letra aqui, por exemplo:  set "FORCAR=F:"
set "FORCAR="
rem  Para levar tambem os packs de personagem/palco (110 MB, ainda nao lidos
rem  pelo jogo):  set "COMPACKS=--com-packs"
set "COMPACKS="
rem ---------------------------------------------------------------------------

set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"
set "DESTINO_ARQ=%SRC%\.fnf_destino"

echo.
echo  =========================================================
echo   FNF PS2 - compilar e enviar para o pendrive
echo  =========================================================
echo   projeto: %SRC%
echo.

where wsl >nul 2>&1
if errorlevel 1 goto :sem_wsl

echo  [1/5] conferindo o WSL (%DISTRO%)...
wsl -d %DISTRO% -- true >nul 2>&1
if errorlevel 1 goto :distro_errada
echo        ok

set "SRCLINUX="
for /f "usebackq delims=" %%i in (`wsl -d %DISTRO% -- wslpath -a "%SRC%"`) do set "SRCLINUX=%%i"
if not defined SRCLINUX goto :sem_wslpath

echo  [2/5] conferindo o ambiente no Ubuntu...
echo.
call :verificar
if "%RC%"=="0" goto :caminho_ubuntu
if not "%RC%"=="6" goto :falhou

rem ---------------------------------------------------------------------------
rem  O Ubuntu nao ve nenhuma unidade gravavel. Quase sempre e' o WSL que subiu
rem  antes do pendrive ser conectado. Reiniciar o WSL com o pendrive conectado
rem  resolve, porque a montagem acontece na inicializacao. Uma tentativa, cara
rem  e' so' alguns segundos.
rem ---------------------------------------------------------------------------
echo.
echo  Nenhuma unidade gravavel no Ubuntu. Tentando reiniciar o WSL com o
echo  pendrive conectado (a montagem das unidades acontece na inicializacao)...
wsl --shutdown >nul 2>&1
timeout /t 3 /nobreak >nul 2>&1
call :verificar
if "%RC%"=="0" goto :caminho_ubuntu
if not "%RC%"=="6" goto :falhou

echo.
echo  Continuo sem ver o pendrive pelo Ubuntu. Seguindo pelo lado do Windows,
echo  que nao depende de montagem nenhuma.
goto :caminho_windows

rem ===========================================================================
rem  CAMINHO A: o Ubuntu ve e grava no pendrive. Tudo feito no Linux.
rem ===========================================================================
:caminho_ubuntu
set "MODO=linux"
echo.
echo  [3/5] compilando e copiando dentro do Ubuntu (pode levar alguns minutos)...
echo.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh %COMPACKS%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto :falhou
goto :pos_build

rem ===========================================================================
rem  CAMINHO B: o Ubuntu nao ve o pendrive. Compila no Linux, copia no Windows.
rem ===========================================================================
:caminho_windows
set "MODO=windows"
echo.
echo  [3/5] compilando dentro do Ubuntu (pode levar alguns minutos)...
echo.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh --no-pendrive %COMPACKS%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto :falhou
goto :pos_build

rem ===========================================================================
rem  Comum aos dois caminhos: pega o destino e confere.
rem ===========================================================================
:pos_build
echo  [4/5] localizando o destino...
call :ler_destino
if not defined DEST goto :sem_destino
rem  O caminho do Ubuntu vem como \\wsl.localhost\<distro>\... ; em WSL1 o mesmo
rem  conteudo aparece em \\wsl$\<distro>\... . Troca o prefixo so' se preciso.
if not exist "%DEST%\" set "DEST=%DEST:\\wsl.localhost\=\\wsl$\%"
if not exist "%DEST%\" goto :sem_destino

if "%MODO%"=="linux" goto :so_conferir

echo  [5/5] procurando a unidade do pendrive no Windows...
call :achar_letra
if not defined LETRA goto :sem_pendrive
echo        unidade: %LETRA%

echo  [5/5] copiando para %LETRA%\FNFPS2 ...
rem  /E copia subpastas e NAO apaga o que ja' existe (sem /PURGE): os assets
rem  de runtime que moram no pendrive continuam la'.
rem  Codigos do robocopy: 0 a 7 = certo, 8 ou mais = falhou.
if not exist "%LETRA%\FNFPS2\" md "%LETRA%\FNFPS2" >nul 2>&1
robocopy "%DEST%" "%LETRA%\FNFPS2" /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP /XF build.log build-tests.log
if errorlevel 8 goto :copia_falhou
set "DEST=%LETRA%\FNFPS2"

:so_conferir
echo  conferindo o que ficou em %DEST%
call :conferir "%DEST%"
if errorlevel 1 goto :copia_incompleta
goto :fim_ok

rem ===========================================================================
rem  Sub-rotinas
rem ===========================================================================

rem  Roda a conferencia de ambiente do Ubuntu. Devolve RC:
rem    0 = tudo pronto (o Ubuntu ve e grava no pendrive)
rem    6 = o Ubuntu nao ve nenhuma unidade gravavel (o .bat tenta o Windows)
rem    outro = falha de verdade (toolchain, arquivos do build)
:verificar
set "RC=0"
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh --verificar"
set "RC=%ERRORLEVEL%"
exit /b 0

rem  Le .fnf_destino, escrito pelo script do Ubuntu.
rem  Formato: MODO|CAMINHO   (CAMINHO e' sempre um caminho do Windows)
:ler_destino
set "DEST="
if not exist "%DESTINO_ARQ%" exit /b 0
for /f "usebackq tokens=1,2 delims=|" %%a in ("%DESTINO_ARQ%") do (
    set "MODO_ARQ=%%a"
    set "DEST=%%b"
)
exit /b 0

rem  Acha a letra do pendrive. Tres tentativas, da mais segura para a mais
rem  frouxa, porque "Removable" e' um palpite do Windows e alguns pendrives e
rem  HDs externos se anunciam como disco fixo:
rem    1) unidade que ja' tem a pasta FNFPS2 (e' a que o jogo usa);
rem    2) unidade que o Windows chama de Removable;
rem    3) qualquer unidade gravavel que nao seja o disco do sistema.
rem  Se nada servir, lista o que existe e pergunta a letra.
:achar_letra
set "LETRA="
if defined FORCAR (
    set "LETRA=%FORCAR%"
    if not exist "%LETRA%\" set "LETRA="
    rem  Nem a escolha explicita pode ser o disco do sistema.
    if /i "%FORCAR%"=="%SystemDrive%" (
        echo        FORCAR aponta para o disco do sistema ^(%SystemDrive%^); ignorado.
        set "LETRA="
    )
    if defined LETRA exit /b 0
)
set "PRIMEIRA_REMOVIVEL="
set "OUTRA="
for /f "usebackq tokens=1,2 delims=|" %%a in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=Get-Volume ^| Where-Object { $_.DriveLetter -and $_.DriveType -ne 'CDRom' }; foreach($v in $s){ $v.DriveLetter + '|' + $v.DriveType }" 2^>nul`) do (
    if exist "%%a:\FNFPS2\" if not defined LETRA set "LETRA=%%a:"
    if /i "%%b"=="Removable" if not defined PRIMEIRA_REMOVIVEL set "PRIMEIRA_REMOVIVEL=%%a:"
    rem  A terceira tentativa NUNCA pode ser o disco do sistema: a raiz do
    rem  Windows tem Windows\ e pagefile.sys, e o disco de sistema e' o
    rem  %%SystemDrive%%. Sem estas duas checagens, "qualquer unidade" poderia
    rem  escolher C: e repetir o erro que gravou o jogo no disco errado.
    if not defined OUTRA if /i not "%%a:"=="%SystemDrive%" if not exist "%%a:\Windows\" if not exist "%%a:\pagefile.sys" set "OUTRA=%%a:"
)
if defined LETRA exit /b 0
if defined PRIMEIRA_REMOVIVEL (
    set "LETRA=%PRIMEIRA_REMOVIVEL%"
    exit /b 0
)
if defined OUTRA (
    set "LETRA=%OUTRA%"
    exit /b 0
)
call :listar_unidades
echo.
echo  Nenhuma unidade foi aceita automaticamente.
set /p "LETRA=Digite a letra do pendrive (ex: F) e tecle Enter: "
if not defined LETRA exit /b 0
if not exist "%LETRA%:\" set "LETRA="
exit /b 0

rem  Mostra as unidades que o Windows tem, com tipo, rotulo e espaco livre.
rem  Uma unica chamada ao PowerShell, so' neste caso.
:listar_unidades
echo        unidades que o Windows ve:
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Volume | Where-Object { $_.DriveLetter } | Sort-Object DriveLetter | ForEach-Object { '         ' + $_.DriveLetter + ':  ' + $_.DriveType + '  ' + $_.FileSystemLabel + '  ' + [math]::Round($_.SizeRemaining/1MB) + ' MB livres' }" 2>nul
exit /b 0

rem  Confere o que ficou no pendrive. Recebe a pasta em %1.
rem  Devolve errorlevel 1 se algo essencial faltar.
:conferir
set "FALHAS=0"
if not exist "%~1\FNF_PS2.ELF" (
    echo        ERRO  falta FNF_PS2.ELF
    set "FALHAS=1"
) else (
    for %%F in ("%~1\FNF_PS2.ELF") do echo        ok  FNF_PS2.ELF -- %%~zF bytes
)
set "CHARTS=0"
for /f %%c in ('dir /b /s "%~1\assets\songs\*.chart" 2^>nul ^| find /c /v ""') do set "CHARTS=%%c"
if "%CHARTS%"=="0" (
    echo        ERRO  nenhum chart copiado
    set "FALHAS=1"
) else (
    echo        ok  %CHARTS% charts em assets\songs\
)
set "FALTA_ASSET="
for %%A in (font.rgba font.widths freakyMenu_48000.pcm audio_stems.pcm title_alphabet.rgba menu_cache.rle) do (
    if not exist "%~1\assets\%%A" (
        echo        aviso  falta assets\%%A
        set "FALTA_ASSET=1"
    )
)
if defined FALTA_ASSET (
    echo.
    echo        ATENCAO: faltam assets de runtime. O jogo abre, mas as telas
    echo        que usam esses arquivos ficam pretas.
)
if "%FALHAS%"=="1" exit /b 1
exit /b 0

rem ===========================================================================
rem  Fim e mensagens de erro
rem ===========================================================================

:fim_ok
echo  [5/5] pronto.
echo.
echo  =========================================================
echo   TERMINOU. O jogo completo esta' em %DEST%
echo  =========================================================
echo.
echo   No PS2: uLaunchELF -^> mass:/FNFPS2/FNF_PS2.ELF
echo.
echo   Se travar, mande os dois arquivos que o jogo grava la':
echo       mass:/FNFPS2/BOOT.txt         ate onde o boot chegou
echo       mass:/FNFPS2/FNF_COLHEITA.txt o log quadro a quadro
echo.
pause
exit /b 0

:sem_destino
echo.
echo  =========================================================
echo   NAO SEI ONDE O JOGO FOI PARAR
echo  =========================================================
echo.
echo   O script do Ubuntu disse que copiou, mas nao deixou o
echo   bilhete de destino (.fnf_destino). Isso e' um erro do
echo   pacote, nao do seu PC. Mande esta tela inteira.
echo.
pause
exit /b 6

:sem_pendrive
echo.
echo  =========================================================
echo   NAO ACHEI O PENDRIVE
echo  =========================================================
echo.
echo   O jogo esta' compilado e pronto nesta pasta:
echo       %DEST%
echo.
echo   Copie o conteudo dela (FNF_PS2.ELF e a pasta assets)
echo   para a raiz do pendrive, dentro de uma pasta FNFPS2.
echo   No Explorador, cole este endereco na barra de cima:
echo       %DEST%
echo.
echo   Se o pendrive nao aparece no Explorador, o problema e'
echo   do Windows, nao do jogo: verifique o cabo, a trava de
echo   escrita e o Gerenciador de Dispositivos.
echo.
pause
exit /b 7

:copia_falhou
echo.
echo  =========================================================
echo   A COPIA PARA O PENDRIVE FALHOU
echo  =========================================================
echo.
echo   O jogo compilou e esta' pronto em:
echo       %DEST%
echo.
echo   Copie essa pasta a mao para o pendrive, no Explorador.
echo   Se o Windows nao deixar gravar, o pendrive pode estar
echo   protegido contra escrita ou cheio.
echo.
pause
exit /b 5

:copia_incompleta
echo.
echo  =========================================================
echo   A COPIA NAO CONFERIU
echo  =========================================================
echo.
echo   Faltou arquivo em %DEST%.
echo   Rode o COMPILAR.bat de novo. Se repetir, mande esta tela.
echo.
pause
exit /b 8

:falhou
echo.
echo  =========================================================
echo   A COMPILACAO FALHOU
echo  =========================================================
echo.
echo   O erro completo esta' em:
echo       %SRC%\build.log
echo.
echo   Mande esse arquivo para eu ver o que aconteceu.
echo.
pause
exit /b 1

:distro_errada
echo.
echo   Nao achei a distribuicao "%DISTRO%" no WSL.
echo.
echo   Veja quais voce tem com:
echo       wsl -l -v
echo.
echo   Se o nome for diferente, edite a linha  DISTRO=  neste arquivo.
echo.
pause
exit /b 1

:sem_wslpath
echo.
echo   O WSL respondeu, mas nao consegui converter o caminho do projeto.
echo   Tente atualizar o WSL, no PowerShell como administrador:
echo       wsl --update
echo.
pause
exit /b 1

:sem_wsl
echo.
echo   O WSL nao esta' instalado (ou nao esta' no PATH).
echo.
echo   No PowerShell como administrador:
echo       wsl --install -d Ubuntu-22.04
echo   Depois reinicie o PC e rode este arquivo de novo.
echo.
pause
exit /b 1
