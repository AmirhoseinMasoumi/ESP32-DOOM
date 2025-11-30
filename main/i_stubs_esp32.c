// ESP32 DOOM Port - Additional Function Stubs
// Stubs for functions that are not needed or not yet implemented on ESP32

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "doomtype.h"

// ===== Network stubs (multiplayer disabled) =====
// Note: D_RegisterLoopCallbacks, D_StartNetGame, D_InitNetGame, TryRunTics, NetUpdate are
// implemented in d_loop.c - do NOT stub them here!
void NET_BindVariables(void) {}
void NET_Init(void) {}
// D_InitNetGame is implemented in d_loop.c - DO NOT STUB IT
void NET_DedicatedServer(void) {}
void NET_MasterQuery(void) {}
void NET_QueryAddress(const char *addr) { (void)addr; }
void NET_LANQuery(void) {}
void D_StartGameLoop(void) {}
int D_PopEvent(void *event) { (void)event; return 0; }

// ===== Input stubs =====
void I_BindInputVariables(void) {}
void I_BindJoystickVariables(void) {}
void I_InitJoystick(void) {}
void I_ReadMouse(void) {}
void I_StartTextInput(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
void I_StopTextInput(void) {}

// ===== Sound and Music stubs (audio disabled) =====
void I_BindSoundVariables(void) {}
void I_InitSound(boolean use_sfx_prefix) { (void)use_sfx_prefix; }
void I_InitMusic(void) {}
void I_ShutdownSound(void) {}
void I_ShutdownMusic(void) {}
boolean I_SoundIsPlaying(int handle) { (void)handle; return false; }
void I_StopSound(int handle) { (void)handle; }
int I_GetSfxLumpNum(void *sfxinfo) { (void)sfxinfo; return 0; }
int I_StartSound(void *sfxinfo, int channel, int vol, int sep) { (void)sfxinfo; (void)channel; (void)vol; (void)sep; return 0; }
void I_PauseSong(void) {}
void I_ResumeSong(void) {}
void I_UpdateSound(void) {}
void I_UpdateSoundParams(int handle, int vol, int sep) { (void)handle; (void)vol; (void)sep; }
void I_SetMusicVolume(int volume) { (void)volume; }
void I_SetOPLDriverVer(int ver) { (void)ver; }
void I_PrecacheSounds(void *sounds, int num_sounds) { (void)sounds; (void)num_sounds; }
void I_StopSong(void) {}
void I_UnRegisterSong(void *handle) { (void)handle; }
void *I_RegisterSong(void *data, int len) { (void)data; (void)len; return NULL; }
void I_PlaySong(void *handle, boolean looping) { (void)handle; (void)looping; }

// ===== Video stubs =====
void I_RegisterWindowIcon(const unsigned int *icon, int width, int height) { (void)icon; (void)width; (void)height; }
byte I_GetPaletteIndex(int r, int g, int b) { (void)r; (void)g; (void)b; return 0; }

// ===== Misc stubs =====
void StatDump(void) {}
void StatCopy(void *dest) { (void)dest; }
int D_NonVanillaRecord(boolean advance_demo, char *name) { (void)advance_demo; (void)name; return 0; }
int D_NonVanillaPlayback(int lumpnum, int length, void *versionptr) { (void)lumpnum; (void)length; (void)versionptr; return 0; }
void D_Endoom(void) {}

// ===== DeHackEd stubs =====
char **I_OPL_DevMessages(void) { static char *msgs[] = {"No OPL", NULL}; return msgs; }
void *I_StartMultiGlob(const char *directory, const char *glob) { (void)directory; (void)glob; return NULL; }
char *I_NextGlob(void *glob) { (void)glob; return NULL; }
void I_EndGlob(void *glob) { (void)glob; }
void PadRejectArray(byte **array, unsigned int *len) { (void)array; (void)len; }

// ===== Cheat code stubs =====
int cht_CheckCheat(void *cht, char key) { (void)cht; (void)key; return 0; }
char cht_GetParam(void *cht, char *buffer) { (void)cht; (void)buffer; return 0; }

// ===== Global variables that need to be defined =====
// Note: gametic and singletics are defined in d_loop.c - do not duplicate
int screenvisible = 1;
int ticdup = 1;
int vanilla_keyboard_mapping = 1;
int usegamma = 0;
int usemouse = 0;
int joywait = 0;
int use_analog = 0;
int joystick_move_sensitivity = 5;
int joystick_turn_sensitivity = 5;
int snd_pitchshift = 0;
int snd_musicdevice = 0;
int drone = 0;
int screensaver_mode = 0;

// Mouse variables
float mouse_acceleration = 2.0f;
int mouse_threshold = 10;

// DeHackEd variables
int deh_initial_health = 100;
int deh_initial_bullets = 50;
int deh_green_armor_class = 1;
int deh_blue_armor_class = 2;
int deh_max_health = 100;
int deh_max_armor = 200;
int deh_soulsphere_health = 100;
int deh_max_soulsphere = 200;
int deh_megasphere_health = 200;
int deh_god_mode_health = 100;
int deh_idfa_armor = 200;
int deh_idfa_armor_class = 2;
int deh_idkfa_armor = 200;
int deh_idkfa_armor_class = 2;
int deh_bfg_cells_per_shot = 40;
int deh_species_infighting = 0;

// Video buffer (moved to PSRAM to avoid DRAM overflow)
byte *I_VideoBuffer = NULL;

// Section types for DeHackEd
void *deh_section_types = NULL;
void *deh_signatures = NULL;
