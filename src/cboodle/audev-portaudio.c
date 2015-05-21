#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <portaudio/portaudio.h>

#include "common.h"
#include "audev.h"

static PaStream *stream;
static int soundrate;
static int samplesperbuf;
static int framesperbuf;
static long *incomingBuffer;
static short *outgoingBuffer;

int audev_init_device(char *devname, long _soundrate, int verbose, extraopt_t *extra)
{
  PaStreamParameters outputParameters;
  PaError err;
  int fragsize = 32768;
  int sound_channels = 2;
  extraopt_t *opt;

  for (opt=extra; opt->key; opt++) {
    if (!strcmp(opt->key, "buffersize") && opt->val) {
      fragsize = atol(opt->val);
    }
    else if (!strcmp(opt->key, "listdevices")) {
      //TODO: implement listdevices
      printf("not implemented\n");
      return FALSE;
    }
    //TODO: can sound_channels be changed?
  }

  err = Pa_Initialize();
  if (err != paNoError) {
    fprintf(stderr, "PA %d: %s\n", err, Pa_GetErrorText(err));
    Pa_Terminate();
    return FALSE;
  }

  //TODO: allow user to select device
  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    fprintf(stderr, "No default output device.\n");
    Pa_Terminate();
    return FALSE;
  }

  outputParameters.channelCount = sound_channels;
  outputParameters.sampleFormat = paInt16;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  samplesperbuf = fragsize * sizeof(int) / sizeof(long);
  framesperbuf = samplesperbuf / 2;
  soundrate = _soundrate;
  if (verbose) {
    fprintf(stderr, "Requested rate: %d\n", soundrate);
  }
  if (soundrate == 0) {
    soundrate = 44100;
    if (verbose) {
      fprintf(stderr, "Changed rate to %d\n", soundrate);
    }
  }

  incomingBuffer = (long *)malloc(sizeof(long) * samplesperbuf);
  if (!incomingBuffer) {
    fprintf(stderr, "Unable to allocate sound buffer.\n");
    Pa_Terminate();
    return FALSE;
  }

  outgoingBuffer = (short *)malloc(sizeof(short) * samplesperbuf);
  if (!incomingBuffer) {
    fprintf(stderr, "Unable to allocate sound buffer.\n");
    Pa_Terminate();
    free(incomingBuffer);
    return FALSE;
  }

  err = Pa_OpenStream(&stream, NULL, &outputParameters, (double)soundrate, (unsigned long)framesperbuf / sound_channels, paClipOff, NULL, NULL);
  if (err != paNoError) {
    fprintf(stderr, "PA %d: %s\n", err, Pa_GetErrorText(err));
    Pa_Terminate();
    free(incomingBuffer);
    free(outgoingBuffer);
    return FALSE;
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "PA %d: %s\n", err, Pa_GetErrorText(err));
    Pa_Terminate();
    free(incomingBuffer);
    free(outgoingBuffer);
    return FALSE;
  }

  return TRUE;
}

long audev_get_soundrate()
{
  return soundrate;
}

long audev_get_framesperbuf()
{
  return framesperbuf;
}

int audev_loop(mix_func_t mixfunc, generate_func_t genfunc, void *rock)
{
  PaError err;
  int res, i;
  short *output;
  long *input;
  while (1) {
    res = mixfunc(incomingBuffer, genfunc, rock);
    if (res)
      return TRUE;

    output = outgoingBuffer;
    input = incomingBuffer;
    for (i = samplesperbuf; i > 0; i--) {
      *(output++) = (short)*(input++);
    }

    err = Pa_WriteStream(stream, outgoingBuffer, framesperbuf);
    if (err != paNoError) {
      fprintf(stderr, "PA %d: %s\n", err, Pa_GetErrorText(err));
      return FALSE;
    }
  }
}

void audev_close_device()
{
  Pa_CloseStream(stream);
  Pa_Terminate();
  free(incomingBuffer);
  free(outgoingBuffer);
}
