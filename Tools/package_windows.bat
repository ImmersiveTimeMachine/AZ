@echo off
REM ---------------------------------------------------------------------------------------------
REM  Package CHALK as a standalone Windows game.
REM
REM  THE EDITOR MUST BE CLOSED. While it runs it holds the Live Coding lock and UAT refuses the
REM  build step outright ("Unable to build while Live Coding is active").
REM
REM  -Map is NOT optional. The project carries 144 .umap files and exactly ONE of them is ours;
REM  the other 143 are marketplace demo levels (MenuSystemPro, ClassicMansion, InventorySystemPro,
REM  GameAnimationSample, ...). Without this the cooker takes all of them and pulls their whole
REM  content trees in with them, including the multi-GB MetaHumanBodyTracker ML models.
REM
REM  Development, not Shipping: this is a playtest build, and Shipping strips the log and the
REM  console that every diagnosis in this project has relied on.
REM ---------------------------------------------------------------------------------------------

set UAT=C:\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat
set PROJECT=C:\UnrealEngine\Games\AZ\AZ.uproject
set ARCHIVE=C:\UnrealEngine\Games\AZ\Packaged

call "%UAT%" BuildCookRun ^
  -project="%PROJECT%" ^
  -noP4 -utf8output ^
  -platform=Win64 ^
  -clientconfig=Development ^
  -target=AZ ^
  -build -cook -stage -pak -compressed -archive ^
  -archivedirectory="%ARCHIVE%" ^
  -Map=/Game/AZ/Maps/L_001 ^
  -unattended

echo.
echo ==== exit code %ERRORLEVEL% ====
