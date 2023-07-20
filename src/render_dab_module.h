#pragma once

#include <gui/widgets/constellation_diagram.h>
struct DAB_Decoder_ImGui {
    float constellation_scale = 1.0f;
    float average_constellation_magnitude = 1.0f;
    ImGui::ConstellationDiagram constellation_diagram;
};

class DAB_Decoder;
void RenderDABModule(DAB_Decoder_ImGui& decoder_imgui, DAB_Decoder& decoder);