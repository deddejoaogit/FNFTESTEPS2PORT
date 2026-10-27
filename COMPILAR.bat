@echo off
rem ===========================================================================
rem  FNF PS2 - compilar e mandar para o pendrive, com um clique.
rem
rem  DIVISAO DE TRABALHO (importante)
rem    Tudo que e' do Ubuntu e' feito no Ubuntu, em bash:
rem        achar o toolchain, conferir espaco, achar o pendrive,
rem        compilar, copiar e conferir arquivo por arquivo.
rem    Tudo que e' do Windows e' feito aqui:
rem        achar a letra da unidade removivel e, se o WSL nao conseguir
rem        gravar no pendrive, copiar com robocopy.
rem    O PowerShell so' aparece se o caminho do Windows for necessario.
rem
rem  POR QUE A COPIA TEM UM PLANO B
rem    O WSL so' ve uma unidade do Windows se o proprio Windows a montar
rem    dentro dele, e isso acontece quando o WSL inicia. Pendrive conectado
rem    depois disso nao aparece. Quando isso ocorre, o jogo e' compilado e
rem    copiado pelo lado do Windows -- que nunca tem esse problema.
rem
rem  Este arquivo e' ASCII puro e com fim de linha CRLF de proposito: o cmd.exe
rem  le mal acentos e le mal LF sozinho.
rem ===========================================================================

setlocal EnableExtensions
title FNF PS2 - compilar e enviar para o pendrive
cd /d "%~dp0"

rem ---- ajustes, se o seu PC for diferente -----------------------------------
set "DISTRO=Ubuntu-22.04"
rem  Deixe FORCAR vazio: a letra e' descoberta sozinha. Se a deteccao errar,
rem  ponha a letra aqui, por exemplo:  set "FORCAR=G:"
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

echo  [2/5] conferindo o ambiente no Ubuntu...
set "SRCLINUX="
for /f "usebackq delims=" %%i in (`wsl -d %DISTRO% -- wslpath -a "%SRC%"`) do set "SRCLINUX=%%i"
if not defined SRCLINUX goto :sem_wslpath
echo        %SRCLINUX%
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh --verificar"
set "RC=%ERRORLEVEL%"
if "%RC%"=="6" goto :via_windows
if not "%RC%"=="0" goto :falhou

echo.
echo  [3/5] compilando dentro do Ubuntu (pode levar alguns minutos)...
echo.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh %COMPACKS%"
set "RC=%ERRORLEVEL%"
if "%RC%"=="6" goto :via_windows
if not "%RC%"=="0" goto :falhou

echo  [4/5] conferindo no pendrive...
call :ler_destino
if not defined DEST goto :sem_destino
if not exist "%DEST%\" goto :sem_destino
call :conferir "%DEST%"
if errorlevel 1 goto :copia_incompleta
goto :fim_ok

rem ---------------------------------------------------------------------------
rem  Plano B: o WSL nao conseguiu montar/gravar o pendrive. Compila e deixa o
rem  layout pronto; a copia e' feita aqui, pelo Windows.
rem ---------------------------------------------------------------------------
:via_windows
echo.
echo  O Ubuntu nao conseguiu gravar no pendrive. Seguindo pelo lado do Windows.
echo  Isso funciona: quem vai gravar e' o proprio Windows, nao o WSL.
echo.
echo  [3b/5] compilando dentro do Ubuntu (pode levar alguns minutos)...
echo.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && sh ./tools/compilar_windows.sh --no-pendrive %COMPACKS%"
if errorlevel 1 goto :falhou

call :ler_destino
if not defined DEST goto :sem_destino
rem  O caminho vem como \\wsl.localhost\<distro>\... . Em WSL1 esse nome nao
rem  existe e o mesmo conteudo aparece em \\wsl$\<distro>\... . Troca so'
rem  o prefixo, so' se o primeiro nao existir. Sem PowerShell.
if not exist "%DEST%\" set "DEST=%DEST:\\wsl.localhost\=\\wsl$\%"
if not exist "%DEST%\" goto :sem_destino

echo  [4b/5] procurando a unidade removivel no Windows...
if defined FORCAR (
    set "LETRA=%FORCAR%"
    echo        forcado pela linha FORCAR=: %FORCAR%
) else (
    call :achar_letra
)
if not defined LETRA goto :sem_pendrive
if not exist "%LETRA%\" goto :sem_pendrive
echo        unidade removivel: %LETRA%

echo  [5b/5] copiando para %LETRA%\FNFPS2 ...
rem  /E copia subpastas e NAO apaga o que ja' existe (sem /PURGE): os assets
rem  de runtime que moram no pendrive continuam la'.
rem  Codigos do robocopy: 0 a 7 = certo, 8 ou mais = falhou.
if not exist "%LETRA%\FNFPS2\" md "%LETRA%\FNFPS2" >nul 2>&1
robocopy "%DEST%" "%LETRA%\FNFPS2" /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP /XF build.log build-tests.log
if errorlevel 8 goto :copia_falhou
set "DEST=%LETRA%\FNFPS2"
call :conferir "%DEST%"
if errorlevel 1 goto :copia_incompleta
goto :fim_ok

rem ---------------------------------------------------------------------------
rem  Le .fnf_destino, escrito pelo script do Ubuntu.
rem  Formato: MODO|CAMINHO   (CAMINHO e' sempre um caminho do Windows)
rem ---------------------------------------------------------------------------
:ler_destino
set "MODO="
set "DEST="
if not exist "%DESTINO_ARQ%" exit /b 0
for /f "usebackq tokens=1,2 delims=|" %%a in ("%DESTINO_ARQ%") do (
    set "MODO=%%a"
    set "DEST=%%b"
)
exit /b 0

rem ---------------------------------------------------------------------------
rem  Confere o que ficou no pendrive. Recebe a pasta em %1.
rem  Devolve errorlevel 1 se algo essencial faltar.
rem ---------------------------------------------------------------------------
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
    echo        que usam esses arquivos ficam pretas. Se existir uma pasta
    echo        FNFPS2\assets em outro disco deste PC, copie o conteudo dela
    echo        para %~1\assets.
)
if "%FALHAS%"=="1" exit /b 1
exit /b 0

rem ---------------------------------------------------------------------------
rem  Acha a unidade removivel. Um unico PowerShell, e so' neste caminho:
rem  a lista de unidades e' informacao do Windows e nao tem traducao de idioma
rem  (o fsutil imprime "Removable Drive" em ingles e outra coisa em portugues).
rem ---------------------------------------------------------------------------
:achar_letra
set "LETRA="
for /f "usebackq delims=" %%d in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Volume ^| Where-Object { $_.DriveType -eq 'Removable' -and $_.DriveLetter } ^| ForEach-Object { $_.DriveLetter + ':' }" 2^>nul`) do (
    if not defined LETRA set "LETRA=%%d"
)
exit /b 0

rem ---------------------------------------------------------------------------

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
echo   O jogo compilou e esta' pronto, mas nenhuma unidade
echo   removivel foi encontrada pelo Windows. Confira:
echo     - o pendrive esta' conectado?
echo     - ele nao esta' com a trava fisica de escrita?
echo.
echo   Se for HD externo (o Windows nao chama de removivel) ou
echo   se estiver em outra letra, edite este arquivo:
echo       set "FORCAR=F:"
echo.
echo   O jogo esta' pronto nesta pasta do WSL, para copiar a mao:
echo       %DEST%
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
