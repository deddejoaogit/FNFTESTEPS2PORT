@echo off
rem ===========================================================================
rem  FNF PS2 - copiar para o pendrive (sem compilar).
rem
rem  Use este arquivo quando o COMPILAR.bat ja' compilou e o jogo esta'
rem  pronto, mas a copia para o pendrive nao aconteceu.
rem
rem  Ele NAO compila nada: pega a pasta pronta e copia com robocopy, que e'
rem  a mesma coisa que arrastar no Explorador. Nada e' apagado no destino.
rem
rem  Uso:  duplo clique, e digite a letra do pendrive quando ele pedir.
rem        ou:  COPIAR-PARA-PENDRIVE.bat F
rem ===========================================================================

setlocal EnableExtensions
title FNF PS2 - copiar para o pendrive
cd /d "%~dp0"

set "DESTINO_ARQ=%~dp0.fnf_destino"
set "LETRA=%~1"
if defined LETRA if not "%LETRA:~1,1%"==":" set "LETRA=%LETRA%:"

set "ORIGEM="
if exist "%DESTINO_ARQ%" for /f "usebackq tokens=1,2 delims=|" %%a in ("%DESTINO_ARQ%") do set "ORIGEM=%%b"

echo.
echo  =========================================================
echo   FNF PS2 - copiar para o pendrive (sem compilar)
echo  =========================================================
echo.

if not defined ORIGEM goto :sem_origem
if not exist "%ORIGEM%\" set "ORIGEM=%ORIGEM:\\wsl.localhost\=\\wsl$\%"
if not exist "%ORIGEM%\" goto :sem_origem
if not exist "%ORIGEM%\FNF_PS2.ELF" goto :sem_origem
echo   pronto para copiar de:
echo       %ORIGEM%
for %%F in ("%ORIGEM%\FNF_PS2.ELF") do echo       FNF_PS2.ELF -- %%~zF bytes
echo.

if not defined LETRA call :escolher_letra
if not defined LETRA goto :cancelado
if not exist "%LETRA%\" goto :letra_ruim
if /i "%LETRA%"=="%SystemDrive%" goto :letra_ruim

rem  Teste de gravacao de verdade: se o Windows consegue criar um arquivo
rem  aqui, o robocopy tambem consegue.
>"%LETRA%\.fnf_teste" echo teste
if not exist "%LETRA%\.fnf_teste" goto :sem_escrita
del "%LETRA%\.fnf_teste" >nul 2>&1

echo   copiando para %LETRA%\FNFPS2 ...
if not exist "%LETRA%\FNFPS2\" md "%LETRA%\FNFPS2" >nul 2>&1
robocopy "%ORIGEM%" "%LETRA%\FNFPS2" /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP /XF build.log build-tests.log
if errorlevel 8 goto :falhou

rem  Conferencia no destino.
set "FALHAS=0"
if not exist "%LETRA%\FNFPS2\FNF_PS2.ELF" (
    echo   ERRO  falta FNF_PS2.ELF no destino
    set "FALHAS=1"
) else (
    for %%F in ("%LETRA%\FNFPS2\FNF_PS2.ELF") do echo   ok  FNF_PS2.ELF -- %%~zF bytes
)
set "CHARTS=0"
for /f %%c in ('dir /b /s "%LETRA%\FNFPS2\assets\songs\*.chart" 2^>nul ^| find /c /v ""') do set "CHARTS=%%c"
if "%CHARTS%"=="0" (
    echo   ERRO  nenhum chart copiado
    set "FALHAS=1"
) else (
    echo   ok  %CHARTS% charts em assets\songs\
)
if "%FALHAS%"=="1" goto :incompleto

echo.
echo  =========================================================
echo   COPIA CONCLUIDA em %LETRA%\FNFPS2
echo  =========================================================
echo.
echo   No PS2: uLaunchELF -^> mass:/FNFPS2/FNF_PS2.ELF
echo.
pause
exit /b 0

rem ---------------------------------------------------------------------------
:escolher_letra
echo   Onde esta' o pendrive? Estas sao as unidades que o Windows ve:
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Volume | Where-Object { $_.DriveLetter } | Sort-Object DriveLetter | ForEach-Object { '      ' + $_.DriveLetter + ':  ' + $_.DriveType + '  ' + $_.FileSystemLabel + '  ' + [math]::Round($_.SizeRemaining/1MB) + ' MB livres' }" 2>nul
echo.
set "LETRA="
set /p "LETRA=Digite a letra do pendrive (ex: F) e tecle Enter: "
if not defined LETRA exit /b 0
if not "%LETRA:~1,1%"==":" set "LETRA=%LETRA%:"
exit /b 0

:sem_origem
echo   Nao achei uma compilacao pronta para copiar.
echo.
echo   Rode o COMPILAR.bat primeiro. Ele compila e, quando o
echo   Ubuntu nao consegue gravar no pendrive, deixa o jogo
echo   pronto em uma pasta do WSL e mostra o caminho.
echo.
pause
exit /b 2

:letra_ruim
echo   "%LETRA%" nao e' uma unidade valida (ou e' o disco do sistema).
echo.
pause
exit /b 3

:sem_escrita
echo   Nao consegui gravar em %LETRA%\.
echo   O pendrive pode estar protegido contra escrita ou cheio.
echo.
pause
exit /b 4

:falhou
echo.
echo   O robocopy falhou (codigo %ERRORLEVEL%).
echo   Tente copiar a pasta a mao, no Explorador:
echo       %ORIGEM%
echo   para
echo       %LETRA%\FNFPS2
echo.
pause
exit /b 5

:incompleto
echo.
echo   A copia nao conferiu: faltou arquivo essencial.
echo   Rode de novo. Se repetir, mande esta tela.
echo.
pause
exit /b 6

:cancelado
echo.
echo   Cancelado.
echo.
pause
exit /b 0
