#include "./render_dab_module.h"

#include <cmath>
#include <fmt/core.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <gui/widgets/constellation_diagram.h>
#include <gui/tuner.h>

// NOTE: gui/gui.h for some reason has defines for min/max
#include <gui/gui.h>
#undef min
#undef max

#include "./dab_decoder.h"
#include "./render_formatters.h"
#include "./texture.h"
#include "ofdm/ofdm_demodulator.h"
#include "basic_radio/basic_radio.h"
#include "basic_radio/basic_dab_plus_channel.h"
#include "basic_radio/basic_slideshow.h"
#include "audio/audio_mixer.h"

DAB_Decoder_ImGui::DAB_Decoder_ImGui(DAB_Decoder& _decoder) 
: decoder(_decoder)
{
    const size_t max_slideshows = 20;
    slideshow_textures_cache.set_max_size(max_slideshows);

    auto& radio = decoder.GetBasicRadio();
    radio.On_DAB_Plus_Channel().Attach([this, max_slideshows](subchannel_id_t subchannel_id, Basic_DAB_Plus_Channel& channel) {
        auto& slideshow_manager = channel.GetSlideshowManager();
        slideshow_manager.SetMaxSize(max_slideshows);
    });
}

Texture* DAB_Decoder_ImGui::TryGetSlideshowTexture(
    subchannel_id_t subchannel_id, mot_transport_id_t transport_id, 
    const uint8_t* data, const size_t total_bytes)
{
    const uint32_t key = GetSlideshowKey(subchannel_id, transport_id);
    auto res = slideshow_textures_cache.find(key);
    if (res == nullptr) {
        auto& v = slideshow_textures_cache.emplace(key, Texture::LoadFromMemory(data, total_bytes));
        return v.get();
    }
    return res->get();
}

// We need to manually clamp values since ImGui_AlwaysClamp doesn't work
void ClampValue(int& value, int min, int max) {
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }
}

// ofdm demodulator
void RenderOFDMState(OFDM_Demod& demod);
void RenderOFDMControls(OFDM_Demod& demod);
void RenderOFDMConstellation(DAB_Decoder_ImGui& decoder_imgui, tcb::span<const std::complex<float>> data);
// basic radio
void RenderRadioServices(DAB_Decoder_ImGui& ctx);
void RenderRadioService(DAB_Decoder_ImGui& ctx);
void RenderRadioStatistics(BasicRadio& radio);
void RenderRadioEnsemble(BasicRadio& radio);
void RenderRadioDateTime(BasicRadio& radio);
// audio mixer
void RenderAudioControls(AudioMixer& mixer);

void RenderDABModule(DAB_Decoder_ImGui& ctx) {
    auto& decoder = ctx.decoder;
    if (ImGui::BeginTabBar("Tab bar")) {
        if (ImGui::BeginTabItem("OFDM")) {
            auto& demod = decoder.GetOFDMDemodulator();
            if (ImGui::Button("Reset")) {
                demod.Reset();
            }

            if (ImGui::BeginTabBar("OFDM tab bar")) {
                if (ImGui::BeginTabItem("State")) {
                    RenderOFDMState(demod);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Controls")) {
                    RenderOFDMControls(demod);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Constellation")) {
                    const auto& constellation = demod.GetFrameDataVec();
                    RenderOFDMConstellation(ctx, constellation);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("DAB")) {
            auto lock_radio = std::unique_lock(decoder.GetBasicRadioMutex());
            auto& radio = decoder.GetBasicRadio();
            auto lock_db = std::scoped_lock(radio.GetDatabaseManager().GetDatabaseMutex());

            if (ImGui::Button("Reset")) {
                decoder.RaiseResetBasicRadioFlag();
                ctx.focused_service_id = 0;
            }

            if (ImGui::BeginTabBar("DAB tab bar")) {
                if (ImGui::BeginTabItem("Channels")) {
                    RenderRadioServices(ctx);
                    ImGui::Separator();
                    RenderRadioService(ctx);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Ensemble")) {
                    RenderRadioEnsemble(radio);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Date&Time")) {
                    RenderRadioDateTime(radio);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Statistics")) {
                    RenderRadioStatistics(radio);
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
                        auto& db = radio.GetDatabaseManager().GetDatabase();
                        for (auto& service: db.services) {
                            auto* components = db.GetServiceComponents(service.reference);
                            if (components != nullptr) {
                                for (auto component: *components) {
                                    auto* channel = radio.Get_DAB_Plus_Channel(component->subchannel_id);
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
                        }
                    }

                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
            auto& audio_player = decoder.GetAudioPlayer();
            auto& mixer = audio_player.GetMixer();
            RenderAudioControls(mixer);
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
    ImGui::Text("Fine freq: %.2f Hz", demod.GetFineFrequencyOffset());
    ImGui::Text("Coarse freq: %.2f Hz", demod.GetCoarseFrequencyOffset());
    ImGui::Text("Net freq: %.2f Hz", demod.GetNetFrequencyOffset());
    ImGui::Text("Signal level: %.2f", demod.GetSignalAverage());
    ImGui::Text("Frames read: %d", demod.GetTotalFramesRead());
    ImGui::Text("Frames desynced: %d", demod.GetTotalFramesDesync());

    #undef ENUM_TO_STRING
}

void RenderOFDMControls(OFDM_Demod& demod) {
    auto& cfg = demod.GetConfig();
    auto params = demod.GetOFDMParams();
    const int MAX_COARSE_FREQ_OFFSET = 500000;
    // ImGui::Checkbox("Update data symbol mag", &cfg.data_sym_mag.is_update);
    // ImGui::Checkbox("Update tii symbol mag", &cfg.is_update_tii_sym_mag);
    ImGui::Checkbox("Coarse frequency correction", &cfg.sync.is_coarse_freq_correction);

    ImGui::SliderFloat("Fine frequency beta", &cfg.sync.fine_freq_update_beta, 0.0f, 1.0f, "%.2f");
    if (ImGui::SliderInt("Max coarse frequency (Hz)", &cfg.sync.max_coarse_freq_correction, 0, MAX_COARSE_FREQ_OFFSET)) {
        const int N = cfg.sync.max_coarse_freq_correction / (int)params.freq_carrier_spacing;
        cfg.sync.max_coarse_freq_correction = N * (int)params.freq_carrier_spacing;
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

    ImGui::SliderFloat("Data sym mag update beta", &cfg.data_sym_mag.update_beta, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("L1 signal update beta", &cfg.signal_l1.update_beta, 0.0f, 1.0f, "%.2f");
}

void RenderRadioServices(DAB_Decoder_ImGui& ctx) {
    auto& radio = ctx.decoder.GetBasicRadio();
    auto& db = radio.GetDatabaseManager().GetDatabase();
    
    // Render channel list selector
    auto focused_service = db.GetService(ctx.focused_service_id);
    auto focused_service_label = focused_service ? focused_service->label.c_str() : "None selected";
    const auto total_services = db.services.size();

    const auto dropdown_label = fmt::format("Services ({})###services_dropdown", total_services);
    if (ImGui::BeginCombo(dropdown_label.c_str(), focused_service_label)) {
        for (auto& service: db.services) {
            ImGui::PushID(service.reference);

            auto* components = db.GetServiceComponents(service.reference);
            const char* service_label = service.label.c_str();

            bool is_play_audio = false;
            bool is_decode_data = false;
            if (components != nullptr) {
                for (auto component: *components) {
                    auto* channel = radio.Get_DAB_Plus_Channel(component->subchannel_id);
                    if (channel != nullptr) {
                        auto& controls = channel->GetControls();
                        is_play_audio |= controls.GetIsPlayAudio();
                        is_decode_data |= controls.GetIsDecodeData();
                    }
                }
            }

            const std::string status_label = fmt::format("{}/{}",
                is_play_audio ? "A" : "-",
                is_decode_data ? "D" : "-"
            );

            const bool is_focused = service.reference == ctx.focused_service_id;
            if (ImGui::Selectable(service_label, is_focused)) {
                ctx.focused_service_id = service.reference;
            }
            
            const float offset = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(status_label.c_str()).x;
            ImGui::SameLine(offset);
            ImGui::Text("%.*s", int(status_label.length()), status_label.c_str());

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
}

void RenderRadioService(DAB_Decoder_ImGui& ctx) {
    auto& radio = ctx.decoder.GetBasicRadio();
    auto& db = radio.GetDatabaseManager().GetDatabase();

    auto* service = db.GetService(ctx.focused_service_id);
    if (service == nullptr) {
        return;
    }

    auto* service_components = db.GetServiceComponents(service->reference);
    if (service_components == nullptr) {
        return;
    }
    
    ServiceComponent* service_component = nullptr;
    if (service_components->size() == 1) {
        for (auto _component: *service_components) {
            service_component = _component;
            break;
        }
    } else {
        static int selected_index = 0;
        const int total_service_components = service_components->size();
        ImGui::SliderInt("Component", &selected_index, 0, total_service_components-1, "%d", ImGuiSliderFlags_AlwaysClamp);
        ClampValue(selected_index, 0, total_service_components-1);
        int index = 0;
        for (auto _component: *service_components) {
            if (index == selected_index) {
                service_component = _component;
                break;
            }
            index++;
        }
    }
    if (service_component == nullptr) {
        return;
    }

    auto subchannel = db.GetSubchannel(service_component->subchannel_id);
    if (subchannel == nullptr) {
        return;
    }

    auto* channel = radio.Get_DAB_Plus_Channel(subchannel->id);
    if (channel == nullptr) {
        ImGui::Text("Not a DAB+ channel");
        return;
    }

    auto& controls = channel->GetControls();
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
    ImGui::Separator();

    auto get_codec_description = [&]() -> std::string {
        const auto prot_label = GetSubchannelProtectionLabel(*subchannel);
        const uint32_t bitrate_kbps = GetSubchannelBitrate(*subchannel);
        const auto& superframe_header = channel->GetSuperFrameHeader();
        const bool is_codec_found = superframe_header.sampling_rate != 0;
        if (is_codec_found) {
            const char* mpeg_surround = GetMPEGSurroundString(superframe_header.mpeg_surround);
            return fmt::format("DAB+ {}Hz {} {} {} @ {} kbp/s", 
                superframe_header.sampling_rate, 
                superframe_header.is_stereo ? "Stereo" : "Mono",  
                GetAACDescriptionString(superframe_header.SBR_flag, superframe_header.PS_flag),
                mpeg_surround ? mpeg_surround : "",
                bitrate_kbps
            );
        } else {
            return std::string("DAB+ (no codec info)");
        }
    };

    auto codec_description = get_codec_description();
    const auto& dynamic_label = channel->GetDynamicLabel();
    ImGui::TextWrapped("%.*s", int(codec_description.length()), codec_description.c_str());
    ImGui::TextWrapped("%.*s", int(dynamic_label.length()), dynamic_label.c_str());

    ImGui::Separator();

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
        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("Status", 2, flags)) {
            ImGui::TableSetupColumn("Decoding Stage", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            int row_id  = 0;
            FIELD_MACRO("Firecode", "%s", channel->IsFirecodeError() ? "Error" : "Good");
            FIELD_MACRO("Reed Solomon", "%s", channel->IsRSError() ? "Error" : "Good");
            FIELD_MACRO("Access Unit CRC", "%s", channel->IsAUError() ? "Error" : "Good");
            FIELD_MACRO("AAC Decoder", "%s", channel->IsCodecError() ? "Error" : "Good");
            
            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("channel_tabs")) {
        if (ImGui::BeginTabItem("Slideshow")) {
            auto& slideshow_manager = channel->GetSlideshowManager();
            auto slideshow_lock = std::scoped_lock(slideshow_manager.GetMutex());
            auto& slideshows = slideshow_manager.GetSlideshows();
            const int total_slideshows = slideshows.size();
            
            if (total_slideshows > 0) {
                static int slideshow_index = 0;

                if (total_slideshows > 1) {
                    ImGui::SliderInt("###Image", &slideshow_index, 0, total_slideshows-1, "%d", ImGuiSliderFlags_AlwaysClamp);
                    ClampValue(slideshow_index, 0, total_slideshows-1);
                } else {
                    slideshow_index = 0;
                }

                Basic_Slideshow* slideshow = nullptr;
                int index = 0;
                for (auto& _slideshow: slideshows) {
                    if (index == slideshow_index) {
                        slideshow = &_slideshow;
                        break;
                    }
                    index++;
                }

                if (slideshow != nullptr) {
                    auto data = slideshow->image_data;
                    const auto* texture = ctx.TryGetSlideshowTexture(subchannel->id, slideshow->transport_id, data.data(), data.size());
                    if (texture != nullptr) {
                        const auto texture_id = texture->GetTextureID(); 

                        const ImVec2 region_min = ImGui::GetWindowContentRegionMin();
                        const ImVec2 region_max = ImGui::GetWindowContentRegionMax();
                        const float region_width = float(region_max.x - region_min.x);
                        const float scale = region_width / static_cast<float>(texture->GetWidth());

                        const auto texture_size = ImVec2(
                            static_cast<float>(texture->GetWidth()) * scale, 
                            static_cast<float>(texture->GetHeight()) * scale
                        );
                        ImGui::Image(ImTextureID(texture_id), texture_size);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%.*s", int(slideshow->name.length()), slideshow->name.c_str());
                        }

                        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
                        if (ImGui::BeginTable("Status", 2, flags)) {
                            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int row_id  = 0;

                            FIELD_MACRO("Subchannel ID", "%u", subchannel->id);
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
                    }
                }
            } else {
                ImGui::Text("No slideshows available yet");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Linked Services")) {
            auto* linked_services = db.GetServiceLSNs(service->reference);
            const size_t total_linked_services = linked_services ? linked_services->size() : 0;
            if (total_linked_services > 0) {
                static int selected_link_index = 0;

                if (total_linked_services > 1) {
                    ImGui::SliderInt("Linked Service", &selected_link_index, 0, total_linked_services-1, "%d", ImGuiSliderFlags_AlwaysClamp);
                    ClampValue(selected_link_index, 0, total_linked_services-1);
                } else {
                    selected_link_index = 0;
                }
                
                LinkService* linked_service = nullptr;
                {
                    int index = 0;
                    for (auto* _linked_service: *linked_services) {
                        if (index == selected_link_index) {
                            linked_service = _linked_service;
                            break;
                        }
                        index++;
                    }
                }
                
                if (linked_service != nullptr) {
                    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
                    // Description header
                    ImGui::Text("Link Service Description");
                    if (ImGui::BeginTable("LSN Description", 2, flags)) {
                        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        int row_id = 0;
                        FIELD_MACRO("LSN", "%u", linked_service->id);
                        FIELD_MACRO("Active", "%s", linked_service->is_active_link ? "Yes" : "No");
                        FIELD_MACRO("Hard Link", "%s", linked_service->is_hard_link ? "Yes": "No");
                        FIELD_MACRO("International", "%s", linked_service->is_international ? "Yes" : "No");
                        ImGui::EndTable();
                    }
                    
                    auto* fm_services = db.Get_LSN_FM_Services(linked_service->id);
                    if (fm_services != nullptr) {
                        ImGui::Separator();
                        ImGui::Text("FM Services (%zu)", fm_services->size());
                        if (ImGui::BeginTable("FM Table", 3, flags)) {
                            ImGui::TableSetupColumn("Callsign",         ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int row_id  = 0;
                            for (auto* fm_service: *fm_services) {
                                ImGui::PushID(row_id++);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextWrapped("%04X", fm_service->RDS_PI_code);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextWrapped("%s", fm_service->is_time_compensated ? "Yes" : "No");
                                ImGui::TableSetColumnIndex(2);
                                auto& frequencies = fm_service->frequencies;
                                for (auto& freq: frequencies) {
                                    std::string label;
                                    if (freq >= 1'000'000'000) {
                                        label = fmt::format("{:3.3} GHz", static_cast<float>(freq)*1e-9f);
                                    } else if (freq >= 1'000'000) {
                                        label = fmt::format("{:3.3} MHz", static_cast<float>(freq)*1e-6f);
                                    } else if (freq >= 1'000) {
                                        label = fmt::format("{:3.3} kHz", static_cast<float>(freq)*1e-3f);
                                    } else {
                                        label = fmt::format("{:3} Hz", static_cast<float>(freq));
                                    }

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

                    auto* drm_services = db.Get_LSN_DRM_Services(linked_service->id);
                    if (drm_services != nullptr) {
                        ImGui::Separator();
                        ImGui::Text("DRM Services (%zu)", drm_services->size());
                        if (ImGui::BeginTable("DRM Table", 3, flags)) {
                            ImGui::TableSetupColumn("ID",               ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int row_id  = 0;
                            for (auto* drm_service: *drm_services) {
                                ImGui::PushID(row_id++);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextWrapped("%u", drm_service->drm_code);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextWrapped("%s", drm_service->is_time_compensated ? "Yes" : "No");
                                ImGui::TableSetColumnIndex(2);
                                auto& frequencies = drm_service->frequencies;
                                for (auto& freq: frequencies) {
                                    ImGui::Text("%3.3f MHz", static_cast<float>(freq)*1e-6f);
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
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("Details", 2, flags)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

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
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("Service Component", 2, flags)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

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
    const auto stats = radio.GetDatabaseManager().GetDatabaseStatistics();
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Date & Time", 2, flags)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

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
        FIELD_MACRO("Total", "%d", stats.nb_total);
        FIELD_MACRO("Pending", "%d", stats.nb_pending);
        FIELD_MACRO("Completed", "%d", stats.nb_completed);
        FIELD_MACRO("Conflicts", "%d", stats.nb_conflicts);
        FIELD_MACRO("Updates", "%d", stats.nb_updates);

        #undef FIELD_MACRO
        ImGui::EndTable();
    }
}

void RenderRadioEnsemble(BasicRadio& radio) {
    auto& db = radio.GetDatabaseManager().GetDatabase();
    auto& ensemble = db.ensemble;

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Ensemble description", 2, flags)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

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
        FIELD_MACRO("Name", "%.*s", ensemble.label.length(), ensemble.label.c_str());
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
    const auto info = radio.GetDatabaseManager().GetDABMiscInfo();
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Date & Time", 2, flags)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

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
        FIELD_MACRO("CIF Counter", "%+4u = %+2u|%-3u", 
            info.cif_counter.GetTotalCount(),
            info.cif_counter.upper_count, info.cif_counter.lower_count);

        #undef FIELD_MACRO
        ImGui::EndTable();
    }
}

void RenderAudioControls(AudioMixer& mixer) {
    auto& volume_gain = mixer.GetOutputGain();

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

void RenderOFDMConstellation(DAB_Decoder_ImGui& decoder_imgui, tcb::span<const std::complex<float>> data) {
    const float menuWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(menuWidth);

    const size_t N = data.size();

    ImGui::SliderFloat("Scale", &decoder_imgui.constellation_scale, 0.0f, 1.0f);
    
    // NOTE: This is hard coded into ImGui::ConstellationDiagram
    //       Taken from sdrplusplus/core/src/gui/widgets/constellation_diagram.h
    const size_t M = 1024;
    const size_t K = std::min(M,N);
    auto& diagram = decoder_imgui.constellation_diagram;
    dsp::complex_t *buf = diagram.acquireBuffer();

    const float scale = decoder_imgui.constellation_scale / decoder_imgui.average_constellation_magnitude;
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
    decoder_imgui.average_constellation_magnitude = average_magnitude;

    diagram.draw();
}
