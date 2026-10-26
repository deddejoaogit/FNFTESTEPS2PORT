@echo off
rem ===========================================================================
rem  FNF PS2 - compilar e mandar para o pendrive, com um clique.
rem
rem  O que este arquivo faz:
rem    1. acha o WSL (Ubuntu-22.04);
rem    2. acha o pendrive de verdade, perguntando ao Windows qual e' o
rem       volume REMOVIVEL (nao adivinha letra);
rem    3. compila dentro do WSL e exporta o jogo completo para build-out\;
rem    4. copia build-out\ para <pendrive>:\FNFPS2\ com robocopy.
rem
rem  POR QUE A COPIA E' FEITA PELO WINDOWS E NAO PELO WSL
rem    O WSL so enxerga uma unidade do Windows se o Windows a montar dentro
rem    dele. Quando isso nao acontece, o Linux nao consegue gravar no pendrive
rem    -- e a versao anterior deste pacote, ao tentar mesmo assim, acabou
rem    gravando o jogo em C:\FNFPS2. O cmd.exe/robocopy nao tem esse problema:
rem    se o Explorador do Windows consegue gravar no pendrive, o robocopy
rem    tambem consegue. Por isso o build acontece no Linux e a copia aqui.
rem
rem  Este arquivo e' ASCII puro e com fim de linha CRLF de proposito: o cmd.exe
rem  le mal acentos e le mal LF sozinho, e um .bat embaralhado falha de um jeito
rem  que nao tem nada a ver com o codigo.
rem ===========================================================================

setlocal EnableExtensions
title FNF PS2 - compilar e enviar para o pendrive

rem ---- ajustes, se o seu PC for diferente -----------------------------------
set "DISTRO=Ubuntu-22.04"
rem  Deixe FORCAR vazio para o pendrive ser detectado sozinho. Se a deteccao
rem  errar, ponha a letra aqui, por exemplo:  set "FORCAR=G:"
set "FORCAR="
set "COMPACKS="
rem  Para levar tambem os packs de personagem/palco (110 MB, ainda nao lidos
rem  pelo jogo), troque a linha acima por:  set "COMPACKS=--com-packs"
rem ---------------------------------------------------------------------------

set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"
set "SAIDA=%SRC%\build-out"

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

echo  [2/5] procurando o pendrive...
if defined FORCAR (
    set "LETRA=%FORCAR%"
    echo        forcado pela linha FORCAR=: %FORCAR%
) else (
    call :achar_pendrive
)
if not defined LETRA goto :sem_pendrive
if not exist "%LETRA%\" goto :sem_pendrive
echo        unidade removivel: %LETRA%
for /f "usebackq delims=" %%v in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Volume -DriveLetter %LETRA:~0,1%).FileSystemLabel" 2^>nul`) do echo        rotulo: %%v

echo  [3/5] convertendo o caminho do projeto...
set "SRCLINUX="
for /f "usebackq delims=" %%i in (`wsl -d %DISTRO% -- wslpath -a "%SRC%"`) do set "SRCLINUX=%%i"
if not defined SRCLINUX goto :sem_wslpath
echo        %SRCLINUX%

echo  [4/5] compilando dentro do WSL (pode levar alguns minutos)...
echo.
rem  O caminho ja' e' Linux, entao vai entre aspas simples para o bash, dentro
rem  das aspas duplas que o cmd.exe entende. Nao uso --cd porque nem toda
rem  versao do WSL o aceita.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && ./tools/compilar_windows.sh --no-pendrive --stage-out '%SRCLINUX%/build-out' %COMPACKS%"
if errorlevel 1 goto :falhou

if not exist "%SAIDA%\FNF_PS2.ELF" goto :sem_stage

echo  [5/5] copiando para %LETRA%\FNFPS2 ...
rem  /E copia subpastas e NAO apaga o que ja' existe no destino (sem /PURGE):
rem  os assets de runtime que moram no pendrive continuam la'.
rem  Codigos de saida do robocopy: 0 a 7 = certo, 8 ou mais = falhou.
if not exist "%LETRA%\FNFPS2\" md "%LETRA%\FNFPS2" >nul 2>&1
robocopy "%SAIDA%" "%LETRA%\FNFPS2" /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP /XF build.log build-tests.log
if errorlevel 8 goto :copia_falhou

if not exist "%LETRA%\FNFPS2\FNF_PS2.ELF" goto :copia_falhou
for %%F in ("%LETRA%\FNFPS2\FNF_PS2.ELF") do set "TAMELF=%%~zF"
set "CHARTS=0"
for /f %%c in ('dir /b /s "%LETRA%\FNFPS2\assets\songs\*.chart" 2^>nul ^| find /c /v ""') do set "CHARTS=%%c"

echo        ok  FNF_PS2.ELF (%TAMELF% bytes)
echo        ok  %CHARTS% charts em %LETRA%\FNFPS2\assets\songs\

rem  Os assets de runtime (fonte, audio do menu, cache) nao estao nos
rem  fontes: o build so' os leva junto se achar uma pasta FNFPS2\assets
rem  no PC. Sem eles o jogo roda, mas as telas que os usam ficam pretas.
set "FALTANDO="
for %%A in (font.rgba font.widths freakyMenu_48000.pcm audio_stems.pcm title_alphabet.rgba menu_cache.rle) do (
    if not exist "%LETRA%\FNFPS2\assets\%%A" (
        echo        aviso  falta assets\%%A
        set "FALTANDO=1"
    )
)
if defined FALTANDO (
    echo.
    echo        ATENCAO: faltam assets de runtime no pendrive.
    echo        O jogo abre, mas as telas que usam esses arquivos ficam
    echo        pretas. Se existir a pasta C:\FNFPS2\assets neste PC,
    echo        copie o conteudo dela para %LETRA%\FNFPS2\assets.
)
echo.
echo  =========================================================
echo   TERMINOU. O jogo completo esta' em %LETRA%\FNFPS2\
echo  =========================================================
echo.
echo   No PS2: uLaunchELF -> mass:/FNFPS2/FNF_PS2.ELF
echo.
echo   Se travar, mande os dois arquivos que o jogo grava la':
echo       mass:/FNFPS2/BOOT.txt         (ate onde o boot chegou)
echo       mass:/FNFPS2/FNF_COLHEITA.txt (o log por quadro)
echo.
pause
exit /b 0

rem ---------------------------------------------------------------------------
rem  Acha o pendrive: pergunta ao Windows qual e' o volume removivel.
rem  1o PowerShell Get-Volume; se nao houver, fsutil por letra, de D: a Z:.
rem  O disco do sistema nunca entra: ele nao e' DriveType Removable.
rem ---------------------------------------------------------------------------
:achar_pendrive
for /f "usebackq delims=" %%d in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Volume ^| Where-Object { $_.DriveType -eq 'Removable' -and $_.DriveLetter } ^| Select-Object -First 1 -ExpandProperty DriveLetter" 2^>nul`) do set "LETRA=%%d:"
if defined LETRA exit /b 0
for %%L in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if not defined LETRA (
        for /f "usebackq delims=" %%t in (`fsutil fsinfo drivetype %%L: 2^>nul ^| findstr /i "Removable"`) do set "LETRA=%%L:"
    )
)
exit /b 0

:sem_pendrive
echo.
echo  =========================================================
echo   NAO ACHEI O PENDRIVE
echo  =========================================================
echo.
echo   Nenhum volume removivel apareceu. Confira:
echo     - o pendrive esta conectado e aparece no Explorador?
echo     - ele nao esta' com a trava fisica de escrita ligada?
echo.
echo   Se ele aparecer com outra letra, ou se for um HD externo
echo   (que o Windows nao chama de removivel), edite este arquivo:
echo       set "FORCAR=F:"
echo.
echo   Nada foi compilado. Nada foi gravado no disco C:.
echo.
pause
exit /b 3

:sem_stage
echo.
echo   A compilacao terminou mas build-out\FNF_PS2.ELF nao existe.
echo   Isso e' um erro do script, nao do seu PC. Mande esta tela.
echo.
pause
exit /b 4

:copia_falhou
echo.
echo  =========================================================
echo   A COPIA PARA O PENDRIVE FALHOU
echo  =========================================================
echo.
echo   O jogo compilou e esta' pronto em:
echo       %SAIDA%
echo   Copie essa pasta a mao para %LETRA%\FNFPS2 no Explorador.
echo.
echo   Se o Windows nao deixar gravar, o pendrive pode estar
echo   protegido contra escrita (trava fisica) ou cheio.
echo.
pause
exit /b 5

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
