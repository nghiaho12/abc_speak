#include "audio.hpp"

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <cstdlib>

#include "log.hpp"
#include "stb_vorbis.hpp"

void Audio::play(bool clear_stream) {
    if (stream) {
        if (clear_stream) {
            SDL_ClearAudioStream(stream);
        }

        SDL_PutAudioStreamData(stream, data.data(), static_cast<int>(data.size()));
        SDL_ResumeAudioStreamDevice(stream);
    }
}

namespace {
std::vector<uint8_t> change_volume(const std::vector<uint8_t> &data, SDL_AudioSpec spec, float volume) {
    std::vector<uint8_t> ret(data.size());
    SDL_MixAudio(ret.data(), data.data(), spec.format, static_cast<Uint32>(data.size()), volume);
    return ret;
}

bool load_stream(SDL_AudioDeviceID audio_device, Audio &audio, float volume) {
    audio.stream = SDL_CreateAudioStream(&audio.spec, NULL);

    if (!audio.stream) {
        LOG("Couldn't create audio stream: %s", SDL_GetError());
        return false;
    }

    if (!SDL_BindAudioStream(audio_device, audio.stream)) {
        LOG("Failed to bind stream to device: %s", SDL_GetError());
        return false;
    }

    if (volume > 0.0f && volume < 1.0f) {
        audio.data = change_volume(audio.data, audio.spec, volume);
    }

    return true;
}

}  // namespace

std::optional<Audio> load_ogg(SDL_AudioDeviceID audio_device, const char *path, float volume) {
    // NOTE: Can't use fopen on files inside an Android APK.
    // SDL provides IO abstraction for this.
    size_t data_size;
    uint8_t *data = static_cast<uint8_t *>(SDL_LoadFile(path, &data_size));

    if (!data) {
        LOG("Failed to open file '%s'.", path);
        return {};
    }

    Audio ret;

    short *output;
    int samples =
        stb_vorbis_decode_memory(data, static_cast<int>(data_size), &ret.spec.channels, &ret.spec.freq, &output);

    ret.data.resize(static_cast<size_t>(samples * ret.spec.channels) * sizeof(short));
    memcpy(ret.data.data(), output, ret.data.size());

    free(output);
    SDL_free(data);

    ret.spec.format = SDL_AUDIO_S16LE;

    if (!load_stream(audio_device, ret, volume)) {
        return {};
    }

    return ret;
}

std::optional<Audio> load_wav(SDL_AudioDeviceID audio_device, const char *path, float volume) {
    Audio ret;

    uint8_t *data = nullptr;
    uint32_t data_len;

    if (!SDL_LoadWAV(path, &ret.spec, &data, &data_len)) {
        LOG("Failed to open file '%s'.", path);
        return {};
    }

    ret.data.assign(data, data + data_len);
    SDL_free(data);

    if (!load_stream(audio_device, ret, volume)) {
        return {};
    }

    return ret;
}
