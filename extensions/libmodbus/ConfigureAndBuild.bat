set script_dir=%~dp0
cd %1
copy %script_dir%\Libmodbus.sln
copy %script_dir%\Libmodbus.vcxproj
cscript.exe configure.js
msbuild Libmodbus.sln /t:build /p:Configuration=Release

