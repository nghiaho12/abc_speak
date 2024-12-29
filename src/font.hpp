#pragma once

#include <SDL3/SDL_opengles2.h>

#include <glm/glm.hpp>
#include <map>
#include <utility>

#include "gl_helper.hpp"

// How to render the Glyph
// Plane is offset relative to cursor pos
// Atlas is bounding box in the texture atlas
struct Glyph {
    float advance;     // em, x increment
    float plane_left;  // em
    float plane_bottom;
    float plane_right;
    float plane_top;
    float atlas_left;  // pixel
    float atlas_bottom;
    float atlas_right;
    float atlas_top;
};

struct FontAtlas {
    TexturePtr tex{{}, {}};

    int distance_range;  // signed distance field range in pixels
    float em_size;       // pixels per em unit
    int grid_width;
    int grid_height;
    std::map<int, Glyph> glyph;

    bool load(const std::string &atlas_path, const std::string &atlas_txt);
    std::pair<VertexBufferPtr, BBox> make_text(const std::string &str, bool normalize);
    std::pair<std::vector<glm::vec4>, std::vector<uint32_t>> make_text_vertex(const std::string &str, bool normalize);

    std::pair<glm::vec2, glm::vec2> get_char_uv(char ch);
    std::vector<glm::vec4> make_letter(float x, float y, char ch);
};

struct FontShader {
    ShaderPtr shader{{}, {}};

    bool init(const FontAtlas &font_atlas);

    // call when window resizes
    void set_ortho(const glm::mat4 &ortho) const;
    void set_display_width(float display_width) const;

    void set_font_distance_range(float range) const;
    void set_font_grid_width(float range) const;
    void set_font_width(float font_width) const;

    void set_trans(const glm::vec2 &trans) const;
    void set_fg(const glm::vec4 &color) const;
    void set_bg(const glm::vec4 &color) const;
    void set_outline(const glm::vec4 &color) const;
    void set_outline_factor(float factor) const;
};
