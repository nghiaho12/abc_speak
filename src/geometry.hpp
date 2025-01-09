// #include <SDL3/SDL_opengles2.h>

#include <glm/glm.hpp>
#include <vector>

#include "gl_helper.hpp"

// Wrapper for GL_TRIANGLES
struct ShapePrimitive {
    VertexBufferPtr vertex_buffer{{}, {}};
    glm::vec4 color{};
};

struct Shape {
    BBox bbox;

    float rotation_direction = 1.f;

    ShapePrimitive line;
    ShapePrimitive line_highlight;
    ShapePrimitive fill;

    glm::vec2 trans{};
    float scale = 1.0f;
    float theta = 0.0f;  // rotation in radians
};

struct ShapeShader {
    ShaderPtr shader{{}, {}};
    glm::vec2 draw_area_offset;
    glm::vec2 draw_area_size;

    bool init();
    void set_ortho(const glm::mat4 &ortho);
};

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};

void draw_shape(const ShapeShader &shape_shader, const Shape &shape, bool fill, bool line, bool line_highlight);

glm::vec2 normalize_pos_to_screen_pos(const ShapeShader &shader, const glm::vec2 &pos);
glm::vec2 screen_pos_to_normalize_pos(const ShapeShader &shader, const glm::vec2 &pos);

VertexIndex make_fill(const std::vector<glm::vec2> &vert);
VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness);

Shape make_shape(const std::vector<glm::vec2> &vert,
                 float line_thickness,
                 const glm::vec4 &line_color,
                 const glm::vec4 &fill_color);
