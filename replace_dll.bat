cd "C:\Games\Steam\steamapps\common\Sven Co-op\svencoop\addons\metamod\dlls"

if exist PortalSpawner_old.dll (
    del PortalSpawner_old.dll
)
if exist PortalSpawner.dll (
    rename PortalSpawner.dll PortalSpawner_old.dll 
)

exit /b 0