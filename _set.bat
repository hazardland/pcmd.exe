@echo off

for %%a in ("%CD%") do set "PARENT_FOLDER=%%~nxa"
title %PARENT_FOLDER%

set GITBRANCH=
for /f %%I in ('git.exe branch --show-current 2^> NUL') do set GITBRANCH=%%I

if "%GITBRANCH%" == "" (
    prompt $E[38;5;75m%PARENT_FOLDER%$G$E[0m$S
) else (
    prompt $E[38;5;75m%PARENT_FOLDER%$E[0m[$E[38;5;229m%GITBRANCH%$E[0m]$G$S
)
