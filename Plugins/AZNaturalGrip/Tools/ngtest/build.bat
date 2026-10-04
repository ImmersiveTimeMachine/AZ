@echo off
rem Builds the Natural Grip core parity harness with MSVC, without Unreal.
rem   build.bat base       primitives only             -> Intermediate\base\ngtest.exe
rem   build.bat            everything                 -> Intermediate\all\ngtest.exe
rem   build.bat support    base + support-hand solver  -> Intermediate\support\ngtest.exe
rem   build.bat trigger    base + trigger-hand solver  -> Intermediate\trigger\ngtest.exe
rem Separate units build into separate folders, so two solvers can be developed at the same time.
rem /fp:precise keeps the double math identical to CPython's. Intermediate\ is git-ignored.
setlocal
set HERE=%~dp0
set CORE=%HERE%..\..\Source\AZNaturalGrip\Private\Core
set EIGEN=C:\UnrealEngine\Engine\Source\ThirdParty\Eigen
set UNIT=%1
if "%UNIT%"=="" set UNIT=all
set OUT=%HERE%Intermediate\%UNIT%
if not exist "%OUT%" mkdir "%OUT%"
set BASE="%CORE%\NGJson.cpp" "%CORE%\NGJsonDiff.cpp" "%CORE%\NGField.cpp" "%CORE%\NGHand.cpp" "%CORE%\NGSetup.cpp" "%HERE%main.cpp" "%HERE%TestPrims.cpp"
set DEFS=
set SRCS=%BASE%
if "%UNIT%"=="support" (
  set DEFS=/DNG_WITH_SUPPORT=1
  set SRCS=%BASE% "%CORE%\NGSupportHand.cpp" "%HERE%TestSupport.cpp" "%HERE%TestFast.cpp" "%HERE%TestPower.cpp"
)
if "%UNIT%"=="trigger" (
  set DEFS=/DNG_WITH_TRIGGER=1
  set SRCS=%BASE% "%CORE%\NGTriggerHand.cpp" "%HERE%TestTrigger.cpp"
)
if "%UNIT%"=="geometry" (
  set DEFS=/DNG_WITH_GEOMETRY=1
  set SRCS=%BASE% "%CORE%\NGGeometry.cpp" "%HERE%TestGeometry.cpp"
)
if "%UNIT%"=="all" (
  set DEFS=/DNG_WITH_SUPPORT=1 /DNG_WITH_TRIGGER=1 /DNG_WITH_GEOMETRY=1
  set SRCS=%BASE% "%CORE%\NGSupportHand.cpp" "%HERE%TestSupport.cpp" "%HERE%TestFast.cpp" "%HERE%TestPower.cpp" "%CORE%\NGTriggerHand.cpp" "%HERE%TestTrigger.cpp" "%CORE%\NGGeometry.cpp" "%HERE%TestGeometry.cpp"
)
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" (
  echo vcvars64.bat not found: %VCVARS%
  exit /b 2
)
call "%VCVARS%" >nul 2>nul
cl /nologo /std:c++17 /O2 /fp:precise /EHsc /W4 /MP /MD /D_CRT_SECURE_NO_WARNINGS %DEFS% /I"%CORE%" /I"%EIGEN%" /I"%HERE%." ^
   /Fo"%OUT%\\" /Fe"%OUT%\ngtest.exe" %SRCS%
exit /b %ERRORLEVEL%
