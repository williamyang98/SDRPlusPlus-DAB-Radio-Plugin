#include "./render_radio_block.h"

#include <cmath>
#include <string_view>
#include <fmt/core.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <gui/widgets/constellation_diagram.h>
#include <gui/tuner.h>

// NOTE: gui/gui.h for some reason has defines for min/max
#include <gui/gui.h>
#undef min
#undef max

#include "./radio_block.h"
#include "./render_formatters.h"
#include "./texture.h"
#include "ofdm/ofdm_demodulator.h"
#include "basic_radio/basic_radio.h"
#include "basic_radio/basic_audio_channel.h"
#include "basic_radio/basic_data_packet_channel.h"
#include "basic_radio/basic_dab_plus_channel.h"
#include "basic_radio/basic_dab_channel.h"
#include "basic_radio/basic_slideshow.h"
#include "dab/database/dab_database.h"
#include "dab/database/dab_database_updater.h"
#include "dab/dab_misc_info.h"
#include "audio/audio_pipeline.h"

template <typename T, typename F>
static T* find_by_callback(std::vector<T>& vec, F&& func) {
    for (auto& e: vec) {
        if (func(e)) return &e;
    }
    return nullptr;
}

// We need to manually clamp values since ImGui_AlwaysClamp doesn't work
static void ClampValue(int& value, int min, int max) {
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }
}

static std::string convert_frequency_to_string(freq_t freq) {
    if (freq >= 1'000'000'000) {
        return fmt::format("{:3.3} GHz", static_cast<float>(freq)*1e-9f);
    } else if (freq >= 1'000'000) {
        return fmt::format("{:3.3} MHz", static_cast<float>(freq)*1e-6f);
    } else if (freq >= 1'000) {
        return fmt::format("{:3.3} kHz", static_cast<float>(freq)*1e-3f);
    } else {
        return fmt::format("{:3} Hz", static_cast<float>(freq));
    }
}

// ofdm demodulator
constexpr float OFDM_DEMOD_SAMPLING_RATE = 2.048e6;
static void RenderOFDMState(OFDM_Demod& demod);
static void RenderOFDMControls(OFDM_Demod& demod);
static void RenderOFDMConstellation(Radio_View_Controller& ctx, tcb::span<const std::complex<float>> data);
// basic radio
static void RenderRadioServices(BasicRadio& radio, Radio_View_Controller& ctx);
static void RenderRadioService(BasicRadio& radio, Radio_View_Controller& ctx);
static void RenderRadioStatistics(BasicRadio& radio);
static void RenderRadioEnsemble(BasicRadio& radio);
static void RenderRadioDateTime(BasicRadio& radio);
// audio mixer
static void RenderAudioControls(AudioPipeline& audio);

Texture* Radio_View_Controller::TryGetSlideshowTexture(
    subchannel_id_t subchannel_id, mot_transport_id_t transport_id, 
    tcb::span<const uint8_t> data)
{
    const uint32_t key = (subchannel_id << 16) | transport_id;
    auto* res = slideshow_textures_cache.find(key);
    if (res == nullptr) {
        auto& v = slideshow_textures_cache.emplace(key, Texture::LoadFromMemory(data.data(), data.size()));
        return v.get();
    }
    return res->get();
}

void Render_Radio_Block(Radio_Block& block, Radio_View_Controller& ctx) {
    auto demod = block.get_ofdm_demodulator();
    auto radio = block.get_basic_radio();
    auto audio_pipeline = block.get_audio_pipeline();

    if (ImGui::BeginTabBar("Tab bar")) {
        if (demod && ImGui::BeginTabItem("OFDM")) {
            if (ImGui::Button("Reset")) {
                demod->Reset();
            }

            if (ImGui::BeginTabBar("OFDM tab bar")) {
                if (ImGui::BeginTabItem("State")) {
                    RenderOFDMState(*demod);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Controls")) {
                    RenderOFDMControls(*demod);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Constellation")) {
                    const auto& constellation = demod->GetFrameDataVec();
                    RenderOFDMConstellation(ctx, constellation);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (radio && ImGui::BeginTabItem("DAB")) {
            if (ImGui::Button("Reset")) {
                block.reset_radio();
                ctx.focused_service_id = 0;
            }

            auto lock = std::scoped_lock(radio->GetMutex());
            if (ImGui::BeginTabBar("DAB tab bar")) {
                if (ImGui::BeginTabItem("Channels")) {
                    RenderRadioServices(*radio, ctx);
                    ImGui::Separator();
                    RenderRadioService(*radio, ctx);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Ensemble")) {
                    RenderRadioEnsemble(*radio);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Date&Time")) {
                    RenderRadioDateTime(*radio);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Statistics")) {
                    RenderRadioStatistics(*radio);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Global")) {
                    ImGui::Text("Global Controls"); 
                    static bool is_play_audio = false;
                    static bool is_decode_data = true;
                    if (ImGui::RadioButton("Play Audio", is_play_audio)) {
                        is_play_audio = !is_play_audio;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Decode Data", is_decode_data)) {
                        is_decode_data = !is_decode_data;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Settings")) {
                        auto& db = radio->GetDatabase();
                        for (auto& subchannel: db.subchannels) {
                            auto* channel = radio->Get_Audio_Channel(subchannel.id);
                            if (channel != nullptr) {
                                auto& controls = channel->GetControls();
                                if (is_play_audio) {
                                    controls.SetIsPlayAudio(true);
                                } else {
                                    controls.SetIsDecodeAudio(false);
                                }
                                controls.SetIsDecodeData(is_decode_data);
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
            RenderAudioControls(*audio_pipeline);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void RenderOFDMState(OFDM_Demod& demod) {
    #define ENUM_TO_STRING(NAME) \
    case OFDM_Demod::State::NAME: ImGui::Text("State: "#NAME); break;

    switch (demod.GetState()) {
    ENUM_TO_STRING(FINDING_NULL_POWER_DIP);
    ENUM_TO_STRING(READING_NULL_AND_PRS);
    ENUM_TO_STRING(RUNNING_COARSE_FREQ_SYNC);
    ENUM_TO_STRING(RUNNING_FINE_TIME_SYNC);
    ENUM_TO_STRING(READING_SYMBOLS);
    default: 
    ImGui::Text("State: Unknown"); 
        break;
    }
    ImGui::Text("Fine freq: %.2f Hz", demod.GetFineFrequencyOffset() * OFDM_DEMOD_SAMPLING_RATE);
    ImGui::Text("Coarse freq: %.2f Hz", demod.GetCoarseFrequencyOffset() * OFDM_DEMOD_SAMPLING_RATE);
    ImGui::Text("Net freq: %.2f Hz", demod.GetNetFrequencyOffset() * OFDM_DEMOD_SAMPLING_RATE);
    ImGui::Text("Signal level: %.2f", demod.GetSignalAverage());
    ImGui::Text("Frames read: %d", demod.GetTotalFramesRead());
    ImGui::Text("Frames desynced: %d", demod.GetTotalFramesDesync());

    #undef ENUM_TO_STRING
}

void RenderOFDMControls(OFDM_Demod& demod) {
    auto& cfg = demod.GetConfig();
    auto params = demod.GetOFDMParams();
    ImGui::Checkbox("Coarse frequency correction", &cfg.sync.is_coarse_freq_correction);
    ImGui::SliderFloat("Fine frequency beta", &cfg.sync.fine_freq_update_beta, 0.0f, 1.0f, "%.2f");
    {
        float frequency_offset_Hz = cfg.sync.max_coarse_freq_correction_norm * OFDM_DEMOD_SAMPLING_RATE;
        if (ImGui::SliderFloat("Max coarse frequency (Hz)", &frequency_offset_Hz, 0, OFDM_DEMOD_SAMPLING_RATE/2.0f)) {
            cfg.sync.max_coarse_freq_correction_norm = frequency_offset_Hz / OFDM_DEMOD_SAMPLING_RATE;
        }
    }
    ImGui::SliderFloat("Coarse freq slow beta", &cfg.sync.coarse_freq_slow_beta, 0.0f, 1.0f);
    ImGui::SliderFloat("Impulse peak threshold (dB)", &cfg.sync.impulse_peak_threshold_db, 0, 100.0f, "%.f");
    ImGui::SliderFloat("Impulse peak distance weight", &cfg.sync.impulse_peak_distance_probability, 0.0f, 1.0f, "%.3f");
    static float null_threshold[2] = {0,0};
    null_threshold[0] = cfg.null_l1_search.thresh_null_start;
    null_threshold[1] = cfg.null_l1_search.thresh_null_end;
    if (ImGui::SliderFloat2("Null detection threshold", null_threshold, 0.0f, 1.0f, "%.2f")) {
        null_threshold[0] = std::min(null_threshold[0], null_threshold[1]);
        null_threshold[1] = std::max(null_threshold[0], null_threshold[1]);
        cfg.null_l1_search.thresh_null_start = null_threshold[0];
        cfg.null_l1_search.thresh_null_end = null_threshold[1];
    }
    ImGui::SliderFloat("L1 signal update beta", &cfg.signal_l1.update_beta, 0.0f, 1.0f, "%.2f");
}

void RenderRadioServices(BasicRadio& radio, Radio_View_Controller& ctx) {
    auto& db = radio.GetDatabase(); 
    // Render channel list selector
    auto* focused_service = find_by_callback(db.services, [&ctx](const auto& service) {
        return service.reference == ctx.focused_service_id;
    });
    const char* const SERVICE_LABEL_PLACEHOLDER = "[Unknown]";
    const char* focused_service_label = "None selected";
    if (focused_service != nullptr) {
        if (focused_service->label.empty()) {
            focused_service_label = SERVICE_LABEL_PLACEHOLDER;
        } else {
            focused_service_label = focused_service->label.c_str();
        }
    }
    const auto total_services = db.services.size();

    const auto dropdown_label = fmt::format("Services ({})###services_dropdown", total_services);
    if (ImGui::BeginCombo(dropdown_label.c_str(), focused_service_label)) {
        static std::vector<Service*> service_list;
        service_list.clear();
        for (auto& service: db.services) {
            service_list.push_back(&service);
        }
        std::sort(service_list.begin(), service_list.end(), [](const auto* a, const auto* b) {
            return (a->label.compare(b->label) < 0);
        });

        for (auto* service: service_list) {
            ImGui::PushID(service->reference);
            bool is_play_audio = false;
            bool is_decode_data = false;
            for (auto& component: db.service_components) {
                if (component.service_reference != service->reference) continue;
                auto* channel = radio.Get_Audio_Channel(component.subchannel_id);
                if (channel != nullptr) {
                    auto& controls = channel->GetControls();
                    is_play_audio |= controls.GetIsPlayAudio();
                    is_decode_data |= controls.GetIsDecodeData();
                }
            }

            auto label = fmt::format("{}###{}", service->label.empty() ? SERVICE_LABEL_PLACEHOLDER : service->label, service->reference);
            const std::string status_label = fmt::format("{}/{}",
                is_play_audio ? "A" : "-",
                is_decode_data ? "D" : "-"
            );

            const bool is_focused = service->reference == ctx.focused_service_id;
            if (ImGui::Selectable(label.c_str(), is_focused)) {
                ctx.focused_service_id = service->reference;
            }
 
            const float offset = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(status_label.c_str()).x;
            ImGui::SameLine(offset);
            ImGui::Text("%.*s", int(status_label.length()), status_label.c_str());

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
}

static void RenderSlideshowManager(Basic_Slideshow_Manager& slideshow_manager, Radio_View_Controller& ctx, subchannel_id_t subchannel_id) {
    static std::vector<std::shared_ptr<Basic_Slideshow>> slideshows;
    {
        auto lock = std::unique_lock(slideshow_manager.GetSlideshowsMutex());
        slideshows.clear();
        for (auto& slideshow: slideshow_manager.GetSlideshows()) {
            slideshows.push_back(slideshow);
        }
    }

    const int total_slideshows = int(slideshows.size());
    if (total_slideshows > 0) {
        static int slideshow_index = 0;
        if (total_slideshows > 1) {
            ImGui::SliderInt("###Image", &slideshow_index, 0, total_slideshows-1, "%d", ImGuiSliderFlags_AlwaysClamp);
        }
        ClampValue(slideshow_index, 0, total_slideshows-1);

        auto slideshow = slideshows[slideshow_index];
        const auto* texture = ctx.TryGetSlideshowTexture(subchannel_id, slideshow->transport_id, slideshow->image_data);
        if (texture != nullptr) {
            const ImVec2 region_min = ImGui::GetWindowContentRegionMin();
            const ImVec2 region_max = ImGui::GetWindowContentRegionMax();
            const float region_width = float(region_max.x - region_min.x);
            const float scale = region_width / static_cast<float>(texture->GetWidth());
            const auto texture_size = ImVec2(
                    static_cast<float>(texture->GetWidth()) * scale, 
                    static_cast<float>(texture->GetHeight()) * scale
                    );
            const auto texture_id = reinterpret_cast<ImTextureID>(texture->GetTextureID());
            ImGui::Image(texture_id, texture_size);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%.*s", int(slideshow->name.length()), slideshow->name.c_str());
            }
        } else {
            ImGui::Text("Failed to load slideshow image");
        }

        #define FIELD_MACRO(name, fmt, ...) {\
            ImGui::PushID(row_id++);\
            ImGui::TableNextRow();\
            ImGui::TableSetColumnIndex(0);\
            ImGui::TextWrapped(name);\
            ImGui::TableSetColumnIndex(1);\
            ImGui::TextWrapped(fmt, __VA_ARGS__);\
            ImGui::PopID();\
        }\

        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("Status", 2, flags)) {
            int row_id  = 0;
            FIELD_MACRO("Subchannel ID", "%u", subchannel_id);
            FIELD_MACRO("Transport ID", "%u", slideshow->transport_id);
            FIELD_MACRO("Name", "%.*s", int(slideshow->name.length()), slideshow->name.c_str());
            FIELD_MACRO("Trigger Time", "%" PRIi64, int64_t(slideshow->trigger_time));
            FIELD_MACRO("Expire Time", "%" PRIi64, int64_t(slideshow->expire_time));
            FIELD_MACRO("Category ID", "%u", slideshow->category_id);
            FIELD_MACRO("Slide ID", "%u", slideshow->slide_id);
            FIELD_MACRO("Category title", "%.*s", int(slideshow->category_title.length()), slideshow->category_title.c_str());
            FIELD_MACRO("Click Through URL", "%.*s", int(slideshow->click_through_url.length()), slideshow->click_through_url.c_str());
            FIELD_MACRO("Alt Location URL", "%.*s", int(slideshow->alt_location_url.length()), slideshow->alt_location_url.c_str());
            FIELD_MACRO("Size", "%zu Bytes", slideshow->image_data.size());

            if (texture != NULL) {
                FIELD_MACRO("Resolution", "%u x %u", texture->GetWidth(), texture->GetHeight());
                FIELD_MACRO("Internal Texture ID", "%" PRIu32, texture->GetTextureID());
            }
            ImGui::EndTable();
        }

        #undef FIELD_MACRO

    } else {
        ImGui::Text("No slideshows available yet");
    }
}

static void RenderAudioChannelControls(Basic_Audio_Channel& channel) {
    auto& controls = channel.GetControls();
    const bool is_play_audio = controls.GetIsPlayAudio();
    const bool is_decode_data = controls.GetIsDecodeData();
    const bool is_all_enabled = is_play_audio && is_decode_data;
    if (is_all_enabled) {
        if (ImGui::Button("Stop All")) controls.StopAll();
    } else {
        if (ImGui::Button("Run All")) controls.RunAll();
    }
    ImGui::SameLine();
    if (is_play_audio) {
        if (ImGui::Button("Mute Audio")) controls.SetIsDecodeAudio(false);
    } else {
        if (ImGui::Button("Play Audio")) controls.SetIsPlayAudio(true);
    }
    ImGui::SameLine();
    if (is_decode_data) {
        if (ImGui::Button("Stop Data Decode")) controls.SetIsDecodeData(false);
    } else {
        if (ImGui::Button("Start Data Decode")) controls.SetIsDecodeData(true);
    }
}

static void RenderDABPlusChannelStatus(Basic_DAB_Plus_Channel& channel, Subchannel& subchannel) {
    std::string codec_description = "DAB+ (no codec info)";
    const auto prot_label = GetSubchannelProtectionLabel(subchannel);
    const uint32_t bitrate_kbps = GetSubchannelBitrate(subchannel);
    const auto& superframe_header = channel.GetSuperFrameHeader();
    const bool is_codec_found = superframe_header.sampling_rate != 0;
    const bool is_stereo = superframe_header.is_stereo || superframe_header.is_parametric_stereo;
    if (is_codec_found) {
        codec_description = fmt::format("DAB+ {}Hz {} {} {}", 
            superframe_header.sampling_rate,
            is_stereo ? "Stereo" : "Mono",
            GetAACDescriptionString(superframe_header.is_spectral_band_replication, superframe_header.is_parametric_stereo),
            GetMPEGSurroundString(superframe_header.mpeg_surround)
        );
    }
    const auto& dynamic_label = channel.GetDynamicLabel();
    ImGui::TextWrapped("%.*s", int(codec_description.length()), codec_description.c_str());
    ImGui::TextWrapped("%.*s", int(dynamic_label.length()), dynamic_label.data());

    ImGui::Separator();

    ImGui::RadioButton("Firecode", !channel.IsFirecodeError());
    ImGui::SameLine();
    ImGui::RadioButton("Reed Solomon", !channel.IsRSError());
    ImGui::SameLine();
    ImGui::RadioButton("Access Unit", !channel.IsAUError());
    ImGui::SameLine();
    ImGui::RadioButton("Codec", !channel.IsCodecError());
}

static void RenderDABChannelStatus(Basic_DAB_Channel& channel, Subchannel& subchannel) {
    std::string codec_description = "DAB (no codec info)";
    const auto prot_label = GetSubchannelProtectionLabel(subchannel);
    const uint32_t bitrate_kbps = GetSubchannelBitrate(subchannel);
    const auto& audio_params_opt = channel.GetAudioParams();
    if (audio_params_opt.has_value()) {
        const auto& audio_params = audio_params_opt.value();
        codec_description = fmt::format("DAB MP2 {}Hz {} {}kb/s", 
            audio_params.sample_rate, 
            audio_params.is_stereo ? "Stereo" : "Mono",  
            audio_params.bitrate_kbps
        );
    }
    const auto& dynamic_label = channel.GetDynamicLabel();
    ImGui::TextWrapped("%.*s", int(codec_description.length()), codec_description.c_str());
    ImGui::TextWrapped("%.*s", int(dynamic_label.length()), dynamic_label.data());

    ImGui::Separator();

    ImGui::RadioButton("MP2 Decoder", !channel.GetIsError());
}

static void RenderAudioChannelStatus(Basic_Audio_Channel& channel, Subchannel& subchannel) {
    const auto type = channel.GetType();
    if (type == AudioServiceType::DAB_PLUS) {
        auto& dab_plus_channel = dynamic_cast<Basic_DAB_Plus_Channel&>(channel);
        RenderDABPlusChannelStatus(dab_plus_channel, subchannel);
    } else if (type == AudioServiceType::DAB) {
        auto& dab_channel = dynamic_cast<Basic_DAB_Channel&>(channel);
        RenderDABChannelStatus(dab_channel, subchannel);
    }
}

void RenderRadioService(BasicRadio& radio, Radio_View_Controller& ctx) {
    auto& db = radio.GetDatabase();

    auto* service = find_by_callback(db.services, [&ctx](const auto& service) {
        return service.reference == ctx.focused_service_id;
    });
    if (service == nullptr) {
        ImGui::Text("Please select a service");
        return;
    }

    static std::vector<ServiceComponent*> service_components;
    service_components.clear();
    for (auto& service_component: db.service_components) {
        if (service_component.service_reference != service->reference) continue;
        service_components.push_back(&service_component);
    }

    if (service_components.size() == 0) {
        ImGui::Text("Service does not have any service components");
        return;
    }

    static int selected_index = 0;
    const int total_components = int(service_components.size());
    if (service_components.size() > 1) {
        ImGui::SliderInt("Component", &selected_index, 0, total_components-1, "%d", ImGuiSliderFlags_AlwaysClamp);
    }
    ClampValue(selected_index, 0, total_components-1);
    auto* service_component = service_components[selected_index];

    const auto subchannel_id = service_component->subchannel_id;
    auto* subchannel = find_by_callback(db.subchannels, [subchannel_id](const auto& subchannel) {
        return subchannel.id == subchannel_id;
    });
    if (subchannel == nullptr) {
        ImGui::Text("Subchannel is not available yet");
        return;
    }

    auto* audio_channel = radio.Get_Audio_Channel(subchannel->id);
    auto* data_packet_channel = radio.Get_Data_Packet_Channel(subchannel->id);
    if (audio_channel != nullptr) {
        RenderAudioChannelControls(*audio_channel);
        ImGui::Separator();
        RenderAudioChannelStatus(*audio_channel, *subchannel);
    } else if (data_packet_channel != nullptr) {
        ImGui::Text("Data Packet Channel");
    }
    ImGui::Separator();

    // Helper macro to layout table rows
    #define FIELD_MACRO(name, fmt, ...) {\
        ImGui::PushID(row_id++);\
        ImGui::TableNextRow();\
        ImGui::TableSetColumnIndex(0);\
        ImGui::TextWrapped(name);\
        ImGui::TableSetColumnIndex(1);\
        ImGui::TextWrapped(fmt, __VA_ARGS__);\
        ImGui::PopID();\
    }\

    {
        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
        auto& ensemble = db.ensemble;
        if (ImGui::BeginTable("Service Description", 2, flags)) {
            int row_id = 0;
            FIELD_MACRO("Programme Type", "%s (%u)", 
                GetProgrammeTypeString(ensemble.international_table_id, service->programme_type),
                service->programme_type);
            FIELD_MACRO("Language", "%s (%u)", GetLanguageTypeString(service->language), service->language);
            FIELD_MACRO("Closed Caption", "%u", service->closed_caption);
            FIELD_MACRO("Country Code", "0x%02X.%01X", service->extended_country_code, service->country_id);
            FIELD_MACRO("Country Name", "%s", GetCountryString(
                service->extended_country_code ? service->extended_country_code : ensemble.extended_country_code, 
                service->country_id ? service->country_id : ensemble.country_id));

            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("channel_tabs")) {
        if (ImGui::BeginTabItem("Slideshow")) {
            if (audio_channel != nullptr) {
                RenderSlideshowManager(audio_channel->GetSlideshowManager(), ctx, subchannel->id);
            } else if (data_packet_channel != nullptr) {
                RenderSlideshowManager(data_packet_channel->GetSlideshowManager(), ctx, subchannel->id);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Linked Services")) {
            static std::vector<const LinkService*> link_services;
            link_services.clear();
            for (const auto& link_service: db.link_services) {
                if (link_service.service_reference != service->reference) continue;
                link_services.push_back(&link_service);
            }
            const int total_linked_services = int(link_services.size());
            if (total_linked_services > 0) {
                static int selected_link_index = 0;
                if (total_linked_services > 1) {
                    ImGui::SliderInt("Linked Service", &selected_link_index, 0, total_linked_services-1, "%d", ImGuiSliderFlags_AlwaysClamp);
                }                     
                ClampValue(selected_link_index, 0, total_linked_services-1);
                auto* link_service = link_services[selected_link_index]; 
                if (link_service != nullptr) {
                    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
                    // Description header
                    ImGui::Text("Link Service Description");
                    if (ImGui::BeginTable("LSN Description", 2, flags)) {
                        int row_id = 0;
                        FIELD_MACRO("LSN", "%u", link_service->id);
                        FIELD_MACRO("Active", "%s", link_service->is_active_link ? "Yes" : "No");
                        FIELD_MACRO("Hard Link", "%s", link_service->is_hard_link ? "Yes": "No");
                        FIELD_MACRO("International", "%s", link_service->is_international ? "Yes" : "No");
                        ImGui::EndTable();
                    }

                    static std::vector<FM_Service*> fm_services;
                    fm_services.clear();
                    for (auto& fm_service: db.fm_services) {
                        if (fm_service.linkage_set_number != link_service->id) continue;
                        fm_services.push_back(&fm_service);
                    }
                    if (fm_services.size() > 0) {
                        ImGui::Separator();
                        ImGui::Text("FM Services (%zu)", fm_services.size());
                        if (ImGui::BeginTable("FM Table", 3, flags)) {
                            ImGui::TableSetupColumn("Callsign",         ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int row_id  = 0;
                            for (auto* fm_service: fm_services) {
                                ImGui::PushID(row_id++);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextWrapped("%04X", fm_service->RDS_PI_code);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextWrapped("%s", fm_service->is_time_compensated ? "Yes" : "No");
                                ImGui::TableSetColumnIndex(2);
                                auto& frequencies = fm_service->frequencies;
                                for (auto& freq: frequencies) {
                                    std::string label = convert_frequency_to_string(freq);
                                    if (ImGui::Selectable(label.c_str(), false)) {
                                        tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, static_cast<double>(freq));
                                    }
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                    } else {
                        ImGui::Text("No linked FM services");
                    }

                    static std::vector<DRM_Service*> drm_services;
                    drm_services.clear();
                    for (auto& drm_service: db.drm_services) {
                        if (drm_service.linkage_set_number != link_service->id) continue;
                        drm_services.push_back(&drm_service);
                    }
                    if (drm_services.size() > 0) {
                        ImGui::Separator();
                        ImGui::Text("DRM Services (%zu)", drm_services.size());
                        if (ImGui::BeginTable("DRM Table", 3, flags)) {
                            ImGui::TableSetupColumn("ID",               ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int row_id  = 0;
                            for (auto* drm_service: drm_services) {
                                ImGui::PushID(row_id++);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextWrapped("%u", drm_service->drm_code);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextWrapped("%s", drm_service->is_time_compensated ? "Yes" : "No");
                                ImGui::TableSetColumnIndex(2);
                                auto& frequencies = drm_service->frequencies;
                                for (auto& freq: frequencies) {
                                    std::string label = convert_frequency_to_string(freq);
                                    if (ImGui::Selectable(label.c_str(), false)) {
                                        tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, static_cast<double>(freq));
                                    }
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                    } else {
                        ImGui::Text("No linked DRM services");
                    }
                }
            } else {
                ImGui::Text("No linked services");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Subchannel")) {
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
            if (ImGui::BeginTable("Details", 2, flags)) {
                int row_id  = 0;
                const auto prot_label = GetSubchannelProtectionLabel(*subchannel);
                const uint32_t bitrate_kbps = GetSubchannelBitrate(*subchannel);
                FIELD_MACRO("Subchannel ID", "%u", subchannel->id);
                FIELD_MACRO("Start Address", "%u", subchannel->start_address);
                FIELD_MACRO("Capacity Units", "%u", subchannel->length);
                FIELD_MACRO("Protection", "%.*s", int(prot_label.length()), prot_label.c_str());
                FIELD_MACRO("Bitrate", "%u kb/s", bitrate_kbps);

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Component")) {
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
            if (ImGui::BeginTable("Service Component", 2, flags)) {
                int row_id  = 0;
                const bool is_audio_type = (service_component->transport_mode == TransportMode::STREAM_MODE_AUDIO);
                const char* type_str = is_audio_type ? 
                    GetAudioTypeString(service_component->audio_service_type) :
                    GetDataTypeString(service_component->data_service_type);
                
                FIELD_MACRO("Label", "%.*s", int(service_component->label.length()), service_component->label.c_str());
                FIELD_MACRO("Component ID", "%u", service_component->component_id);
                FIELD_MACRO("Global ID", "%u", service_component->global_id);
                FIELD_MACRO("Transport Mode", "%s", GetTransportModeString(service_component->transport_mode));
                FIELD_MACRO("Type", "%s", type_str);

                ImGui::EndTable();
            } 
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    #undef FIELD_MACRO
}

void RenderRadioStatistics(BasicRadio& radio) {
    const auto& stats = radio.GetDatabaseStatistics();
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("Date & Time", 2, flags)) {
        #define FIELD_MACRO(name, fmt, ...) {\
            ImGui::PushID(row_id++);\
            ImGui::TableNextRow();\
            ImGui::TableSetColumnIndex(0);\
            ImGui::TextWrapped(name);\
            ImGui::TableSetColumnIndex(1);\
            ImGui::TextWrapped(fmt, __VA_ARGS__);\
            ImGui::PopID();\
        }\

        int row_id = 0;
        FIELD_MACRO("Total", "%zu", stats.nb_total);
        FIELD_MACRO("Pending", "%zu", stats.nb_pending);
        FIELD_MACRO("Completed", "%zu", stats.nb_completed);
        FIELD_MACRO("Conflicts", "%zu", stats.nb_conflicts);
        FIELD_MACRO("Updates", "%zu", stats.nb_updates);

        #undef FIELD_MACRO
        ImGui::EndTable();
    }
}

void RenderRadioEnsemble(BasicRadio& radio) {
    auto& db = radio.GetDatabase();
    auto& ensemble = db.ensemble;

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("Ensemble description", 2, flags)) {
        #define FIELD_MACRO(name, fmt, ...) {\
            ImGui::PushID(row_id++);\
            ImGui::TableNextRow();\
            ImGui::TableSetColumnIndex(0);\
            ImGui::TextWrapped(name);\
            ImGui::TableSetColumnIndex(1);\
            ImGui::TextWrapped(fmt, __VA_ARGS__);\
            ImGui::PopID();\
        }\

        int row_id = 0;
        const float LTO = static_cast<float>(ensemble.local_time_offset) / 10.0f;
        FIELD_MACRO("Name", "%.*s", int(ensemble.label.length()), ensemble.label.c_str());
        FIELD_MACRO("ID", "%u", ensemble.reference);
        FIELD_MACRO("Country Code", "%s (0x%02X.%01X)", 
            GetCountryString(ensemble.extended_country_code, ensemble.country_id),
            ensemble.extended_country_code, ensemble.country_id);
        FIELD_MACRO("Local Time Offset", "%.1f hours", LTO);
        FIELD_MACRO("Inter Table ID", "%u", ensemble.international_table_id);
        FIELD_MACRO("Total Services", "%u", ensemble.nb_services);
        FIELD_MACRO("Total Reconfig", "%u", ensemble.reconfiguration_count);
        #undef FIELD_MACRO

        ImGui::EndTable();
    }
}

void RenderRadioDateTime(BasicRadio& radio) {
    const auto& info = radio.GetMiscInfo();
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("Date & Time", 2, flags)) {
        #define FIELD_MACRO(name, fmt, ...) {\
            ImGui::PushID(row_id++);\
            ImGui::TableNextRow();\
            ImGui::TableSetColumnIndex(0);\
            ImGui::TextWrapped(name);\
            ImGui::TableSetColumnIndex(1);\
            ImGui::TextWrapped(fmt, __VA_ARGS__);\
            ImGui::PopID();\
        }\

        int row_id = 0;
        FIELD_MACRO("Date", "%02d/%02d/%04d", 
            info.datetime.day, info.datetime.month, info.datetime.year);
        FIELD_MACRO("Time", "%02u:%02u:%02u.%03u", 
            info.datetime.hours, info.datetime.minutes, 
            info.datetime.seconds, info.datetime.milliseconds);
        FIELD_MACRO("CIF Counter", "%4u = %2u|%-3u", 
            info.cif_counter.GetTotalCount(),
            info.cif_counter.upper_count, info.cif_counter.lower_count);

        #undef FIELD_MACRO
        ImGui::EndTable();
    }
}

void RenderAudioControls(AudioPipeline& audio) {
    float& volume_gain = audio.get_global_gain();

    static bool is_overgain = false;
    static float last_unmuted_volume = 0.0f;

    bool is_muted = (volume_gain == 0.0f);
    const float max_gain = is_overgain ? 6.0f : 2.0f;
    if (!is_overgain) {
        volume_gain = (volume_gain > max_gain) ? max_gain : volume_gain;
    }

    ImGui::PushItemWidth(-1.0f);
    ImGui::Text("Volume");

    const float volume_scale = 100.0f;
    float curr_volume = volume_gain * volume_scale;
    if (ImGui::SliderFloat("###Volume", &curr_volume, 0.0f, max_gain*volume_scale, "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
        volume_gain = (curr_volume / volume_scale);
        if (volume_gain > 0.0f) {
            last_unmuted_volume = volume_gain;
        } else {
            last_unmuted_volume = 1.0f;
        }
    }
    ImGui::PopItemWidth();

    if (is_muted) {
        if (ImGui::Button("Unmute")) {
            volume_gain = last_unmuted_volume;
        }
    } else {
        if (ImGui::Button("Mute")) {
            last_unmuted_volume = volume_gain;
            volume_gain = 0.0f;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(is_overgain ? "Normal gain" : "Boost gain")) {
        is_overgain = !is_overgain;
    }
}

void RenderOFDMConstellation(Radio_View_Controller& ctx, tcb::span<const std::complex<float>> data) {
    const float menuWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(menuWidth);

    const size_t N = data.size();

    ImGui::SliderFloat("Scale", &ctx.constellation_scale, 0.0f, 1.0f);
    
    // NOTE: This is hard coded into ImGui::ConstellationDiagram
    //       Taken from sdrplusplus/core/src/gui/widgets/constellation_diagram.h
    const size_t M = 1024;
    const size_t K = std::min(M,N);
    auto& diagram = ctx.constellation_diagram;
    dsp::complex_t *buf = diagram.acquireBuffer();

    const float scale = ctx.constellation_scale / ctx.average_constellation_magnitude;
    // we compute the average magnitude over time
    float magnitude = 0.0f;
    for (size_t i = 0; i < K; i++) {
        const float I = data[i].real();
        const float Q = data[i].imag();
        magnitude += (std::abs(I) + std::abs(Q));
        buf[i].re = I * scale;
        buf[i].im = Q * scale;
    }
    diagram.releaseBuffer();

    const float average_magnitude = magnitude / float(K);
    ctx.average_constellation_magnitude = average_magnitude;

    diagram.draw();
}
