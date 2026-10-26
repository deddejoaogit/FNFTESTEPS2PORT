@echo off
rem ===========================================================================
rem  FNF PS2 - compilar e mandar para o pendrive, com um clique.
rem
rem  O que este arquivo faz:
rem    1. acha o WSL (Ubuntu);
rem    2. roda tools/compilar_windows.sh dentro do Linux, que compila;
rem    3. copia o resultado para F:\FNFPS2\ (a letra do pendrive pode ser
rem       trocada na linha PENDRIVE= abaixo).
rem
rem  Este arquivo e' ASCII puro e com fim de linha CRLF de proposito: o cmd.exe
rem  le mal acentos e le mal LF sozinho, e um .bat embaralhado falha de um jeito
rem  que nao tem nada a ver com o codigo.
rem ===========================================================================

setlocal EnableExtensions
title FNF PS2 - compilar e enviar para o pendrive

rem ---- ajustes, se o seu PC for diferente -----------------------------------
set "DISTRO=Ubuntu-22.04"
set "PENDRIVE=/mnt/f"
set "COMPACKS="
rem  Para levar tambem os packs de personagem/palco (110 MB, ainda nao lidos
rem  pelo jogo), troque a linha acima por:  set "COMPACKS=--com-packs"
rem ---------------------------------------------------------------------------

set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"

echo.
echo  =========================================================
echo   FNF PS2 - compilar e enviar para o pendrive
echo  =========================================================
echo   projeto: %SRC%
echo.

where wsl >nul 2>&1
if errorlevel 1 goto :sem_wsl

echo  [1/3] conferindo o WSL (%DISTRO%)...
wsl -d %DISTRO% -- true >nul 2>&1
if errorlevel 1 goto :distro_errada
echo        ok

echo  [2/3] convertendo o caminho do projeto...
set "SRCLINUX="
for /f "usebackq delims=" %%i in (`wsl -d %DISTRO% -- wslpath -a "%SRC%"`) do set "SRCLINUX=%%i"
if not defined SRCLINUX goto :sem_wslpath
echo        %SRCLINUX%

echo  [3/3] compilando dentro do WSL (pode levar alguns minutos)...
echo.
rem  O caminho ja' e' Linux, entao vai entre aspas simples para o bash, dentro
rem  das aspas duplas que o cmd.exe entende. Nao uso --cd porque nem toda
rem  versao do WSL o aceita.
wsl -d %DISTRO% -- bash -c "cd '%SRCLINUX%' && ./tools/compilar_windows.sh --pendrive %PENDRIVE% %COMPACKS%"
if errorlevel 1 goto :falhou

echo.
echo  =========================================================
echo   TERMINOU. O jogo esta' no pendrive em F:\FNFPS2\
echo  =========================================================
echo.
echo   No PS2: uLaunchELF -^> mass:/FNFPS2/FNF_PS2.ELF
echo.
pause
exit /b 0

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
