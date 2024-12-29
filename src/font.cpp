#include "font.hpp"

#include <SDL3/SDL_surface.h>

#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <sstream>

#include "gl_helper.hpp"
#include "log.hpp"

namespace {
const char *font_vertex_shader = R"(#version 300 es
precision mediump float;    

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 atlas_tex_coord;

uniform mat4 ortho_matrix;
uniform vec2 trans;
uniform float font_width;
out vec2 texCoord;

void main() {
    gl_Position = ortho_matrix * vec4(pos*font_width + trans, 0.0, 1.0);
    texCoord = atlas_tex_coord;
})";

const char *font_fragment_shader = R"(#version 300 es
precision mediump float;

in vec2 texCoord;
out vec4 color;
uniform sampler2D msdf;
uniform vec4 bg_color;
uniform vec4 fg_color;
uniform vec4 outline_color;
uniform float outline_factor;
uniform float distance_range;
uniform float grid_width;
uniform float display_width;
uniform float font_width;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(msdf, texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    float norm_grid_width = grid_width / display_width;
    float range_scale = font_width / norm_grid_width;

    float screen_px_range = distance_range * range_scale;
    float dist_px = screen_px_range*(sd - 0.5) + 0.5;
    float outline_dist = screen_px_range*outline_factor;

    // There's probably some edge case I haven't tested.
    if (outline_dist > 0.0) {
        if (dist_px > 0.0 && dist_px < 1.0) { 
            // inner and start of outline
            color = mix(outline_color, fg_color, dist_px);
        } else if (dist_px > -outline_dist && dist_px < -outline_dist + 1.0) {
            // end of outline and background
            float opacity = clamp(dist_px + outline_dist, 0.0, 1.0);
            color = mix(bg_color, outline_color, opacity);
        } else if (dist_px > -outline_dist && dist_px < 1.0) {
            color = outline_color;
        } else {
            float opacity = clamp(dist_px, 0.0, 1.0);
            color = mix(bg_color, fg_color, opacity);
        }
    } else {
        float opacity = clamp(dist_px, 0.0, 1.0);
        color = mix(bg_color, fg_color, opacity);
    }
})";
}  // namespace

bool FontAtlas::load(const std::string &atlas_path, const std::string &atlas_txt) {
    tex = make_texture(atlas_path);

    if (!tex) {
        return false;
    }

    // quad for letter
    std::vector<glm::vec4> empty_vert(4);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};

    size_t data_size;
    char *data = static_cast<char *>(SDL_LoadFile(atlas_txt.c_str(), &data_size));

    if (!data) {
        LOG("Failed to open file '%s'.", atlas_txt.c_str());
        return {};
    }

    std::string str(data);
    std::stringstream ss(str);

    std::string label;
    ss >> label;
    assert(label == "distance_range");
    ss >> distance_range;
    ss >> label;
    assert(label == "em_size");
    ss >> em_size;
    ss >> label;
    assert(label == "grid_width");
    ss >> grid_width;
    ss >> label;
    assert(label == "grid_height");
    ss >> grid_height;
    ss >> label;
    assert(label == "unicode");

    while (true) {
        int unicode;
        ss >> unicode;

        if (ss.eof()) {
            break;
        }

        Glyph g;
        ss >> g.advance;
        ss >> g.plane_left;
        ss >> g.plane_bottom;
        ss >> g.plane_right;
        ss >> g.plane_top;
        ss >> g.atlas_left;
        ss >> g.atlas_bottom;
        ss >> g.atlas_right;
        ss >> g.atlas_top;

        glyph[unicode] = g;
    }

    return true;
}

std::pair<glm::vec2, glm::vec2> FontAtlas::get_char_uv(char ch) {
    const Glyph &g = glyph[static_cast<int>(ch)];

    float w = static_cast<float>(tex->width);
    float h = static_cast<float>(tex->height);
    glm::vec2 start{g.atlas_left / w, 1 - g.atlas_bottom / h};
    glm::vec2 end{g.atlas_right / w, 1 - g.atlas_top / h};

    return {start, end};
}

std::vector<glm::vec4> FontAtlas::make_letter(float x, float y, char ch) {
    auto [start, end] = get_char_uv(ch);

    const Glyph &g = glyph[static_cast<int>(ch)];

    float w = (g.atlas_right - g.atlas_left);
    float h = (g.atlas_top - g.atlas_bottom);
    float xoff = g.plane_left * em_size;
    float yoff = std::abs(g.plane_bottom) * em_size;

    x += xoff;
    y += yoff;

    // pos + uv
    return std::vector<glm::vec4>{
        {x, y, start.x, start.y},
        {x + w, y, end.x, start.y},
        {x + w, y - h, end.x, end.y},
        {x, y - h, start.x, end.y},
    };
}

std::pair<std::vector<glm::vec4>, std::vector<uint32_t>> FontAtlas::make_text_vertex(const std::string &str,
                                                                                     bool normalize) {
    float xpos = 0;

    std::vector<glm::vec4> vertex_uv;
    std::vector<uint32_t> index;

    uint32_t vertex_count = 0;

    for (char ch : str) {
        auto v = make_letter(xpos, 0, ch);

        if (normalize) {
            for (auto &v_ : v) {
                v_.x /= static_cast<float>(grid_width);
                v_.y /= static_cast<float>(grid_width);
            }
        }

        vertex_uv.insert(vertex_uv.end(), v.begin(), v.end());

        const Glyph &g = glyph[static_cast<int>(ch)];
        xpos += g.advance * em_size;

        // quad
        std::vector<uint32_t> idx{0, 1, 2, 0, 2, 3};
        for (auto &i : idx) {
            i += vertex_count;
        }
        index.insert(index.end(), idx.begin(), idx.end());

        vertex_count += 4;
    }

    return {vertex_uv, index};
}

std::pair<VertexBufferPtr, BBox> FontAtlas::make_text(const std::string &str, bool normalize) {
    auto [vertex_uv, index] = make_text_vertex(str, normalize);
    return {make_vertex_buffer(vertex_uv, index), bbox(vertex_uv)};
}

bool FontShader::init(const FontAtlas &font_atlas) {
    shader = make_shader(font_vertex_shader, font_fragment_shader);

    if (shader) {
        shader->use();
        glUniform1i(shader->get_loc("msdf"), 0);
        set_font_distance_range(static_cast<float>(font_atlas.distance_range));
        set_font_grid_width(static_cast<float>(font_atlas.grid_width));

        return true;
    }

    return false;
}

void FontShader::set_trans(const glm::vec2 &trans) const {
    assert(shader);
    shader->use();
    glUniform2fv(shader->get_loc("trans"), 1, glm::value_ptr(trans));
}

void FontShader::set_font_grid_width(float grid_width) const {
    assert(shader);
    glUniform1f(shader->get_loc("grid_width"), grid_width);
}

void FontShader::set_font_width(float font_width) const {
    assert(shader);
    glUniform1f(shader->get_loc("font_width"), font_width);
}

void FontShader::set_font_distance_range(float range) const {
    assert(shader);
    glUniform1f(shader->get_loc("distance_range"), range);
}

void FontShader::set_fg(const glm::vec4 &color) const {
    assert(shader);
    shader->use();
    glUniform4fv(shader->get_loc("fg_color"), 1, glm::value_ptr(color));
}

void FontShader::set_bg(const glm::vec4 &color) const {
    assert(shader);
    shader->use();
    glUniform4fv(shader->get_loc("bg_color"), 1, glm::value_ptr(color));
}

void FontShader::set_outline(const glm::vec4 &color) const {
    assert(shader);
    shader->use();
    glUniform4fv(shader->get_loc("outline_color"), 1, glm::value_ptr(color));
}

void FontShader::set_outline_factor(float factor) const {
    assert(shader);
    shader->use();
    glUniform1f(shader->get_loc("outline_factor"), factor);
}

void FontShader::set_ortho(const glm::mat4 &ortho) const {
    assert(shader);
    shader->use();
    glUniformMatrix4fv(shader->get_loc("ortho_matrix"), 1, GL_FALSE, glm::value_ptr(ortho));
}

void FontShader::set_display_width(float display_width) const {
    assert(shader);
    shader->use();
    glUniform1f(shader->get_loc("display_width"), display_width);
}
