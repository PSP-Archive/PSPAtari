/*
 * atari_sdl.c - SDL library specific port code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
   Thanks to David Olofson for scaling tips!

   TODO:
   - implement all Atari800 parameters
   - use mouse and real joystick
   - turn off fullscreen when error happen
*/

#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//LUDO:
#include "global.h"
#include "psp_sdl.h"
#include "psp_kbd.h"

#ifdef linux
#define LPTJOY  1
#endif

#ifdef LPTJOY
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/lp.h>
#endif /* LPTJOY */

/* Atari800 includes */
#include "input.h"
#include "colours.h"
#include "monitor.h"
#include "platform.h"
#include "ui.h"
#include "screen.h"
#include "pokeysnd.h"
#include "gtia.h"
#include "antic.h"
#include "devices.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "log.h"
#include "util.h"
#include "atari_ntsc.h"

//LUDO:
  int psp_screenshot_mode = 0;

//LUDO:
int joystick_states[2][6];

/* you can set that variables in code, or change it when emulator is running
   I am not sure what to do with sound_enabled (can't turn it on inside
   emulator, probably we need two variables or command line argument) */
#ifdef SOUND
static int sound_flags = 0;
static int sound_bits = 8;
#endif
static int SDL_ATARI_BPP = 0;  /* 0 - autodetect */
//LUDO: static int FULLSCREEN = 1;
static int FULLSCREEN = 0;
static int BW = 0;
static int SWAP_JOYSTICKS = 0;
static int WIDTH_MODE = 1;
static int ROTATE90 = 0;
static int ntscemu = 0;
static int scanlines_percentage = 20;
static atari_ntsc_t *the_ntscemu;
/* making setup static conveniently clears all fields to 0 */
static atari_ntsc_setup_t atari_ntsc_setup;
#define SHORT_WIDTH_MODE 0
#define DEFAULT_WIDTH_MODE 1
#define FULL_WIDTH_MODE 2

/* you need to uncomment this to turn on fps counter */

/* #define FPS_COUNTER = 1 */

/* joystick emulation
   keys are loaded from config file
   Here the defaults if there is no keymap in the config file... */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int kbd_joy_0_enabled = TRUE;  /* enabled by default, doesn't hurt */
int kbd_joy_1_enabled = FALSE;  /* disabled, would steal normal keys */

int KBD_TRIG_0 = SDLK_LCTRL;
int KBD_STICK_0_LEFT = SDLK_KP4;
int KBD_STICK_0_RIGHT = SDLK_KP6;
int KBD_STICK_0_DOWN = SDLK_KP2;
int KBD_STICK_0_UP = SDLK_KP8;
int KBD_STICK_0_LEFTUP = SDLK_KP7;
int KBD_STICK_0_RIGHTUP = SDLK_KP9;
int KBD_STICK_0_LEFTDOWN = SDLK_KP1;
int KBD_STICK_0_RIGHTDOWN = SDLK_KP3;

int KBD_TRIG_1 = SDLK_TAB;
int KBD_STICK_1_LEFT = SDLK_a;
int KBD_STICK_1_RIGHT = SDLK_d;
int KBD_STICK_1_DOWN = SDLK_x;
int KBD_STICK_1_UP = SDLK_w;
int KBD_STICK_1_LEFTUP = SDLK_q;
int KBD_STICK_1_RIGHTUP = SDLK_e;
int KBD_STICK_1_LEFTDOWN = SDLK_z;
int KBD_STICK_1_RIGHTDOWN = SDLK_c;

/* real joysticks */

int fd_joystick0 = -1;
int fd_joystick1 = -1;

SDL_Joystick *joystick0 = NULL;
SDL_Joystick *joystick1 = NULL;
int joystick0_nbuttons, joystick1_nbuttons;

#define minjoy 10000      /* real joystick tolerancy */

extern int alt_function;
    int key_pressed = 0;

#ifdef SOUND
/* sound */
#define FRAGSIZE        10    /* 1<<FRAGSIZE is size of sound buffer */
static int SOUND_VOLUME = (SDL_MIX_MAXVOLUME * 3) / 4;
static int dsprate = 44100;
#endif

/* video */
SDL_Surface *MainScreen = NULL;
SDL_Color colors[256];      /* palette */
Uint16 Palette16[256];      /* 16-bit palette */
Uint32 Palette32[256];      /* 32-bit palette */

/* used in UI to show how the keyboard joystick is mapped */
char *joy_0_description(char *buffer, int maxsize)
{
  snprintf(buffer, maxsize, " (L=%s R=%s U=%s D=%s B=%s)",
      SDL_GetKeyName(KBD_STICK_0_LEFT),
      SDL_GetKeyName(KBD_STICK_0_RIGHT),
      SDL_GetKeyName(KBD_STICK_0_UP),
      SDL_GetKeyName(KBD_STICK_0_DOWN),
      SDL_GetKeyName(KBD_TRIG_0)
  );
  return buffer;
}

char *joy_1_description(char *buffer, int maxsize)
{
  snprintf(buffer, maxsize, " (L=%s R=%s U=%s D=%s B=%s)",
      SDL_GetKeyName(KBD_STICK_1_LEFT),
      SDL_GetKeyName(KBD_STICK_1_RIGHT),
      SDL_GetKeyName(KBD_STICK_1_UP),
      SDL_GetKeyName(KBD_STICK_1_DOWN),
      SDL_GetKeyName(KBD_TRIG_1)
  );
  return buffer;
}

#ifdef SOUND

void Sound_Pause(void)
{
  if (ATARI.atari_snd_enable) {
    /* stop audio output */
    SDL_PauseAudio(1);
  }
}

void Sound_Continue(void)
{
  if (ATARI.atari_snd_enable) {
    /* start audio output */
    SDL_PauseAudio(0);
  }
}

void Sound_Update(void)
{
  /* fake function */
}

#endif /* SOUND */

void SetPalette(void)
{
  SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);
}

void CalcPalette(void)
{
  int i, rgb;
  float y;
  Uint32 c;
  if (BW == 0)
    for (i = 0; i < 256; i++) {
      rgb = colortable[i];
      colors[i].r = (rgb & 0x00ff0000) >> 16;
      colors[i].g = (rgb & 0x0000ff00) >> 8;
      colors[i].b = (rgb & 0x000000ff) >> 0;
    }
  else
    for (i = 0; i < 256; i++) {
      rgb = colortable[i];
      y = 0.299 * ((rgb & 0x00ff0000) >> 16) +
        0.587 * ((rgb & 0x0000ff00) >> 8) +
        0.114 * ((rgb & 0x000000ff) >> 0);
      colors[i].r = y;
      colors[i].g = y;
      colors[i].b = y;
    }
  for (i = 0; i < 256; i++) {
    c =
      SDL_MapRGB(MainScreen->format, colors[i].r, colors[i].g,
             colors[i].b);
    switch (MainScreen->format->BitsPerPixel) {
    case 16:
      Palette16[i] = (Uint16) c;
      break;
    case 32:
      Palette32[i] = (Uint32) c;
      break;
    }
  }

}

void ModeInfo(void)
{
  char bwflag, fullflag, width, joyflag;
  if (BW)
    bwflag = '*';
  else
    bwflag = ' ';
  if (FULLSCREEN)
    fullflag = '*';
  else
    fullflag = ' ';
  switch (WIDTH_MODE) {
  case FULL_WIDTH_MODE:
    width = 'f';
    break;
  case DEFAULT_WIDTH_MODE:
    width = 'd';
    break;
  case SHORT_WIDTH_MODE:
    width = 's';
    break;
  default:
    width = '?';
    break;
  }
  if (SWAP_JOYSTICKS)
    joyflag = '*';
  else
    joyflag = ' ';
  Aprint("Video Mode: %dx%dx%d", MainScreen->w, MainScreen->h,
       MainScreen->format->BitsPerPixel);
  Aprint("[%c] FULLSCREEN  [%c] BW  [%c] WIDTH MODE  [%c] JOYSTICKS SWAPPED",
     fullflag, bwflag, width, joyflag);
}

# if 0 //LUDO: 
void SetVideoMode(int w, int h, int bpp)
{
# if 0 //LUDO:
  if (FULLSCREEN)
    MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_FULLSCREEN);
  else
    MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_RESIZABLE);
  if (MainScreen == NULL) {
    Aprint("Setting Video Mode: %dx%dx%d FAILED", w, h, bpp);
    Aflushlog();
    exit(-1);
  }
# else
  MainScreen=SDL_SetVideoMode(w,h,bpp, SDL_ANYFORMAT|SDL_DOUBLEBUF|SDL_HWSURFACE|SDL_HWPALETTE);
# endif

}
# endif

void SetNewVideoMode(int w, int h, int bpp)
{
  float ww, hh;

# if 0 //LUDO:
  if (ROTATE90||ntscemu) {
    SetVideoMode(w, h, bpp);
  }
  else 
# endif
    {

    if ((h < ATARI_HEIGHT) || (w < ATARI_WIDTH)) {
      h = ATARI_HEIGHT;
      w = ATARI_WIDTH;
    }

    /* aspect ratio, floats needed */
    ww = w;
    hh = h;
    switch (WIDTH_MODE) {
    case SHORT_WIDTH_MODE:
      if (ww * 0.75 < hh)
        hh = ww * 0.75;
      else
        ww = hh / 0.75;
      break;
    case DEFAULT_WIDTH_MODE:
      if (ww / 1.4 < hh)
        hh = ww / 1.4;
      else
        ww = hh * 1.4;
      break;
    case FULL_WIDTH_MODE:
      if (ww / 1.6 < hh)
        hh = ww / 1.6;
      else
        ww = hh * 1.6;
      break;
    }
    w = ww;
    h = hh;
    w = w / 8;
    w = w * 8;
    h = h / 8;
    h = h * 8;

# if 0 //LUDO:
    SetVideoMode(w, h, bpp);
# endif

    SDL_ATARI_BPP = MainScreen->format->BitsPerPixel;
# if 0 //LUDO:
    if (bpp == 0) {
      Aprint("detected %dbpp", SDL_ATARI_BPP);
      if ((SDL_ATARI_BPP != 8) && (SDL_ATARI_BPP != 16)
        && (SDL_ATARI_BPP != 32)) {
        Aprint("it's unsupported, so setting 8bit mode (slow conversion)");
        SetVideoMode(w, h, 8);
      }
    }
# endif
  }

  SetPalette();

# if 0  //LUDO:
  SDL_ShowCursor(SDL_DISABLE);  /* hide mouse cursor */
# endif

  ModeInfo();

}

void SwitchFullscreen(void)
{
  FULLSCREEN = 1 - FULLSCREEN;
  SetNewVideoMode(MainScreen->w, MainScreen->h,
          MainScreen->format->BitsPerPixel);
  Atari_DisplayScreen();
}

void SwitchWidth(void)
{
  WIDTH_MODE++;
  if (WIDTH_MODE > FULL_WIDTH_MODE)
    WIDTH_MODE = SHORT_WIDTH_MODE;
  SetNewVideoMode(MainScreen->w, MainScreen->h,
          MainScreen->format->BitsPerPixel);
  Atari_DisplayScreen();
}

void SwitchBW(void)
{
  BW = 1 - BW;
  CalcPalette();
  SetPalette();
  ModeInfo();
}

void SwapJoysticks(void)
{
  SWAP_JOYSTICKS = 1 - SWAP_JOYSTICKS;
  ModeInfo();
}

static  Uint8 dsp_buffer[2 << FRAGSIZE]; /* x2, because 16bit buffers */
#ifdef SOUND
void SDL_Sound_Update(void *userdata, Uint8 *stream, int len)
{
  if (len > (1 << FRAGSIZE))
    len = 1 << FRAGSIZE;
  Pokey_process(dsp_buffer, len);

# if 1
  if (sound_bits == 8) {
    SDL_MixAudio(stream, dsp_buffer, len, SOUND_VOLUME);
  } else {
    SDL_MixAudio(stream, dsp_buffer, 2 * len, SOUND_VOLUME);
  }
# else
  if (sound_bits == 8) memcpy(stream, dsp_buffer, len);
  else                 memcpy(stream, dsp_buffer, len * 2); 
# endif
}

void 
SDL_Sound_Initialise(void)
{
  int i, j;
  SDL_AudioSpec desired, obtained;

# if 0 
  for (i = j = 1; i < *argc; i++) {
    if (strcmp(argv[i], "-sound") == 0)
      sound_enabled = TRUE;
    else if (strcmp(argv[i], "-nosound") == 0)
      sound_enabled = FALSE;
    else if (strcmp(argv[i], "-audio16") == 0) {
      Aprint("audio 16bit enabled");
    }
    else if (strcmp(argv[i], "-dsprate") == 0)
      dsprate = Util_sscandec(argv[++i]);
    else {
      if (strcmp(argv[i], "-help") == 0) {
        Aprint("\t-sound           Enable sound");
        Aprint("\t-nosound         Disable sound");
        Aprint("\t-dsprate <rate>  Set DSP rate in Hz");
        sound_enabled = FALSE;
      }
      argv[j++] = argv[i];
    }
  }
  *argc = j;
# endif

  /* FOR_TEST !! */
  sound_flags |= SND_BIT16;
  sound_bits = 16;

# if 0 //LUDO:
  if (sound_enabled) 
# endif
  {
    desired.freq = dsprate;
    if (sound_bits == 8)
      desired.format = AUDIO_U8;
    else if (sound_bits == 16)
      desired.format = AUDIO_S16;
    else {
      Aprint("unknown sound_bits");
      Atari800_Exit(FALSE);
      Aflushlog();
    }

    desired.samples = 1 << FRAGSIZE;
    desired.callback = SDL_Sound_Update;
    desired.userdata = NULL;
    desired.channels = 1;

    if (SDL_OpenAudio(&desired, &obtained) < 0) {
      Aprint("Problem with audio: %s", SDL_GetError());
      Aprint("Sound is disabled.");
# if 0 //LUDO:
      sound_enabled = FALSE;
# endif
      return;
    }

    SDL_Delay(1000);        // Give sound some time to init

    Pokey_sound_init(FREQ_17_EXACT, dsprate, 1, sound_flags);

    if (ATARI.atari_snd_enable) SDL_PauseAudio(0);
    else                        SDL_PauseAudio(1);

  }
}
#endif /* SOUND */


int Atari_Keyboard(void)
{
  int ret_key_code;
# if 0 //LUDO:
  static int lastkey = AKEY_NONE;
    static int key_control = 0;
   static int lastuni = 0;
  int shiftctrl = 0;
# endif

   ret_key_code = psp_update_keys();

# if 0 //LUDO:
   if (ret_key_code != AKEY_NONE) {
     fprintf(stdout, "ret_key_code=%d press=%d\n", ret_key_code, key_pressed);
   }
# endif

# if 0 //LUDO:
   if (kbhits == NULL) {
     kbhits = (Uint8 *)malloc(sizeof(Uint8) * 1024);
     memset(kbhits, 0, 1024);
   }
   key_pressed = 0;
   alt_function = -1;
   key_consol = CONSOL_NONE;
  SDL_Event event;
  if (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      lastkey = event.key.keysym.sym;
       lastuni = event.key.keysym.unicode;
      key_pressed = 1;
      break;
    case SDL_KEYUP:
      lastkey = event.key.keysym.sym;
       lastuni = 0; /* event.key.keysym.unicode is not defined for KEYUP */
      key_pressed = 0;
      break;
    case SDL_VIDEORESIZE:
      SetNewVideoMode(event.resize.w, event.resize.h, MainScreen->format->BitsPerPixel);
      break;
    case SDL_QUIT:
      return AKEY_EXIT;
      break;
    }
  }
  else if (!key_pressed)
    return AKEY_NONE;

  kbhits = SDL_GetKeyState(NULL);

  if (kbhits == NULL) {
    Aprint("oops, kbhits is NULL!");
    Aflushlog();
    exit(-1);
  }

  alt_function = -1;
  if (kbhits[SDLK_LALT]) {
    if (key_pressed) {
      switch (lastkey) {
      case SDLK_f:
        key_pressed = 0;
        SwitchFullscreen();
        break;
      case SDLK_g:
        key_pressed = 0;
        SwitchWidth();
        break;
      case SDLK_b:
        key_pressed = 0;
        SwitchBW();
        break;
      case SDLK_j:
        key_pressed = 0;
        SwapJoysticks();
        break;
      case SDLK_r:
        alt_function = MENU_RUN;
        break;
      case SDLK_y:
        alt_function = MENU_SYSTEM;
        break;
      case SDLK_o:
        alt_function = MENU_SOUND;
        break;
      case SDLK_w:
        alt_function = MENU_SOUND_RECORDING;
        break;
      case SDLK_a:
        alt_function = MENU_ABOUT;
        break;
      case SDLK_s:
        alt_function = MENU_SAVESTATE;
        break;
      case SDLK_d:
        alt_function = MENU_DISK;
        break;
      case SDLK_l:
        alt_function = MENU_LOADSTATE;
        break;
      case SDLK_c:
        alt_function = MENU_CARTRIDGE;
        break;
      default:
        if(ntscemu){
          switch(lastkey){
          case SDLK_1:
            key_pressed = 0;
            atari_ntsc_setup.sharpness-=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_2:
            key_pressed = 0;
            atari_ntsc_setup.sharpness+=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_3:
            key_pressed = 0;
            atari_ntsc_setup.saturation-=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_4:
            key_pressed = 0;
            atari_ntsc_setup.saturation+=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_5:
            key_pressed = 0;
            atari_ntsc_setup.brightness-=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_6:
            key_pressed = 0;
            atari_ntsc_setup.brightness+=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_7:
            key_pressed = 0;
            atari_ntsc_setup.contrast-=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_8:
            key_pressed = 0;
            atari_ntsc_setup.contrast+=0.1;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_9:
            key_pressed = 0;
            atari_ntsc_setup.burst_phase-=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_0:
            key_pressed = 0;
            atari_ntsc_setup.burst_phase+=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_MINUS:
            key_pressed = 0;
            atari_ntsc_setup.gaussian_factor-=.2;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_EQUALS:
            key_pressed = 0;
            atari_ntsc_setup.gaussian_factor+=.2;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_LEFTBRACKET:
            key_pressed = 0;
            scanlines_percentage -= 5*(scanlines_percentage>=5);
            Aprint("scanlines percentage:%d",scanlines_percentage);
            break;
          case SDLK_RIGHTBRACKET:
            key_pressed = 0;
            scanlines_percentage += 5*(scanlines_percentage<=100-5);
            Aprint("scanlines percentage:%d",scanlines_percentage);
            break;
          case SDLK_SEMICOLON:
            key_pressed = 0;
            atari_ntsc_setup.hue-=.01;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_QUOTE:
            key_pressed = 0;
            atari_ntsc_setup.hue+=.01;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_COMMA:
            key_pressed = 0;
            atari_ntsc_setup.gamma_adj-=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_PERIOD:
            key_pressed = 0;
            atari_ntsc_setup.gamma_adj+=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_INSERT:
            key_pressed = 0;
            atari_ntsc_setup.saturation_ramp-=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          case SDLK_DELETE:
            key_pressed = 0;
            atari_ntsc_setup.saturation_ramp+=.05;
            atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
            break;
          }
        }
      break;
      }
    }
    if (alt_function != -1) {
      key_pressed = 0;
      return AKEY_UI;
    }
  }

  /* SHIFT STATE */
  if ((kbhits[SDLK_LSHIFT]) || (kbhits[SDLK_RSHIFT]))
    key_shift = 1;
  else
    key_shift = 0;

    /* CONTROL STATE */
  if ((kbhits[SDLK_LCTRL]) || (kbhits[SDLK_RCTRL]))
    key_control = 1;
  else
    key_control = 0;

  /*
  if (event.type == 2 || event.type == 3) {
    Aprint("E:%x S:%x C:%x K:%x U:%x M:%x",event.type,key_shift,key_control,lastkey,event.key.keysym.unicode,event.key.keysym.mod);
  }
  */

  /* OPTION / SELECT / START keys */
  key_consol = CONSOL_NONE;
  if (kbhits[SDLK_F2])
    key_consol &= (~CONSOL_OPTION);
  if (kbhits[SDLK_F3])
    key_consol &= (~CONSOL_SELECT);
  if (kbhits[SDLK_F4])
    key_consol &= (~CONSOL_START);

  if (key_pressed == 0)
    return AKEY_NONE;

  /* Handle movement and special keys. */
  switch (lastkey) {
  case SDLK_F1:
    key_pressed = 0;
    return AKEY_UI;
  case SDLK_F5:
    key_pressed = 0;
    return key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
  case SDLK_F8:
    key_pressed = 0;
    return (Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT);
  case SDLK_F9:
    return AKEY_EXIT;
  case SDLK_F10:
    key_pressed = 0;
    return key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT_INTERLACE;
  }

  /* keyboard joysticks: don't pass the keypresses to emulation
   * as some games pause on a keypress (River Raid, Bruce Lee)
   */
  if (!ui_is_active && kbd_joy_0_enabled) {
    if (lastkey == KBD_STICK_0_LEFT || lastkey == KBD_STICK_0_RIGHT ||
      lastkey == KBD_STICK_0_UP || lastkey == KBD_STICK_0_DOWN ||
      lastkey == KBD_STICK_0_LEFTUP || lastkey == KBD_STICK_0_LEFTDOWN ||
      lastkey == KBD_STICK_0_RIGHTUP || lastkey == KBD_STICK_0_RIGHTDOWN ||
      lastkey == KBD_TRIG_0) {
      key_pressed = 0;
      return AKEY_NONE;
    }
  }

  if (!ui_is_active && kbd_joy_1_enabled) {
    if (lastkey == KBD_STICK_1_LEFT || lastkey == KBD_STICK_1_RIGHT ||
      lastkey == KBD_STICK_1_UP || lastkey == KBD_STICK_1_DOWN ||
      lastkey == KBD_STICK_1_LEFTUP || lastkey == KBD_STICK_1_LEFTDOWN ||
      lastkey == KBD_STICK_1_RIGHTUP || lastkey == KBD_STICK_1_RIGHTDOWN ||
      lastkey == KBD_TRIG_1) {
      key_pressed = 0;
      return AKEY_NONE;
    }
  }

  if (key_shift)
    shiftctrl ^= AKEY_SHFT;

  if (machine_type == MACHINE_5200 && !ui_is_active) {
    if (lastkey == SDLK_F4)
      return AKEY_5200_START ^ shiftctrl;
    switch (lastuni) {
    case 'p':
      return AKEY_5200_PAUSE ^ shiftctrl;
    case 'r':
      return AKEY_5200_RESET ^ shiftctrl;
    case '0':
      return AKEY_5200_0 ^ shiftctrl;
    case '1':
      return AKEY_5200_1 ^ shiftctrl;
    case '2':
      return AKEY_5200_2 ^ shiftctrl;
    case '3':
      return AKEY_5200_3 ^ shiftctrl;
    case '4':
      return AKEY_5200_4 ^ shiftctrl;
    case '5':
      return AKEY_5200_5 ^ shiftctrl;
    case '6':
      return AKEY_5200_6 ^ shiftctrl;
    case '7':
      return AKEY_5200_7 ^ shiftctrl;
    case '8':
      return AKEY_5200_8 ^ shiftctrl;
    case '9':
      return AKEY_5200_9 ^ shiftctrl;
    /* XXX: " ^ shiftctrl" harmful for '#' and '*' ? */
    case '#':
    case '=':
      return AKEY_5200_HASH;
    case '*':
      return AKEY_5200_ASTERISK;
    }
    return AKEY_NONE;
  }

  if (key_control)
    shiftctrl ^= AKEY_CTRL;

  switch (lastkey) {
  case SDLK_BACKQUOTE: /* fallthrough */
  case SDLK_LSUPER:
    return AKEY_ATARI ^ shiftctrl;
  case SDLK_RSUPER:
    if (key_shift)
      return AKEY_CAPSLOCK;
    else
      return AKEY_CAPSTOGGLE;
  case SDLK_END:
  case SDLK_F6:
    return AKEY_HELP ^ shiftctrl;
  case SDLK_PAGEDOWN:
    return AKEY_F2 | AKEY_SHFT;
  case SDLK_PAGEUP:
    return AKEY_F1 | AKEY_SHFT;
  case SDLK_HOME:
    return AKEY_CLEAR;
  case SDLK_PAUSE:
  case SDLK_F7:
    return AKEY_BREAK;
  case SDLK_CAPSLOCK:
    return AKEY_CAPSLOCK;
  case SDLK_SPACE:
    return AKEY_SPACE ^ shiftctrl;
  case SDLK_BACKSPACE:
    return AKEY_BACKSPACE;
  case SDLK_RETURN:
    return AKEY_RETURN ^ shiftctrl;
  case SDLK_LEFT:
    return AKEY_LEFT ^ shiftctrl;
  case SDLK_RIGHT:
    return AKEY_RIGHT ^ shiftctrl;
  case SDLK_UP:
    return AKEY_UP ^ shiftctrl;
  case SDLK_DOWN:
    return AKEY_DOWN ^ shiftctrl;
  case SDLK_ESCAPE:
    return AKEY_ESCAPE ^ shiftctrl;
  case SDLK_TAB:
    return AKEY_TAB ^ shiftctrl;
  case SDLK_DELETE:
    if (key_shift)
      return AKEY_DELETE_LINE;
    else
      return AKEY_DELETE_CHAR;
  case SDLK_INSERT:
    if (key_shift)
      return AKEY_INSERT_LINE;
    else
      return AKEY_INSERT_CHAR;
  }

  /* Handle CTRL-0 to CTRL-9 */
  if (key_control) {
    switch(lastuni) {
    case '.':
      return AKEY_FULLSTOP | AKEY_CTRL;
    case ',':
      return AKEY_COMMA | AKEY_CTRL;
    case ';':
      return AKEY_SEMICOLON | AKEY_CTRL;
    }
    switch (lastkey) {
    case SDLK_0:
      return AKEY_CTRL_0;
    case SDLK_1:
      return AKEY_CTRL_1;
    case SDLK_2:
      return AKEY_CTRL_2;
    case SDLK_3:
      return AKEY_CTRL_3;
    case SDLK_4:
      return AKEY_CTRL_4;
    case SDLK_5:
      return AKEY_CTRL_5;
    case SDLK_6:
      return AKEY_CTRL_6;
    case SDLK_7:
      return AKEY_CTRL_7;
    case SDLK_8:
      return AKEY_CTRL_8;
    case SDLK_9:
      return AKEY_CTRL_9;
    }
  }

  /* Uses only UNICODE translation, no shift states */
  switch (lastuni) {
  case 1:
    return AKEY_CTRL_a;
  case 2:
    return AKEY_CTRL_b;
  case 3:
    return AKEY_CTRL_c;
  case 4:
    return AKEY_CTRL_d;
  case 5:
    return AKEY_CTRL_e;
  case 6:
    return AKEY_CTRL_f;
  case 7:
    return AKEY_CTRL_g;
  case 8:
    return AKEY_CTRL_h;
  case 9:
    return AKEY_CTRL_i;
  case 10:
    return AKEY_CTRL_j;
  case 11:
    return AKEY_CTRL_k;
  case 12:
    return AKEY_CTRL_l;
  case 13:
    return AKEY_CTRL_m;
  case 14:
    return AKEY_CTRL_n;
  case 15:
    return AKEY_CTRL_o;
  case 16:
    return AKEY_CTRL_p;
  case 17:
    return AKEY_CTRL_q;
  case 18:
    return AKEY_CTRL_r;
  case 19:
    return AKEY_CTRL_s;
  case 20:
    return AKEY_CTRL_t;
  case 21:
    return AKEY_CTRL_u;
  case 22:
    return AKEY_CTRL_v;
  case 23:
    return AKEY_CTRL_w;
  case 24:
    return AKEY_CTRL_x;
  case 25:
    return AKEY_CTRL_y;
  case 26:
    return AKEY_CTRL_z;
  case 'A':
    return AKEY_A;
  case 'B':
    return AKEY_B;
  case 'C':
    return AKEY_C;
  case 'D':
    return AKEY_D;
  case 'E':
    return AKEY_E;
  case 'F':
    return AKEY_F;
  case 'G':
    return AKEY_G;
  case 'H':
    return AKEY_H;
  case 'I':
    return AKEY_I;
  case 'J':
    return AKEY_J;
  case 'K':
    return AKEY_K;
  case 'L':
    return AKEY_L;
  case 'M':
    return AKEY_M;
  case 'N':
    return AKEY_N;
  case 'O':
    return AKEY_O;
  case 'P':
    return AKEY_P;
  case 'Q':
    return AKEY_Q;
  case 'R':
    return AKEY_R;
  case 'S':
    return AKEY_S;
  case 'T':
    return AKEY_T;
  case 'U':
    return AKEY_U;
  case 'V':
    return AKEY_V;
  case 'W':
    return AKEY_W;
  case 'X':
    return AKEY_X;
  case 'Y':
    return AKEY_Y;
  case 'Z':
    return AKEY_Z;
  case ':':
    return AKEY_COLON;
  case '!':
    return AKEY_EXCLAMATION;
  case '@':
    return AKEY_AT;
  case '#':
    return AKEY_HASH;
  case '$':
    return AKEY_DOLLAR;
  case '%':
    return AKEY_PERCENT;
  case '^':
    return AKEY_CARET;
  case '&':
    return AKEY_AMPERSAND;
  case '*':
    return AKEY_ASTERISK;
  case '(':
    return AKEY_PARENLEFT;
  case ')':
    return AKEY_PARENRIGHT;
  case '+':
    return AKEY_PLUS;
  case '_':
    return AKEY_UNDERSCORE;
  case '"':
    return AKEY_DBLQUOTE;
  case '?':
    return AKEY_QUESTION;
  case '<':
    return AKEY_LESS;
  case '>':
    return AKEY_GREATER;
  case 'a':
    return AKEY_a;
  case 'b':
    return AKEY_b;
  case 'c':
    return AKEY_c;
  case 'd':
    return AKEY_d;
  case 'e':
    return AKEY_e;
  case 'f':
    return AKEY_f;
  case 'g':
    return AKEY_g;
  case 'h':
    return AKEY_h;
  case 'i':
    return AKEY_i;
  case 'j':
    return AKEY_j;
  case 'k':
    return AKEY_k;
  case 'l':
    return AKEY_l;
  case 'm':
    return AKEY_m;
  case 'n':
    return AKEY_n;
  case 'o':
    return AKEY_o;
  case 'p':
    return AKEY_p;
  case 'q':
    return AKEY_q;
  case 'r':
    return AKEY_r;
  case 's':
    return AKEY_s;
  case 't':
    return AKEY_t;
  case 'u':
    return AKEY_u;
  case 'v':
    return AKEY_v;
  case 'w':
    return AKEY_w;
  case 'x':
    return AKEY_x;
  case 'y':
    return AKEY_y;
  case 'z':
    return AKEY_z;
  case ';':
    return AKEY_SEMICOLON;
  case '0':
    return AKEY_0;
  case '1':
    return AKEY_1;
  case '2':
    return AKEY_2;
  case '3':
    return AKEY_3;
  case '4':
    return AKEY_4;
  case '5':
    return AKEY_5;
  case '6':
    return AKEY_6;
  case '7':
    return AKEY_7;
  case '8':
    return AKEY_8;
  case '9':
    return AKEY_9;
  case ',':
    return AKEY_COMMA;
  case '.':
    return AKEY_FULLSTOP;
  case '=':
    return AKEY_EQUAL;
  case '-':
    return AKEY_MINUS;
  case '\'':
    return AKEY_QUOTE;
  case '/':
    return AKEY_SLASH;
  case '\\':
    return AKEY_BACKSLASH;
  case '[':
    return AKEY_BRACKETLEFT;
  case ']':
    return AKEY_BRACKETRIGHT;
  case '|':
    return AKEY_BAR;
  }
# endif
  return ret_key_code;
}

void Init_SDL_Joysticks(int first, int second)
{
  if (first) {
    joystick0 = SDL_JoystickOpen(0);
    if (joystick0 == NULL)
      Aprint("joystick 0 not found");
    else {
      Aprint("joystick 0 found!");
      joystick0_nbuttons = SDL_JoystickNumButtons(joystick0);
      SWAP_JOYSTICKS = 1;    /* real joy is STICK(0) and numblock is STICK(1) */
    }
  }

  if (second) {
    joystick1 = SDL_JoystickOpen(1);
    if (joystick1 == NULL)
      Aprint("joystick 1 not found");
    else {
      Aprint("joystick 1 found!");
      joystick1_nbuttons = SDL_JoystickNumButtons(joystick1);
    }
  }
}

void Init_Joysticks(int *argc, char *argv[])
{
#ifdef LPTJOY
  char *lpt_joy0 = NULL;
  char *lpt_joy1 = NULL;
  int i;
  int j;

  for (i = j = 1; i < *argc; i++) {
    if (!strcmp(argv[i], "-joy0")) {
      if (i == *argc - 1) {
        Aprint("joystick device path missing!");
        break;
      }
      lpt_joy0 = argv[++i];
    }
    else if (!strcmp(argv[i], "-joy1")) {
      if (i == *argc - 1) {
        Aprint("joystick device path missing!");
        break;
      }
      lpt_joy1 = argv[++i];
    }
    else {
      argv[j++] = argv[i];
    }
  }
  *argc = j;

  if (lpt_joy0 != NULL) {        /* LPT1 joystick */
    fd_joystick0 = open(lpt_joy0, O_RDONLY);
    if (fd_joystick0 == -1)
      perror(lpt_joy0);
  }
  if (lpt_joy1 != NULL) {        /* LPT2 joystick */
    fd_joystick1 = open(lpt_joy1, O_RDONLY);
    if (fd_joystick1 == -1)
      perror(lpt_joy1);
  }
#endif /* LPTJOY */
  Init_SDL_Joysticks(fd_joystick0 == -1, fd_joystick1 == -1);
}


void Atari_Initialise(int *argc, char *argv[])
{
  int i;
  int no_joystick;
  int width, height, bpp;
  int help_only = FALSE;

  no_joystick = 0;

  width = 480;
  height = 272;
  bpp = 16;

  psp_sdl_init();
  MainScreen = back_surface;

  SetNewVideoMode(width, height, bpp);
  CalcPalette();
  SetPalette();

  Aprint("video initialized");

  if (no_joystick == 0)
    Init_Joysticks(argc, argv);

  SDL_EnableUNICODE(1);
}

int Atari_Exit(int run_monitor)
{
  int restart;
  int original_fullscreen = FULLSCREEN;

  if (run_monitor) {
    /* disable graphics, set alpha mode */
    if (FULLSCREEN) {
      SwitchFullscreen();
    }
#ifdef SOUND
    Sound_Pause();
#endif
    restart = monitor();
#ifdef SOUND
    Sound_Continue();
#endif
  }
  else {
    restart = FALSE;
  }

  if (restart) {
    /* set up graphics and all the stuff */
    if (original_fullscreen != FULLSCREEN) {
      SwitchFullscreen();
    }
    return 1;
  }

  SDL_Quit();

  Aflushlog();

  return restart;
}

# define PSP_LINE_WIDTH 512

void
Display_fill_blit_surface()
{
  int x_s;
  int y_s;

  uchar  *atari_top_scr = (UBYTE *)atari_screen;
  ushort *psp_top_scr   = (ushort *)blit_surface->pixels;

  uchar  *atari_scr = atari_top_scr;
  ushort *psp_scr = psp_top_scr;
  
  for (y_s = 0; y_s < ATARI_HEIGHT; y_s++) {
    for (x_s = 0; x_s < ATARI_WIDTH; x_s++) {
      psp_scr[x_s] = Palette16[ atari_scr[x_s] ];
    }
    atari_scr += ATARI_WIDTH;
    psp_scr   += ATARI_WIDTH;
  }
}

static void
Display_gu_fit_width(void)
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = ATARI_WIDTH;
  srcRect.h = ATARI_HEIGHT;
  dstRect.x = 0;
  dstRect.y = 16;
  dstRect.w = back_surface->w;
  dstRect.h = ATARI_HEIGHT;

  Display_fill_blit_surface();
  psp_sdl_gu_stretch(&srcRect, &dstRect);
}

static void
Display_gu_fit(void)
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = ATARI_WIDTH;
  srcRect.h = ATARI_HEIGHT;
  dstRect.x = 0;
  dstRect.y = 0;
  dstRect.w = back_surface->w;
  dstRect.h = back_surface->h;

  Display_fill_blit_surface();
  psp_sdl_gu_stretch(&srcRect, &dstRect);
}

static void
Display_gu_fit_height(void)
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = ATARI_WIDTH;
  srcRect.h = ATARI_HEIGHT;
  dstRect.x = 22;
  dstRect.y = 0;
  dstRect.w = 435;
  dstRect.h = back_surface->h;

  Display_fill_blit_surface();
  psp_sdl_gu_stretch(&srcRect, &dstRect);
}

static void
Display_gu_max(void)
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 32;
  srcRect.y = 0;
  srcRect.w = 320;
  srcRect.h = ATARI_HEIGHT;
  dstRect.x = 0;
  dstRect.y = 0;
  dstRect.w = back_surface->w;
  dstRect.h = back_surface->h;

  Display_fill_blit_surface();
  psp_sdl_gu_stretch(&srcRect, &dstRect);
}

static void
Display_gu_normal(void)
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = ATARI_WIDTH;
  srcRect.h = ATARI_HEIGHT;
  dstRect.x = 48;
  dstRect.y = 16;
  dstRect.w = ATARI_WIDTH;
  dstRect.h = ATARI_HEIGHT;

  Display_fill_blit_surface();
  psp_sdl_gu_stretch(&srcRect, &dstRect);
}

static void
Display_normal(void)
{
  int x_s;
  int y_s;

  uchar  *atari_top_scr = (UBYTE *)atari_screen;
  ushort *psp_top_scr   = (ushort *)MainScreen->pixels;

  uchar  *atari_scr = atari_top_scr;
  ushort *psp_scr = psp_top_scr + (16 * PSP_LINE_WIDTH);
  
  for (y_s = 0; y_s < ATARI_HEIGHT; y_s++) {
    for (x_s = 0; x_s < ATARI_WIDTH; x_s++) {
      psp_scr[x_s + 48] = Palette16[ atari_scr[x_s] ];
    }
    atari_scr += ATARI_WIDTH;
    psp_scr   += PSP_LINE_WIDTH;
  }
}

/* LUDO: */
void
atari_synchronize(void)
{
  static u32 nextclock = 1;

  if (nextclock) {
    u32 curclock;
    do {
     curclock = SDL_GetTicks();
    } while (curclock < nextclock);
    nextclock = curclock + (u32)( 1000 / ATARI.atari_speed_limiter);
  }
}

void
atari_update_fps()
{
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;
  cur_num_frame++;
  u32 curclock = SDL_GetTicks();
  if (curclock > next_sec_clock) {
    next_sec_clock = curclock + 1000;
    ATARI.atari_current_fps = cur_num_frame;
    cur_num_frame = 0;
  }
}

void
psp_atari_wait_vsync()
{
  static int loc_pv = 0;
  int cv = sceDisplayGetVcount();
  if (loc_pv == cv) {
    sceDisplayWaitVblankCB();
  }
  loc_pv = sceDisplayGetVcount();
}

void 
Atari_DisplayScreen(void)
{
  if (ATARI.psp_skip_cur_frame <= 0) {

    ATARI.psp_skip_cur_frame = ATARI.psp_skip_max_frame;

    if (ATARI.atari_render_mode == ATARI_RENDER_NORMAL   ) Display_gu_normal();
    else
    if (ATARI.atari_render_mode == ATARI_RENDER_FIT_WIDTH) Display_gu_fit_width();
    else
    if (ATARI.atari_render_mode == ATARI_RENDER_FIT      ) Display_gu_fit();
    else
    if (ATARI.atari_render_mode == ATARI_RENDER_FIT_HEIGHT) Display_gu_fit_height();
    else
    if (ATARI.atari_render_mode == ATARI_RENDER_MAX      ) Display_gu_max();

    if (psp_kbd_is_danzeff_mode()) {
      sceDisplayWaitVblankStart();
      danzeff_moveTo(-165, -50);
      danzeff_render();
    }

    if (ATARI.atari_view_fps) {
      char buffer[32];
      sprintf(buffer, "%3d", (int)ATARI.atari_current_fps);
      psp_sdl_fill_print(0, 0, buffer, 0xffffff, 0 );
    }

    if (ATARI.psp_display_lr) {
      psp_kbd_display_active_mapping();
    }
    if (ATARI.atari_vsync) {
      psp_atari_wait_vsync();
    }
    psp_sdl_flip();
  
    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_sdl_save_screenshot();
        psp_screenshot_mode = 0;
      }
    }

  } else if (ATARI.psp_skip_max_frame) {
    ATARI.psp_skip_cur_frame--;
  }

  if (ATARI.atari_speed_limiter) {
	  atari_synchronize();
  }

  if (ATARI.atari_view_fps) {
    atari_update_fps();
  }
}

# if 0 //LUDO:
int get_SDL_joystick_state(SDL_Joystick *joystick)
{
  int x;
  int y;

  x = SDL_JoystickGetAxis(joystick, 0);
  y = SDL_JoystickGetAxis(joystick, 1);

  if (x > minjoy) {
    if (y < -minjoy)
      return STICK_UR;
    else if (y > minjoy)
      return STICK_LR;
    else
      return STICK_RIGHT;
  }
  else if (x < -minjoy) {
    if (y < -minjoy)
      return STICK_UL;
    else if (y > minjoy)
      return STICK_LL;
    else
      return STICK_LEFT;
  }
  else {
    if (y < -minjoy)
      return STICK_FORWARD;
    else if (y > minjoy)
      return STICK_BACK;
    else
      return STICK_CENTRE;
  }
}
# endif

int get_LPT_joystick_state(int fd)
{
#ifdef LPTJOY
  int status;

  ioctl(fd, LPGETSTATUS, &status);
  status ^= 0x78;

  if (status & 0x40) {      /* right */
    if (status & 0x10) {    /* up */
      return STICK_UR;
    }
    else if (status & 0x20) {  /* down */
      return STICK_LR;
    }
    else {
      return STICK_RIGHT;
    }
  }
  else if (status & 0x80) {    /* left */
    if (status & 0x10) {    /* up */
      return STICK_UL;
    }
    else if (status & 0x20) {  /* down */
      return STICK_LL;
    }
    else {
      return STICK_LEFT;
    }
  }
  else {
    if (status & 0x10) {    /* up */
      return STICK_FORWARD;
    }
    else if (status & 0x20) {  /* down */
      return STICK_BACK;
    }
    else {
      return STICK_CENTRE;
    }
  }
#else
  return 0;
#endif /* LPTJOY */
}

# define JOY_UP       0
# define JOY_DOWN     1
# define JOY_LEFT     2
# define JOY_RIGHT    3
# define JOY_FIRE1    4 
# define JOY_FIRE2    5

void SDL_Atari_PORT(Uint8 *s0, Uint8 *s1)
{
  int stick0, stick1;
  stick0 = stick1 = STICK_CENTRE;

  /* 0 Up  1 Down 2 Left 3 Right 4 Fire1 5 Fire2 */
  if (kbd_joy_0_enabled) {
    if (joystick_states[0][JOY_LEFT])
      stick0 = STICK_LEFT;
    if (joystick_states[0][JOY_RIGHT])
      stick0 = STICK_RIGHT;
    if (joystick_states[0][JOY_UP])
      stick0 = STICK_FORWARD;
    if (joystick_states[0][JOY_DOWN])
      stick0 = STICK_BACK;
    if ((joystick_states[0][JOY_LEFT]) && (joystick_states[0][JOY_UP]))
      stick0 = STICK_UL;
    if ((joystick_states[0][JOY_LEFT]) && (joystick_states[0][JOY_DOWN]))
      stick0 = STICK_LL;
    if ((joystick_states[0][JOY_RIGHT]) && (joystick_states[0][JOY_UP]))
      stick0 = STICK_UR;
    if ((joystick_states[0][JOY_RIGHT]) && (joystick_states[0][JOY_DOWN]))
      stick0 = STICK_LR;
  }

  if (SWAP_JOYSTICKS) {
    *s1 = stick0;
    *s0 = stick1;
  }
  else {
    *s0 = stick0;
    *s1 = stick1;
  }
# if 0 //LUDO:
  if (kbd_joy_0_enabled) {
    if (kbhits[KBD_STICK_0_LEFT])
      stick0 = STICK_LEFT;
    if (kbhits[KBD_STICK_0_RIGHT])
      stick0 = STICK_RIGHT;
    if (kbhits[KBD_STICK_0_UP])
      stick0 = STICK_FORWARD;
    if (kbhits[KBD_STICK_0_DOWN])
      stick0 = STICK_BACK;
    if ((kbhits[KBD_STICK_0_LEFTUP])
      || ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_UP])))
      stick0 = STICK_UL;
    if ((kbhits[KBD_STICK_0_LEFTDOWN])
      || ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_DOWN])))
      stick0 = STICK_LL;
    if ((kbhits[KBD_STICK_0_RIGHTUP])
      || ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_UP])))
      stick0 = STICK_UR;
    if ((kbhits[KBD_STICK_0_RIGHTDOWN])
      || ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_DOWN])))
      stick0 = STICK_LR;
  }
  if (kbd_joy_1_enabled) {
    if (kbhits[KBD_STICK_1_LEFT])
      stick1 = STICK_LEFT;
    if (kbhits[KBD_STICK_1_RIGHT])
      stick1 = STICK_RIGHT;
    if (kbhits[KBD_STICK_1_UP])
      stick1 = STICK_FORWARD;
    if (kbhits[KBD_STICK_1_DOWN])
      stick1 = STICK_BACK;
    if ((kbhits[KBD_STICK_1_LEFTUP])
      || ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_UP])))
      stick1 = STICK_UL;
    if ((kbhits[KBD_STICK_1_LEFTDOWN])
      || ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_DOWN])))
      stick1 = STICK_LL;
    if ((kbhits[KBD_STICK_1_RIGHTUP])
      || ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_UP])))
      stick1 = STICK_UR;
    if ((kbhits[KBD_STICK_1_RIGHTDOWN])
      || ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_DOWN])))
      stick1 = STICK_LR;
  }
  if (SWAP_JOYSTICKS) {
    *s1 = stick0;
    *s0 = stick1;
  }
  else {
    *s0 = stick0;
    *s1 = stick1;
  }

  if ((joystick0 != NULL) || (joystick1 != NULL))  /* can only joystick1!=NULL ? */
  {
    SDL_JoystickUpdate();
  }

  if (fd_joystick0 != -1)
    *s0 = get_LPT_joystick_state(fd_joystick0);
  else if (joystick0 != NULL)
    *s0 = get_SDL_joystick_state(joystick0);

  if (fd_joystick1 != -1)
    *s1 = get_LPT_joystick_state(fd_joystick1);
  else if (joystick1 != NULL)
    *s1 = get_SDL_joystick_state(joystick1);
# endif
}

void SDL_Atari_TRIG(Uint8 *t0, Uint8 *t1)
{
  int trig0, trig1, i;
  trig0 = trig1 = 1;

  if (kbd_joy_0_enabled) {
    trig0 = joystick_states[0][JOY_FIRE1] ? 0 : 1;
  }

  if (kbd_joy_1_enabled) {
    trig1 = joystick_states[0][JOY_FIRE2] ? 0 : 1;
  }

  if (SWAP_JOYSTICKS) {
    *t1 = trig0;
    *t0 = trig1;
  }
  else {
    *t0 = trig0;
    *t1 = trig1;
  }

# if 0 //LUDO:
  if (kbd_joy_0_enabled) {
    trig0 = kbhits[KBD_TRIG_0] ? 0 : 1;
  }

  if (kbd_joy_1_enabled) {
    trig1 = kbhits[KBD_TRIG_1] ? 0 : 1;
  }

  if (SWAP_JOYSTICKS) {
    *t1 = trig0;
    *t0 = trig1;
  }
  else {
    *t0 = trig0;
    *t1 = trig1;
  }

  if (fd_joystick0 != -1) {
#ifdef LPTJOY
    int status;
    ioctl(fd_joystick0, LPGETSTATUS, &status);
    if (status & 8)
      *t0 = 1;
    else
      *t0 = 0;
#endif /* LPTJOY */
  }
  else if (joystick0 != NULL) {
    trig0 = 1;
    for (i = 0; i < joystick0_nbuttons; i++) {
      if (SDL_JoystickGetButton(joystick0, i)) {
        trig0 = 0;
        break;
      }
    }
    *t0 = trig0;
  }

  if (fd_joystick1 != -1) {
#ifdef LPTJOY
    int status;
    ioctl(fd_joystick1, LPGETSTATUS, &status);
    if (status & 8)
      *t1 = 1;
    else
      *t1 = 0;
#endif /* LPTJOY */
  }
  else if (joystick1 != NULL) {
    trig1 = 1;
    for (i = 0; i < joystick1_nbuttons; i++) {
      if (SDL_JoystickGetButton(joystick1, i)) {
        trig1 = 0;
        break;
      }
    }
    *t1 = trig1;
  }
# endif
}

void CountFPS(void)
{
  static int ticks1 = 0, ticks2, shortframes;
  if (ticks1 == 0)
    ticks1 = SDL_GetTicks();
  ticks2 = SDL_GetTicks();
  shortframes++;
  if (ticks2 - ticks1 > 1000) {
    ticks1 = ticks2;
    Aprint("%d fps", shortframes);
    shortframes = 0;
  }
}

int Atari_PORT(int num)
{
#ifndef DONT_DISPLAY
  if (num == 0) {
    UBYTE a, b;
    SDL_Atari_PORT(&a, &b);
    return (b << 4) | (a & 0x0f);
  }
#endif
  return 0xff;
}

int Atari_TRIG(int num)
{
#ifndef DONT_DISPLAY
  UBYTE a, b;
  SDL_Atari_TRIG(&a, &b);
  switch (num) {
  case 0:
    return a;
  case 1:
    return b;
  default:
    break;
  }
#endif
  return 1;
}

int 
SDL_main(int argc, char **argv)
{
  /* initialise Atari800 core */
  if (!Atari800_Initialise(&argc, argv))
    return 3;
  /* main loop */
  for (;;) {
    key_code = Atari_Keyboard();
    Atari800_Frame();
    if (display_screen)
      Atari_DisplayScreen();
  }
}

void 
audio_pause(void)
{
  if (ATARI.atari_snd_enable) {
    SDL_PauseAudio(1);
  }
}

void 
audio_resume(void)
{
  if (ATARI.atari_snd_enable) {
    SDL_PauseAudio(0);
  }
}

void 
audio_shutdown(void)
{
  SDL_CloseAudio();
}

