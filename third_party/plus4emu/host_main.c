#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include "plus4lib/plus4emu.h"

static Plus4VM            *vm = NULL;
static Plus4VideoDecoder  *videoDecoder = NULL;

static uint8_t          *fb_buf;
static int              fb_pitch;
static int              ui_trap;
static int              ui_warp;
static int              joy_latch_value[2];

#define COLOR16(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

static void errorMessage(const char *fmt, ...)
{
  va_list args;
  fprintf(stderr, " *** Plus/4 error: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  Plus4VM_Destroy(vm);
  exit(-1);
}

static void vmError(void)
{
  fprintf(stderr, " *** Plus/4 error: %s\n", Plus4VM_GetLastErrorMessage(vm));
  Plus4VM_Destroy(vm);
  exit(-1);
}

static void audioOutputCallback(void *userData,
                                const int16_t *buf, size_t nFrames)
{
  //if (!ui_warp)
  //   circle_sound_write((int16_t*)buf, nFrames);
}

static void videoLineCallback(void *userData,
                              int lineNum, const Plus4VideoLineData *lineData)
{
   lineNum = lineNum / 2;
   if (lineNum >= 0 && lineNum < 288) {
     Plus4VideoDecoder_DecodeLine(videoDecoder, fb_buf + lineNum * fb_pitch, 384, lineData);
   }
}

static long count;
static void videoFrameCallback(void *userData)
{
#ifndef HOST_BUILD
  circle_frames_ready_fbl(FB_LAYER_VIC,
                          -1 /* no 2nd layer */,
                          !ui_warp /* sync */);
#endif
  count++;
  if (count == 1000) {printf ("%ld\n",count); count = 0; }
}

static void resetMemoryConfiguration(void)
{
  if (Plus4VM_SetRAMConfiguration(vm, 64, 0x99999999UL) != PLUS4EMU_SUCCESS)
    vmError();
  /* load ROM images */
  if (Plus4VM_LoadROM(vm, 0x00, "p4_basic.rom", 0) != PLUS4EMU_SUCCESS)
    errorMessage("cannot load p4_basic.rom");
  if (Plus4VM_LoadROM(vm, 0x01, "p4kernal.rom", 0) != PLUS4EMU_SUCCESS)
    errorMessage("cannot load p4kernal.rom");
  if (Plus4VM_LoadROM(vm, 0x10, "dos1541.rom", 0) != PLUS4EMU_SUCCESS)
    errorMessage("cannot load dos1541.rom");
}

// This is made to look like VICE's main entry point so our
// Plus4Emu version of EmulatorCore can look more or less the same
// as the Vice version.
int main_program(int argc, char **argv)
{
  int     quitFlag = 0;

  (void) argc;
  (void) argv;

  printf ("Init\n");

#ifndef HOST_BUILD
  // BMC64 Video Init
  if (circle_alloc_fbl(FB_LAYER_VIC, 1 /* RGB565 */, &fb_buf,
                              384, 288, &fb_pitch)) {
    printf ("Failed to create video buf.\n");
    assert(0);
  }
  circle_clear_fbl(FB_LAYER_VIC);
  circle_show_fbl(FB_LAYER_VIC);
#else
  fb_buf = (uint8_t*) malloc(384*288*2);
  fb_pitch = 384;
#endif

  vm = Plus4VM_Create();
  if (!vm)
    errorMessage("could not create Plus/4 emulator object");

  Plus4VM_SetAudioOutputCallback(vm, &audioOutputCallback, NULL);
  if (Plus4VM_SetAudioOutputQuality(vm, 1) != PLUS4EMU_SUCCESS)
    vmError();

  int audioSampleRate;
  int fragsize;
  int fragnr;
  int channels;

  //circle_sound_init(NULL, &audioSampleRate, &fragsize, &fragnr, &channels);
  if (Plus4VM_SetAudioSampleRate(vm, audioSampleRate) != PLUS4EMU_SUCCESS)
    vmError();
  resetMemoryConfiguration();
  if (Plus4VM_SetWorkingDirectory(vm, ".") != PLUS4EMU_SUCCESS)
    vmError();
  /* enable read-write IEC level drive emulation for unit 8 */
  Plus4VM_SetIECDriveReadOnlyMode(vm, 0);
  //emux_detach_disk(0);
  if (Plus4VM_SetDiskImageFile(vm, 0, "../advmono.d64", 0) != PLUS4EMU_SUCCESS) {
     printf ("Could not attach disk\n");
  }
 

  Plus4VM_Reset(vm, 1);

  videoDecoder =
      Plus4VideoDecoder_Create(&videoLineCallback, &videoFrameCallback, NULL);
  if (!videoDecoder)
    errorMessage("could not create video decoder object");
  //Plus4VideoDecoder_UpdatePalette(videoDecoder, 0, 16, 8, 0); // not using rgb
  Plus4VM_SetVideoOutputCallback(vm, &Plus4VideoDecoder_VideoCallback,
                                 (void *) videoDecoder);

  //vic_enabled = 1; // really TED
  //ui_init_menu();

  // Here, we should make whatever calls are necessary to configure the VM
  // according to any settings that were loaded.
  //apply_sid_config();

  /* run Plus/4 emulation until the F12 key is pressed */
  printf ("Enter emulation loop\n");

  do {
    if (Plus4VM_Run(vm, 2000) != PLUS4EMU_SUCCESS)
      vmError();

  } while (!quitFlag);

  Plus4VM_Destroy(vm);
  Plus4VideoDecoder_Destroy(videoDecoder);
  return 0;
}

// Begin emu_api impl.


#ifdef HOST_BUILD

int circle_sound_init(const char *param, int *speed, int *fragsize,
                        int *fragnr, int *channels) {
   *speed = 48000;
   *fragsize = 2048;
   *fragnr = 16;
   *channels = 1;
}

int circle_sound_write(int16_t *pbuf, size_t nr) {
}

int main(int argc, char *argv[]) {
  main_program(argc, argv);
}

#endif

