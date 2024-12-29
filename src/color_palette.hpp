#pragma once

#include <glm/glm.hpp>

// color is based on Tableau 10
struct Color {
    static constexpr glm::vec4 blue{0.3412f, 0.4706f, 0.6431f, 1.f};
    static constexpr glm::vec4 orange{0.8941f, 0.5804f, 0.2667f, 1.f};
    static constexpr glm::vec4 red{0.8196f, 0.3804f, 0.3647f, 1.f};
    static constexpr glm::vec4 teal{0.5216f, 0.7137f, 0.6980f, 1.f};
    static constexpr glm::vec4 green{0.4157f, 0.6235f, 0.3451f, 1.f};
    static constexpr glm::vec4 yellow{0.9059f, 0.7922f, 0.3765f, 1.f};
    static constexpr glm::vec4 purple{0.6588f, 0.4863f, 0.6235f, 1.f};
    static constexpr glm::vec4 pink{0.9451f, 0.6353f, 0.6627f, 1.f};
    static constexpr glm::vec4 brown{0.5882f, 0.4627f, 0.3843f, 1.f};
    static constexpr glm::vec4 grey{0.7216f, 0.6902f, 0.6745f, 1.f};
    static constexpr glm::vec4 white{1.0000f, 1.0000f, 1.0000f, 1.f};
    static constexpr glm::vec4 darkgrey{0.3f, 0.3f, 0.3f, 1.f};
    static constexpr glm::vec4 black{0.f, 0.f, 0.f, 1.f};
    static constexpr glm::vec4 transparent{0.f};
};
