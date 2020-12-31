
    Welcome to PSPATARI

Original Authors of Atari800

  David Firth, Petr Stehlik and Atari800 Development Team 

Author of the PSP port version 

  Ludovic.Jacomme also known as Zx-81 (zx81.zx81@gmail.com)


1. INTRODUCTION
   ------------

  Atari800 is an emulator for the 800, 800XL, 130XE and 5200 models of
  the Atari personal computer. It can be used on console, FrameBuffer or X11.
  See http://atari800.sourceforge.net/ for further informations.

  PSPATARI is a port on PSP of the version 2.0.2 (April 08 2006) of Atari800
  from Petr Stehlik.

  Thanks to Danzel and Jeff Chen for their virtual keyboard,
  and to all PSPSDK developpers.

  This software is under GPL Copyright, read COPYING file for more 
  information about it.

  *** Special Thanks ***

  -> to Cyphre for his help on the sound issue !
  -> to Shadow for his beautiful icons on previous versions !
  -> to Raven for his beautiful icons (http://www.criticalraven.be/PSP/)
  -> to Carpy for his help on NTSC issue !

  Thanks should goes also to Pou-chan who developped the multiple 
  keyboard mapping feature.


2. INSTALLATION
   ------------

  Unzip the zip file, and copy the content of the directory fw3.x
  on the directory psp/game3xx (depending on your custom firmware version).

  Put your roms files on "roms" sub-directory. 

  It has been developped on linux for Firmware 3.xx-OE.

  For any comments or questions on this version, please visit 
  http://zx81.zx81.free.fr or http://zx81.dcemu.co.uk

3. CONTROL
   ------------

3.1 - Virtual keyboard

  In the ATARI emulator window 

  Normal & RTrigger mapping :

    PSP        ATARI 
  
    Square     Fire2
    Triangle   Return
    Cross      Fire1
    Circle     Space
    Up         Up
    Down       Down
    Left       Left
    Right      Right

    LTrigger   Toogle with L keyboard mapping
    RTrigger   Toggle with R keyboard mapping

  LTrigger mapping :

    PSP        ATARI 
  
    Square     Fire2
    Triangle   Escape
    Cross      Fire1
    Circle     Console Start
    Up         Up
    Down       Down
    Left       Left
    Right      Right


    A-Pad      Joystick
  
    Press Start  + L + R   to exit and return to eloader.
    Press Select           to enter in emulator main menu.
    Press Start            open/close the On-Screen keyboard

  In the main menu

    L+R+Start  Exit the emulator
    R Trigger  Reset the ATARI console

    Triangle   Go Up directory
    Cross      Valid
    Circle     Valid
    Square     Go Back to the emulator window

  The On-Screen Keyboard of "Danzel" and "Jeff Chen"

    Use Analog stick to choose one of the 9 squares, and
    use Triangle, Square, Cross and Circle to choose one
    of the 4 letters of the highlighted square.

    Use LTrigger and RTrigger to see other 9 squares 
    figures.

3.2 - IR keyboard

  You can also use IR keyboard. Edit the pspirkeyb.ini file
  to specify your IR keyboard model, and modify eventually
  layout keyboard files in the keymap directory.

  The following mapping is done :

  IR-keyboard   PSP

  Cursor        Digital Pad

  Tab           Start
  Ctrl-W        Start

  Escape        Select
  Ctrl-Q        Select

  Ctrl-E        Triangle
  Ctrl-X        Cross
  Ctrl-S        Square
  Ctrl-F        Circle
  Ctrl-Z        L-trigger
  Ctrl-C        R-trigger

  In the emulator window you can use the IR keyboard for
  cursor keys (but it doesn't work well).


4. LOADING ATARI ROM FILES
   ------------

  If you want to load rom image in your emulator, you have to put your rom file
  (with .zip, .rom, atr or .bin file extension) on your PSP memory stick in the
  'roms' directory.

  Then, while inside PSPATARI emulator, just press SELECT to enter in 
  the emulator main menu, and then using the file selector choose one 
  rom file to load in your emulator.

  Back to the emulator window, the rom should stard automatically.

  (NOTE) In the main emulator window you can only load Atari 800 roms ... If you 
  want to load 5200 roms, please use the "Original" Atari800 menu (the white 
  option in the emulator menu).

  Then select "Cartridge management" then "Insert cartridge" and specify
  your disk image file, then choose the cartridge type 5200, and go back
  to the emulator using LTrigger.


5. LOADING KEY MAPPING FILES
   ------------

  For given games, the default keyboard mapping between PSP Keys and
  ATARI keys, is not suitable, and the game can't be played on PSPATARI.

  To overcome the issue, you can write your own mapping file. Using notepad for
  example you can edit a file with the .kbd extension and put it in the kbd 
  directory.

  For the exact syntax of those mapping files, have a look on sample files already
  presents in the kbd directory (default.kbd etc ...).

  After writting such keyboard mapping file, you can load them using the main menu
  inside the emulator.

  If the keyboard filename is the same as the rom filename (.zip etc ...)
  then when you load this rom, the corresponding keyboard file is automatically 
  loaded !

  You can now use the Keyboard menu and edit, load and save your
  keyboard mapping files inside the emulator. The Save option save the .kbd
  file in the kbd directory using the "Game Name" as filename. The game name
  is displayed on the right corner in the emulator menu.

6. COMPILATION
   ------------

  It has been developped under Linux using gcc with PSPSDK. 
  To rebuild the homebrew run the Makefile in the src archive.

