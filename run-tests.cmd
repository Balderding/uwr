@echo off
rem No proof or check may ever load the language model.
set UWPORT_LLM=0
rem Builds, then checks the engine against the Python reference and draws a few pictures with no
rem window (build\shots). Nothing is written to data\ or game\.
cd /d "%~dp0"
call "%~dp0build.cmd" || exit /b 1
if not exist extracted\data-ref\levels\level1.txt python -I tools\loose.py write || exit /b 1
if not exist extracted\levels\level1.png python -I tools\assets.py export || exit /b 1
build\check.exe || exit /b 1
if not exist build\shots mkdir build\shots
build\uw.exe --shot=build\shots\start.png --level=1 --pos=32,1.5 --yaw=0 || exit /b 1
build\uw.exe --shot=build\shots\entrance.png --level=1 --pos=32,2.5 --yaw=180 --pitch=-10 || exit /b 1
for %%L in (2 3 4 5 6 7 8 9) do build\uw.exe --shot=build\shots\level%%L.png --level=%%L || exit /b 1
rem ray tracing: the torch; the torch out (placed lights + bounced light); the orb under DLSS 5; no ray tracing
build\uw.exe --shot=build\shots\rt_torch.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --frames=40 || exit /b 1
build\uw.exe --shot=build\shots\rt_notorch.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --frames=60 --torch=0 || exit /b 1
build\uw.exe --shot=build\shots\rt_orb_dlss5.png --level=1 --pos=31.6,1.7 --yaw=-15 --pitch=8 --size=1600x900 --dlss=2 --frames=40 || exit /b 1
build\uw.exe --shot=build\shots\rt_high.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --dlss=1 --frames=40 --set=rt_quality=high || exit /b 1
rem the weapon (WEAPONS.GR): a longsword's slash, let go at frame 42, mid-swing at 47
build\uw.exe --shot=build\shots\weapon_swing.png --level=1 --pos=32,2.5 --yaw=0 --size=960x540 --ui=1 --dlss=0 --frames=47 --fight=1 --attack=2 --hold=40 --wield=5 || exit /b 1
rem saves (into build\savetest, never data\saves): take the sack and save; load it in a second run (the sack in the pack, not on the floor)
build\uw.exe --shot=build\shots\save_take.png --level=1 --pos=33.5,2.6 --yaw=-21 --pitch=-40 --size=960x540 --ui=1 --dlss=0 --frames=8 --use=2 --saves=build\savetest --save-at=5 || exit /b 1
build\uw.exe --shot=build\shots\save_load.png --level=1 --pos=33.5,2.6 --yaw=-21 --pitch=-40 --size=960x540 --ui=1 --dlss=0 --frames=8 --saves=build\savetest --load-at=2 --inv=4 || exit /b 1
rem character creation, stopped at "Keep this character?" (a female Mage)
build\uw.exe --shot=build\shots\chargen.png --level=1 --size=960x540 --ui=1 --dlss=0 --frames=12 --chargen=1 --picks=2,2,2,1,2,1,3,2,0 || exit /b 1
rem the character panel (,) and a weapon's tooltip with the character's own numbers
build\uw.exe --shot=build\shots\character.png --level=1 --pos=32,2.5 --yaw=0 --size=960x540 --ui=1 --dlss=0 --frames=4 --sheet=1 || exit /b 1
build\uw.exe --shot=build\shots\tooltip_sword.png --level=1 --pos=32,2.5 --yaw=0 --size=960x540 --ui=1 --dlss=0 --frames=4 --inv=1 --wield=5 --mouse=307,60 || exit /b 1
build\uw.exe --shot=build\shots\grid_fallback.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --rt=0 || exit /b 1
rem the relief on and off, and with DLSS 5
build\uw.exe --shot=build\shots\relief.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 || exit /b 1
build\uw.exe --shot=build\shots\relief_off.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --normals=0 || exit /b 1
build\uw.exe --shot=build\shots\relief_dlss5.png --level=1 --pos=31.3,2.2 --yaw=-60 --size=1600x900 --dlss=2 --frames=32 || exit /b 1
rem DLSS and DLSS 5 (32 frames of history), and the ' key's switch: on, off at 20, on again at 40
rem objects: look at a wall writing (its text), blood cannot be taken, the starting sack opened in the panel
build\uw.exe --shot=build\shots\look_writing.png --level=1 --pos=30.75,2.69 --yaw=-90 --pitch=-10 --size=960x540 --ui=1 --frames=4 --look=2 || exit /b 1
build\uw.exe --shot=build\shots\blood.png --level=1 --pos=28.8,4.7 --yaw=0 --pitch=-50 --size=960x540 --ui=1 --frames=5 --use=3 || exit /b 1
build\uw.exe --shot=build\shots\sack.png --level=1 --pos=33.5,2.6 --yaw=-21 --pitch=-40 --size=960x540 --ui=1 --frames=8 --use=2 --inv=4 --mouse=357,131 --rclick=6 || exit /b 1
build\uw.exe --shot=build\shots\dlss5.png --level=1 --pos=32,2.5 --yaw=180 --pitch=-10 --size=1600x900 --dlss=2 --frames=32 || exit /b 1
build\uw.exe --shot=build\shots\dlss5_switched.png --level=1 --pos=32,2.5 --yaw=180 --pitch=-10 --size=1600x900 --dlss=2 --frames=60 --toggle-every=20 || exit /b 1
build\uw.exe --shot=build\shots\cast_light.png --level=1 --pos=32,2.5 --yaw=180 --size=960x540 --ui=1 --dlss=0 --frames=8 --cast=2:In,Lor || exit /b 1
build\uw.exe --shot=build\shots\runes.png --level=1 --pos=32,2.5 --yaw=180 --size=960x540 --ui=1 --dlss=0 --frames=4 --cast=99:In,Lor,Ort --runes=1 --mouse=480,300 || exit /b 1
build\uw.exe --shot=build\shots\wand.png --level=2 --pos=36.45,3.3 --yaw=180 --pitch=-32 --size=960x540 --ui=1 --dlss=0 --frames=12 --use=2 --inv=4 --mouse=304,130 || exit /b 1
build\uw.exe --shot=build\shots\summon.png --level=1 --pos=32,3.5 --yaw=180 --size=960x540 --ui=1 --dlss=0 --frames=10 --char-level=16 --cast=3:Kal,Mani || exit /b 1
build\uw.exe --shot=build\shots\flame_wind.png --level=1 --pos=32,3.5 --yaw=180 --size=960x540 --ui=1 --dlss=0 --frames=10 --char-level=16 --cast=3:Flam,Hur || exit /b 1
call tools\verify.cmd || exit /b 1
call tests\xmidi_test.cmd 3000 || exit /b 1
build\uw.exe --sound-wav=work\sound_check.wav,5 || exit /b 1
build\uw.exe --shot=build\shots\door_open.png --level=1 --pos=24.5,6.9 --yaw=0 --size=960x540 --ui=1 --dlss=0 --frames=60 --use=2 || exit /b 1
build\uw.exe --shot=build\shots\sheet_tip.png --level=1 --pos=32,2.5 --yaw=0 --size=960x540 --ui=1 --dlss=0 --frames=4 --sheet=1 --mouse=30,124 || exit /b 1
echo all tests passed
