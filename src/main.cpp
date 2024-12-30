#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <glm/gtc/type_ptr.hpp>
#define SDL_MAIN_USE_CALLBACKS  // use the callbacks instead of main()
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengles2.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <optional>
#include <random>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "audio.hpp"
#include "color_palette.hpp"
#include "font.hpp"
#include "geometry.hpp"
#include "gl_helper.hpp"
#include "log.hpp"
#include "vosk_api.h"

// All co-ordinates used are normalized as follows
// x: [0.0, 1.0]
// y: [0.0, 1/ASPECT_RATIO]
// origin at top-left

constexpr float ASPECT_RATIO = 16.f / 9.f;
constexpr float NORM_WIDTH = 1.f;
constexpr float NORM_HEIGHT = 1.f / ASPECT_RATIO;

constexpr glm::vec4 BG_COLOR = Color::darkgrey;

constexpr glm::vec4 FONT_BG = Color::transparent;
constexpr glm::vec4 FONT_OUTLINE = Color::white;
constexpr float FONT_OUTLINE_FACTOR = 0.1f;
constexpr float FONT_WIDTH = 0.15f;

enum class AudioEnum { BGM, CLICK, CLAP, WIN };

using VoskModelPtr =  std::unique_ptr<VoskModel, void (*)(VoskModel *)>;
using VoskRecognizerPtr =  std::unique_ptr<VoskRecognizer, void (*)(VoskRecognizer*)>;

struct AppState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_GLContext gl_ctx;
    SDL_AudioDeviceID audio_device = 0;
    SDL_AudioDeviceID recording_device = 0;
    SDL_AudioStream *recording_stream = nullptr;

    std::vector<char> record_buf;
    std::map<AudioEnum, Audio> audio;

    VoskModelPtr model{{}, {}};
    VoskRecognizerPtr recognizer{{}, {}};

    bool init = false;

    VertexArrayPtr vao{{}, {}};

    FontAtlas font;
    FontShader font_shader;

    ShapeShader shape_shader;
    Shape draw_area_bg;

    VertexBufferPtr letter{{}, {}};
    std::array<glm::vec2, 26> letter_pos;

    char spoken_letter = 0;
};

bool resize_event(AppState &as) {
    int win_w, win_h;

    if (!SDL_GetWindowSize(as.window, &win_w, &win_h)) {
        LOG("%s", SDL_GetError());
        return false;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_canvas_element_size("#canvas", win_w, win_h);
#endif

    float win_wf = static_cast<float>(win_w);
    float win_hf = static_cast<float>(win_h);

    glm::vec2 draw_area_size;
    glm::vec2 draw_area_offset;

    if (win_w > win_h) {
        draw_area_size.y = win_hf;
        draw_area_size.x = win_hf * ASPECT_RATIO;
        draw_area_offset.x = (win_wf - draw_area_size.x) / 2;
        draw_area_offset.y = 0;
    } else {
        draw_area_size.x = win_wf;
        draw_area_size.y = win_wf / ASPECT_RATIO;
        draw_area_offset.x = 0;
        draw_area_offset.y = (win_hf - draw_area_size.y) / 2;
    }

    auto norm_x = [=](float x) { return (x - draw_area_offset.x) / draw_area_size.x; };
    auto norm_y = [=](float y) { return (y - draw_area_offset.y) / draw_area_size.x; };

    glViewport(0, 0, win_w, win_h);
    glm::mat4 ortho = glm::ortho(norm_x(0.f), norm_x(win_wf), norm_y(win_hf), norm_y(0.f));

    as.shape_shader.set_ortho(ortho);
    as.shape_shader.draw_area_size = draw_area_size;
    as.shape_shader.draw_area_offset = draw_area_offset;

    as.font_shader.set_ortho(ortho);
    as.font_shader.set_display_width(draw_area_size.x);

    return true;
}

void init_game(AppState &as) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dice(0, 9);

    resize_event(as);
}

bool init_audio(AppState &as, const std::string &base_path) {
    as.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (as.audio_device == 0) {
        LOG("Couldn't open audio device: %s", SDL_GetError());
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "bgm.ogg").c_str(), 0.2f)) {
        as.audio[AudioEnum::BGM] = *w;
    } else {
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "win.ogg").c_str())) {
        as.audio[AudioEnum::WIN] = *w;
    } else {
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "clap.ogg").c_str())) {
        as.audio[AudioEnum::CLAP] = *w;
    } else {
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "switch30.ogg").c_str())) {
        as.audio[AudioEnum::CLICK] = *w;
    } else {
        return false;
    }

    SDL_AudioSpec spec{};
    spec.freq = 16000;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;

    as.recording_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec);

    if (as.recording_device == 0) {
        LOG("Failed to open recording device: %s", SDL_GetError());
        return false;
    }

    as.recording_stream = SDL_CreateAudioStream(nullptr, &spec);

    if (!as.recording_stream) {
        LOG("Couldn't create recording stream: %s", SDL_GetError());
        return false;
    }

    if (!SDL_BindAudioStream(as.recording_device, as.recording_stream)) {
        LOG("Failed to bind stream to device: %s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioStreamDevice(as.recording_stream);

    return true;
}

bool init_font(AppState &as, const std::string &base_path) {
    if (!as.font.load(base_path + "atlas.bmp", base_path + "atlas.txt")) {
        return false;
    }

    if (!as.font_shader.init(as.font)) {
        return false;
    }

    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    // Unused
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOG("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = new AppState();

    if (!as) {
        LOG("can't alloc memory for AppState");
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    std::string base_path = "assets/";
#ifdef __ANDROID__
    base_path = "";
#endif

    if (!init_audio(*as, base_path)) {
        return SDL_APP_FAILURE;
    }

    as->model = VoskModelPtr(vosk_model_new("/home/nghia/.cache/vosk/vosk-model-small-en-us-0.15"), 
        [](VoskModel *model) {        
            LOG("freeing vosk model");
            vosk_model_free(model);
        }
    );

    std::string grammar = "[\"a b c d e f g h i j k l m n o p q r s t u v w x y z\", \"[unk]\"]";   
    as->recognizer = VoskRecognizerPtr(vosk_recognizer_new_grm(as->model.get(), 16000.0, grammar.c_str()),
        [](VoskRecognizer *recognizer) {        
            LOG("freeing vosk recognizer");
            vosk_recognizer_free(recognizer);
        }
    );

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

    // Android
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    if (!SDL_CreateWindowAndRenderer(
            "Number Sequence Game", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
        LOG("SDL_CreateWindowAndRenderer failed");
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderVSync(as->renderer, 1)) {
        LOG("SDL_SetRenderVSync failed");
        return SDL_APP_FAILURE;
    }

#ifndef __EMSCRIPTEN__
    as->gl_ctx = SDL_GL_CreateContext(as->window);
    SDL_GL_MakeCurrent(as->window, as->gl_ctx);
    enable_gl_debug_callback();
#endif

    if (!init_font(*as, base_path)) {
        return SDL_APP_FAILURE;
    }

    if (!as->shape_shader.init()) {
        return SDL_APP_FAILURE;
    }

    as->vao = make_vertex_array();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // background color for drawing area
    {
        float h = 1.0f / ASPECT_RATIO;

        std::vector<glm::vec2> vertex{
            {0.f, 0.f},
            {1.f, 0.f},
            {1.f, h},
            {0.f, h},
        };

        as->draw_area_bg = make_shape(vertex, 0, {}, BG_COLOR);
    }

    init_game(*as);

    std::tie(as->letter, std::ignore) = as->font.make_text("A", true);

    int rows = 4;
    int cols = 7;
    float xoff = 0.02f;

    size_t count = 0;
    for (int i=0; i < rows; i++) {
        for (int j=0; j < cols; j++) {
            float x = xoff + (static_cast<float>(j) / static_cast<float>(cols)) * NORM_WIDTH;
            float y = (static_cast<float>(i) + 0.8f) / static_cast<float>(rows) * NORM_HEIGHT;

            as->letter_pos[count] = {x, y};

            if (count >= 26) {
                break;
            }

            count++;
        }

        if (count >= 26) {
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState &as = *static_cast<AppState *>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
#ifndef __EMSCRIPTEN__
            if (event->key.key == SDLK_ESCAPE) {
                SDL_Quit();
                return SDL_APP_SUCCESS;
            }
#endif
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            resize_event(as);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    // Unused
    (void)result;

    if (appstate) {
        AppState &as = *static_cast<AppState *>(appstate);
        SDL_DestroyRenderer(as.renderer);
        SDL_DestroyWindow(as.window);

        SDL_CloseAudioDevice(as.audio_device);

        // TODO: This code causes a crash as of libSDL preview-3.1.6
        // for (auto &a: as.audio) {
        //     if (a.second.stream) {
        //         SDL_DestroyAudioStream(a.second.stream);
        //     }
        // }

        delete &as;
    }
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState &as = *static_cast<AppState *>(appstate);

    auto &bgm = as.audio[AudioEnum::BGM];
    if (SDL_GetAudioStreamAvailable(bgm.stream) < static_cast<int>(bgm.data.size())) {
        bgm.play(false);
    }

    std::vector<char> buf(32000);
    int bytes = SDL_GetAudioStreamData(as.recording_stream, buf.data(), static_cast<int>(buf.size()));
    as.record_buf.insert(as.record_buf.end(), buf.begin(), buf.begin() + bytes);

    if (as.record_buf.size() > static_cast<size_t>(2 * 16000 * 0.5)) {
        auto parse_json = [](std::string str) {
            size_t start = 0;
            size_t end = 0;

            size_t i = 0;
            int count = 0;

            for (auto ch: str) {
                if (ch == '"') {
                    count++;

                    if (count == 3) {
                        start = i;
                    } else if (count == 4) {
                        end = i;
                        break;
                    }
                }
                i++;
            }

            return str.substr(start + 1, end - start - 1);
        };

        int done = vosk_recognizer_accept_waveform(as.recognizer.get(), as.record_buf.data(), static_cast<int>(as.record_buf.size()));

        std::string word;
        if (done) {
            word = parse_json(vosk_recognizer_result(as.recognizer.get()));
        } else {
            // word = parse_json(vosk_recognizer_partial_result(as.recognizer.get()));
        }

        // LOG("time: %f", (SDL_GetTicksNS() - s)*1e-6);
        if (!word.empty()) {
            LOG("word: %s", word.c_str());
        }

        if (word == "a") {
            as.spoken_letter = 'A';
        } else if (word == "b" || word == "be") {
            as.spoken_letter = 'B';
        } else if (word == "c" || word == "see") {
            as.spoken_letter = 'C';
        } else if (word == "d" || word == "the") {
            as.spoken_letter = 'D';
        } else if (word == "e" || word == "he") {
            as.spoken_letter = 'E';
        } else if (word == "f" || word == "if") {
            as.spoken_letter = 'F';
        } else if (word == "g" || word == "gee") {
            as.spoken_letter = 'G';
        } else if (word == "h") {
            as.spoken_letter = 'H';
        } else if (word == "i" || word == "hi") {
            as.spoken_letter = 'I';
        } else if (word == "j" || word == "jay") {
            as.spoken_letter = 'J';
        } else if (word == "k" || word == "hey") {
            as.spoken_letter = 'K';
        } else if (word == "l" || word == "hell") {
            as.spoken_letter = 'L';
        } else if (word == "m" || word == "em") {
            as.spoken_letter = 'M';
        } else if (word == "n" || word == "in" || word == "and") {
            as.spoken_letter = 'N';
        } else if (word == "o" || word == "oh") {
            as.spoken_letter = 'O';
        } else if (word == "p") {
            as.spoken_letter = 'P';
        } else if (word == "q") {
            as.spoken_letter = 'Q';
        } else if (word == "r" || word == "ah") {
            as.spoken_letter = 'R';
        } else if (word == "s") {
            as.spoken_letter = 'S';
        } else if (word == "t" || word == "te") {
            as.spoken_letter = 'T';
        } else if (word == "u" || word == "you") {
            as.spoken_letter = 'U';
        } else if (word == "v") {
            as.spoken_letter = 'V';
        } else if (word == "w") {
            as.spoken_letter = 'W';
        } else if (word == "x" || word == "ex") {
            as.spoken_letter = 'X';
        } else if (word == "y" || word == "why") {
            as.spoken_letter = 'Y';
        } else if (word == "z" || word == "zebra") {
            as.spoken_letter = 'Z';
        }

        as.record_buf.clear();
    }

#ifndef __EMSCRIPTEN__
    SDL_GL_MakeCurrent(as.window, as.gl_ctx);
#endif

    as.shape_shader.shader->use();

    if (!as.init) {
        resize_event(as);
        as.init = true;
    }

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    as.vao->use();

    draw_shape(as.shape_shader, as.draw_area_bg, true, false, false);

    as.font_shader.set_bg(FONT_BG);
    as.font_shader.set_outline(FONT_OUTLINE);
    as.font_shader.set_outline_factor(FONT_OUTLINE_FACTOR);

    const std::vector<glm::vec4> color{
        Color::blue,
        Color::orange,
        Color::red,
        Color::teal,
        Color::green,
        Color::yellow,
        Color::purple,
        Color::pink,
        Color::brown,
    };

    for (size_t i=0; i < 26; i++) {
        if (as.spoken_letter == 'A' + i) {
            as.font_shader.set_font_width(FONT_WIDTH*1.5);
        } else {
            as.font_shader.set_font_width(FONT_WIDTH);
        }

        as.font_shader.set_fg(color[i % color.size()]);
        as.font_shader.set_trans(as.letter_pos[i]);

        std::string str;
        str = 'A' + static_cast<char>(i);
        auto [vertex_uv, bbox] = as.font.make_text_vertex(str, true);
        as.letter->update_vertex(glm::value_ptr(vertex_uv[0]), sizeof(glm::vec4) * vertex_uv.size());
        draw_vertex_buffer(as.font_shader.shader, as.letter, as.font.tex);
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
