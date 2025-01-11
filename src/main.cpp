#ifdef WIN32
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengles2.h>
#endif

#define SDL_MAIN_USE_CALLBACKS  // use the callbacks instead of main()
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_system.h>
#include <SDL3/SDL_iostream.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

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
const char *VOSK_MODEL = "vosk-model-small-en-us-0.15";

constexpr float ASPECT_RATIO = 16.f / 9.f;
constexpr float NORM_WIDTH = 1.f;
constexpr float NORM_HEIGHT = 1.f / ASPECT_RATIO;

constexpr glm::vec4 BG_COLOR = Color::darkgrey;

constexpr glm::vec4 FONT_BG = Color::transparent;
constexpr glm::vec4 FONT_OUTLINE = Color::white;
constexpr float FONT_OUTLINE_FACTOR = 0.1f;
constexpr float FONT_WIDTH = 0.15f;

constexpr int AUDIO_RATE = 16000;

using VoskModelPtr = std::unique_ptr<VoskModel, void (*)(VoskModel *)>;
using VoskRecognizerPtr = std::unique_ptr<VoskRecognizer, void (*)(VoskRecognizer *)>;

struct AppState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_GLContext gl_ctx;
    SDL_AudioDeviceID audio_device = 0;
    SDL_AudioStream *recording_stream = nullptr;

    VoskModelPtr model{{}, {}};
    VoskRecognizerPtr recognizer{{}, {}};

    bool init = false;

    VertexArrayPtr vao{{}, {}};

    FontAtlas font;
    FontShader font_shader;

    ShapeShader shape_shader;
    Shape draw_area_bg;

    VertexBufferPtr letter{{}, {}};
    std::array<glm::vec2, 26> letter_center;

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

void record_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    (void)additional_amount;

    AppState &as = *static_cast<AppState *>(userdata);

    if (!as.recognizer) {
        return;
    }

    std::vector<char> buf(static_cast<size_t>(total_amount));
    SDL_GetAudioStreamData(stream, buf.data(), total_amount);

    auto parse_json = [](std::string str) {
        size_t start = 0;
        size_t end = 0;

        size_t i = 0;
        int count = 0;

        for (auto ch : str) {
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

    int done = vosk_recognizer_accept_waveform(as.recognizer.get(), buf.data(), total_amount);

    std::string word;
    if (done) {
        word = parse_json(vosk_recognizer_final_result(as.recognizer.get()));
        vosk_recognizer_reset(as.recognizer.get());
    } else {
        word = parse_json(vosk_recognizer_partial_result(as.recognizer.get()));
    }

    if (!word.empty()) {
        // LOG("Word: %s", word.c_str());
    }

    if (!word.empty() && word != "[unk]") {
        // if the final word contains multiple words pick the letter of the last word
        std::reverse(word.begin(), word.end());

        for (auto ch: word) {
            if (ch == ' ') {
                break;
            }
            as.spoken_letter = static_cast<char>(std::toupper(ch));
        }
    }
}

bool init_audio(AppState &as) {
    SDL_AudioSpec spec{};
    spec.freq = AUDIO_RATE;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;

    as.recording_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, record_callback, &as);

    if (!as.recording_stream) {
        LOG("Couldn't create recording stream: %s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioStreamDevice(as.recording_stream);

    return true;
}

bool init_font(AppState &as, const std::string &asset_path) {
    if (!as.font.load(asset_path + "atlas.bmp", asset_path + "atlas.txt")) {
        return false;
    }

    if (!as.font_shader.init(as.font)) {
        return false;
    }

    return true;
}

bool init_vosk_android() {
#ifdef __ANDROID__
    std::string vosk_path = std::string(SDL_GetAndroidExternalStoragePath()) + "/" + VOSK_MODEL;

    for (auto subdir: {"conf", "am", "graph/phones", "ivector"}) {
        SDL_PathInfo info;
        std::string path = vosk_path + "/" + subdir;

        if (!SDL_GetPathInfo(path.c_str(), &info)) {
            if (SDL_CreateDirectory(path.c_str())) {
                LOG("creating dir: %s", path.c_str());
            } else {
                LOG("can't create dir: %s", path.c_str());
                return false;
            }
        }
    }

    const std::vector<std::string> files{
        "conf/model.conf",
        "conf/mfcc.conf",
        "am/final.mdl",
        "graph/Gr.fst",
        "graph/HCLr.fst",
        "graph/phones/word_boundary.int",
        "graph/disambig_tid.int",
        "ivector/online_cmvn.conf",
        "ivector/final.mat",
        "ivector/splice.conf",
        "ivector/global_cmvn.stats",
        "ivector/final.dubm",
        "ivector/final.ie",
    };

    for (auto f: files) {
        std::string in_path = std::string(VOSK_MODEL) + "/" + f;
        std::string out_path = vosk_path + "/" + f;

        SDL_PathInfo info;
        if (SDL_GetPathInfo(out_path.c_str(), &info)) {
            LOG("Vosk model file exists: %s", out_path.c_str());
        } else {
            LOG("Copying Vosk model file to: %s", out_path.c_str());

            size_t datasize;
            void *data = SDL_LoadFile(in_path.c_str(), &datasize);

            if (!data) {
                LOG("can't open file: %s", in_path.c_str());
                return false;
            }

            if (!SDL_SaveFile(out_path.c_str(), data, datasize)) {
                LOG("can't save file: %s", out_path.c_str());
                return false;
            }
        }
    }
#endif
    return true;
}

bool init_vosk_model(AppState &as, const std::string &model_path) {
    as.model =
        VoskModelPtr(vosk_model_new((model_path + VOSK_MODEL).c_str()), [](VoskModel *model) {
                LOG("freeing vosk model");
                vosk_model_free(model);
                });

    if (!as.model) {
        LOG("can't load model at: %s", (model_path + VOSK_MODEL).c_str());
        return false;
    }

    std::vector<std::string> words;

    for (int i=0; i < 26; i++) {
        char ch[] = {static_cast<char>('a' + i)};
        words.push_back(ch);
    }

    words.push_back("alfa");
    words.push_back("bravo");
    words.push_back("charlie");
    words.push_back("delta");
    words.push_back("echo");
    words.push_back("foxtrot");
    words.push_back("golf");
    words.push_back("hotel");
    words.push_back("india");
    words.push_back("juliet");
    words.push_back("kilo");
    words.push_back("lima");
    words.push_back("mike");
    words.push_back("november");
    words.push_back("oscar");
    words.push_back("papa");
    words.push_back("quebec");
    words.push_back("romeo");
    words.push_back("sierra");
    words.push_back("tango");
    words.push_back("uniform");
    words.push_back("victor");
    words.push_back("whiskey");
    words.push_back("xray");
    words.push_back("yankee");
    words.push_back("zulu");

    std::string grammar("[");

    for (auto &w : words) {
        grammar.append("\"");
        grammar.append(w);
        grammar.append("\",");
    }
    grammar.append("\"[unk]\"]");

    as.recognizer = VoskRecognizerPtr(vosk_recognizer_new_grm(as.model.get(), AUDIO_RATE, grammar.c_str()),
            [](VoskRecognizer *recognizer) {
            LOG("freeing vosk recognizer");
            vosk_recognizer_free(recognizer);
            });

    vosk_recognizer_set_endpointer_mode(as.recognizer.get(), VOSK_EP_ANSWER_SHORT);

    LOG("model loaded");

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

    std::string asset_path = "assets/";
    std::string model_path = "assets/";

#ifdef __ANDROID__
    asset_path = "";
    if (!init_vosk_android()) {
        return SDL_APP_FAILURE;
    }

    model_path = std::string(SDL_GetAndroidExternalStoragePath()) + "/";
#endif

    if (!init_vosk_model(*as, model_path)) {
        return SDL_APP_FAILURE;
    }

    if (!init_audio(*as)) {
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Android
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    if (!SDL_CreateWindowAndRenderer(
            "ABC Speak", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS, &as->window, &as->renderer)) {
        LOG("SDL_CreateWindowAndRenderer failed");
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderVSync(as->renderer, 1)) {
        LOG("SDL_SetRenderVSync failed");
        return SDL_APP_FAILURE;
    }

    SDL_SetWindowFullscreen(as->window, true);

#ifndef __EMSCRIPTEN__
    as->gl_ctx = SDL_GL_CreateContext(as->window);
    SDL_GL_MakeCurrent(as->window, as->gl_ctx);
    enable_gl_debug_callback();
#endif

#ifdef WIN32
    glewInit();
#endif

    if (!init_font(*as, asset_path)) {
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

    // letter position
    {
        std::tie(as->letter, std::ignore) = as->font.make_text("A", true);

        int rows = 4;
        int cols = 7;
        float xoff = FONT_WIDTH * 0.5f;
        float yoff = FONT_WIDTH * 0.5f;

        size_t count = 0;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                float x = xoff + (static_cast<float>(j) / static_cast<float>(cols)) * NORM_WIDTH;
                float y = yoff + (static_cast<float>(i) / static_cast<float>(rows)) * NORM_HEIGHT;

                as->letter_center[count] = {x, y};

                count++;

                if (count >= 26) {
                    break;
                }
            }
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
            if (event->key.key == SDLK_F) {
                auto flags = SDL_GetWindowFlags(as.window);
                if (flags & SDL_WINDOW_FULLSCREEN) {
                    SDL_SetWindowFullscreen(as.window, false);
                } else { 
                    SDL_SetWindowFullscreen(as.window, true);
                }
            }

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

    for (size_t i = 0; i < 26; i++) {
        if (as.spoken_letter == static_cast<char>('A' + i)) {
            as.font_shader.set_font_width(FONT_WIDTH * 1.2);

            // glowing color effect
            uint64_t now = SDL_GetTicksNS();
            double secs = static_cast<double>(SDL_NS_TO_SECONDS(now));
            double frac = static_cast<double>(now % SDL_NS_PER_SECOND) * 1e-9;
            double t = secs + frac;
            double f = 0.75;
            double lo = 0.5;
            double hi = 1.0;
            double a = (hi - lo) * 0.5;
            double c = (hi + lo) * 0.5;

            float k = static_cast<float>(c + a * std::sin(2 * M_PI * f * t));

            glm::vec4 col = color[i % color.size()] * k; 
            col[3] = 1.f; // alpha
                          
            as.font_shader.set_fg(col);
        } else {
            as.font_shader.set_font_width(FONT_WIDTH);
            as.font_shader.set_fg(Color::transparent);
        }

        std::string str;
        str = 'A' + static_cast<char>(i);

        auto [vertex_uv, index] = as.font.make_text_vertex(str, true);
        as.letter->update_vertex(glm::value_ptr(vertex_uv[0]), sizeof(glm::vec4) * vertex_uv.size());

        BBox b = bbox(vertex_uv);
        glm::vec2 center = (b.start + b.end) * 0.5f * FONT_WIDTH;
        glm::vec2 trans = as.letter_center[i] - center;
        as.font_shader.set_trans(trans);

        draw_vertex_buffer(as.font_shader.shader, as.letter, as.font.tex);
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
