#pragma once

typedef unsigned char uint8;

extern "C" {
int stb_vorbis_decode_memory(const uint8 *mem, int len, int *channels, int *sample_rate, short **output);
}
