#ifdef WIN32
#define GLEW_STATIC
#include <GL/glew.h>
#else
#include <SDL3/SDL_opengles2.h>
#endif

#include "geometry.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gl_helper.hpp"

namespace {
const char *vertex_shader = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec2 pos; // normalized by drawinga area width

uniform vec2 drawing_area_offset; // screen pixel units
uniform float scale; // scale to apply on normalized units
uniform float theta; // rotation in radians
uniform vec2 trans; // normalized units
uniform mat4 ortho_matrix;

void main() {
    float c = cos(theta);
    float s = sin(theta);
    mat2 rotation = mat2(c, s, -s, c);

    gl_Position = ortho_matrix * vec4(rotation*pos*scale + trans, 0.0, 1.0);
})";

const char *fragment_shader = R"(#version 300 es
precision mediump float;

uniform vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
})";

}  // namespace
   // :
std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius) {
    std::vector<glm::vec2> vert;

    for (int i = 0; i < sides; i++) {
        float theta = static_cast<float>(i * 2 * M_PI / sides);
        float r = radius[static_cast<size_t>(i) % radius.size()];
        float x = r * std::cos(theta);
        float y = r * std::sin(theta);

        vert.push_back(glm::vec2{x, y});
    }

    return vert;
}

VertexIndex make_fill(const std::vector<glm::vec2> &vert) {
    std::vector<glm::vec2> fill_vert;
    std::vector<uint32_t> fill_idx;

    fill_vert = vert;
    fill_vert.push_back(glm::vec2{0.0f, 0.0f});  // cener of shape

    for (size_t i = 0; i < vert.size(); i++) {
        uint32_t j = static_cast<uint32_t>((i + 1) % vert.size());

        fill_idx.push_back(static_cast<uint32_t>(i));
        fill_idx.push_back(j);
        fill_idx.push_back(static_cast<uint32_t>(fill_vert.size() - 1));
    }

    return {fill_vert, fill_idx};
}

VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness) {
    // quads for each line
    std::vector<glm::vec2> tri_pts;
    std::vector<uint32_t> tri_idx;
    std::vector<glm::vec2> inner, outer;

    for (size_t i = 0; i < vert.size(); i++) {
        size_t j = (i + 1) % vert.size();

        glm::vec2 v = vert[j] - vert[i];
        v /= glm::length(v);

        // normal to the line
        glm::vec2 n{-v[1], v[0]};

        uint32_t idx = static_cast<uint32_t>(tri_pts.size());

        tri_idx.push_back(idx + 0);
        tri_idx.push_back(idx + 1);
        tri_idx.push_back(idx + 2);
        tri_idx.push_back(idx + 0);
        tri_idx.push_back(idx + 2);
        tri_idx.push_back(idx + 3);

        tri_pts.push_back(vert[i] + n * thickness * 0.5f);
        tri_pts.push_back(vert[j] + n * thickness * 0.5f);
        tri_pts.push_back(vert[j] - n * thickness * 0.5f);
        tri_pts.push_back(vert[i] - n * thickness * 0.5f);

        outer.push_back(vert[i] + n * thickness * 0.5f);
        outer.push_back(vert[j] + n * thickness * 0.5f);
        inner.push_back(vert[j] - n * thickness * 0.5f);
        inner.push_back(vert[i] - n * thickness * 0.5f);
    }

    // miter
    for (size_t i = 0; i < vert.size(); i++) {
        size_t j = (i + 1) % vert.size();

        for (const auto &pts : {inner, outer}) {
            glm::vec2 p0 = pts[i * 2];
            glm::vec2 p1 = pts[i * 2 + 1];
            glm::vec2 p2 = pts[j * 2];
            glm::vec2 p3 = pts[j * 2 + 1];

            glm::vec2 v0 = p1 - p0;
            glm::vec2 v1 = p3 - p2;

            // find intersection of 2 lines
            // Ax = b
            // A = [v0 | -v1]
            // b = p2 - p0
            // use Cramer's rule

            glm::vec2 b = p2 - p0;

            double det = v0[0] * (-v1[1]) - (-v1[0] * v0[1]);
            double c0 = b[0] * (-v1[1]) - (-v1[0] * b[1]);
            float t0 = static_cast<float>(c0 / det);

            glm::vec2 pp = p0 + v0 * t0;

            uint32_t idx = static_cast<uint32_t>(tri_pts.size());

            tri_idx.push_back(idx + 0);
            tri_idx.push_back(idx + 1);
            tri_idx.push_back(idx + 2);
            tri_idx.push_back(idx + 0);
            tri_idx.push_back(idx + 1);
            tri_idx.push_back(idx + 3);

            tri_pts.push_back(vert[j]);
            tri_pts.push_back(pp);
            tri_pts.push_back(p1);
            tri_pts.push_back(p2);
        }
    }

    return {tri_pts, tri_idx};
}

Shape make_shape(const std::vector<glm::vec2> &vert,
                 float line_thickness,
                 const glm::vec4 &line_color,
                 const glm::vec4 &fill_color) {
    Shape shape;
    {
        auto [vertex, index] = make_fill(vert);
        shape.fill.vertex_buffer = make_vertex_buffer(vertex, index);
        shape.fill.color = fill_color;
    }

    {
        auto [vertex, index] = make_line(vert, line_thickness);
        shape.line.vertex_buffer = make_vertex_buffer(vertex, index);
        shape.line.color = line_color;
    }

    {
        auto [vertex, index] = make_line(vert, line_thickness * 2);
        shape.line_highlight.vertex_buffer = make_vertex_buffer(vertex, index);
        shape.line_highlight.color = line_color;
    }

    shape.bbox.start = glm::vec2{-1.f, -1.f};
    shape.bbox.end = glm::vec2{1.f, 1.f};

    return shape;
}

bool ShapeShader::init() {
    shader = make_shader(vertex_shader, fragment_shader);
    if (shader) {
        return true;
    }
    return false;
}

void ShapeShader::set_ortho(const glm::mat4 &ortho) {
    assert(shader);
    shader->use();
    glUniformMatrix4fv(shader->get_loc("ortho_matrix"), 1, GL_FALSE, glm::value_ptr(ortho));
}

glm::vec2 normalize_pos_to_screen_pos(const ShapeShader &shader, const glm::vec2 &pos) {
    return shader.draw_area_offset + pos * shader.draw_area_size.x;
}

glm::vec2 screen_pos_to_normalize_pos(const ShapeShader &shader, const glm::vec2 &pos) {
    return (pos - shader.draw_area_offset) / shader.draw_area_size.x;
}

void draw_shape(const ShapeShader &shape_shader, const Shape &shape, bool fill, bool line, bool line_highlight) {
    const ShaderPtr &s = shape_shader.shader;

    s->use();

    glUniform1f(s->get_loc("scale"), shape.scale);
    glUniform1f(s->get_loc("theta"), shape.theta);
    glUniform2fv(s->get_loc("trans"), 1, glm::value_ptr(shape.trans));

    if (fill) {
        glUniform4fv(s->get_loc("color"), 1, glm::value_ptr(shape.fill.color));
        draw_vertex_buffer(s, shape.fill.vertex_buffer);
    }

    if (line) {
        glUniform4fv(s->get_loc("color"), 1, glm::value_ptr(shape.line.color));
        draw_vertex_buffer(s, shape.line.vertex_buffer);
    }

    if (line_highlight) {
        glUniform4fv(s->get_loc("color"), 1, glm::value_ptr(shape.line_highlight.color));
        draw_vertex_buffer(s, shape.line_highlight.vertex_buffer);
    }
}
