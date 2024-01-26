#pragma once

#include <stdint.h>
#include <memory>
#include <gui/widgets/constellation_diagram.h>
#include "dab/database/dab_database_types.h"
#include "dab/mot/MOT_entities.h"
#include "utility/lru_cache.h"
#include "utility/span.h"
#include "./texture.h"

class Radio_Block;

class Radio_View_Controller 
{
public:
    float constellation_scale = 1.0f;
    float average_constellation_magnitude = 1.0f;
    ImGui::ConstellationDiagram constellation_diagram;
    service_id_t focused_service_id = 0;
private:
    LRU_Cache<uint32_t, std::unique_ptr<Texture>> slideshow_textures_cache;
public:
    Radio_View_Controller() {
        slideshow_textures_cache.set_max_size(100);
    }
    Texture* TryGetSlideshowTexture(
        subchannel_id_t subchannel_id, mot_transport_id_t transport_id, 
        tcb::span<const uint8_t> data
    );
};

void Render_Radio_Block(Radio_Block& block, Radio_View_Controller& ctx);