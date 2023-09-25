#pragma once

#include <stdint.h>
#include <memory>
#include <set>
#include <gui/widgets/constellation_diagram.h>
#include "dab/database/dab_database_types.h"
#include "utility/lru_cache.h"
#include "dab/mot/MOT_entities.h"
#include "dab/database/dab_database_entities.h"
#include "./texture.h"

class Basic_Slideshow;
class DAB_Decoder;

class DAB_Decoder_ImGui 
{
public:
    struct SelectedSlideshow {
        subchannel_id_t subchannel_id = 0;
        Basic_Slideshow* slideshow = nullptr;
    };
public:
    float constellation_scale = 1.0f;
    float average_constellation_magnitude = 1.0f;
    ImGui::ConstellationDiagram constellation_diagram;
    LRU_Cache<uint32_t, std::unique_ptr<Texture>> slideshow_textures_cache;
    service_id_t focused_service_id = 0;
    DAB_Decoder& decoder;
public:
    DAB_Decoder_ImGui(DAB_Decoder& _decoder);
    Texture* TryGetSlideshowTexture(
        subchannel_id_t subchannel_id, mot_transport_id_t transport_id, 
        const uint8_t* data, const size_t total_bytes);
private:
    uint32_t GetSlideshowKey(subchannel_id_t subchannel_id, mot_transport_id_t transport_id) {
        return (subchannel_id << 16) | transport_id;
    }
};

void RenderDABModule(DAB_Decoder_ImGui& ctx);