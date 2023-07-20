#include "render_dab_module.h"

#include <cmath>
#include <fmt/core.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "ofdm/ofdm_demodulator.h"
#include "basic_radio/basic_radio.h"
#include "audio/audio_mixer.h"

#include "render_formatters.h"

void RenderOFDMState(OFDM_Demod& demod);
void RenderOFDMControls(OFDM_Demod& demod);
// void DrawConstellation(tcb::span<const std::complex<float>> data);

void RenderRadioChannels(BasicRadio& radio);
void RenderRadioStatistics(BasicRadio& radio);
void RenderRadioEnsemble(BasicRadio& radio);
void RenderRadioDateTime(BasicRadio& radio);

void RenderAudioControls(AudioMixer& mixer);

void RenderDABModule(OFDM_Demod& demod, BasicRadio& radio, AudioMixer& mixer) {
    if (ImGui::BeginTabBar("Tab bar")) {
        if (ImGui::BeginTabItem("OFDM")) {
            if (ImGui::BeginTabBar("OFDM tab bar")) {
                if (ImGui::BeginTabItem("State")) {
                    RenderOFDMState(demod);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    RenderOFDMControls(demod);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("DAB")) {
            auto lock_db = std::scoped_lock(radio.GetDatabaseManager().GetDatabaseMutex());

            if (ImGui::BeginTabBar("DAB tab bar")) {
                if (ImGui::BeginTabItem("Channels")) {
                    RenderRadioChannels(radio);
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
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
            RenderAudioControls(mixer);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    auto data_vec = demod.GetFrameDataVec();
    // float menuWidth = ImGui::GetContentRegionAvail().x;
    // ImGui::SetNextItemWidth(menuWidth);
    // DrawConstellation(data_vec);
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

    if (ImGui::Button("Reset")) {
        demod.Reset();
    }

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

void RenderRadioChannels(BasicRadio& radio) {
    // TODO: Replace this with another form of reset
    // if (ImGui::Button("Reset")) {
    //     radio.Reset();
    // }

    auto& db = radio.GetDatabaseManager().GetDatabase();
    auto window_label = fmt::format("Subchannels ({})###Subchannels Full List", db.subchannels.size());
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Subchannels table", 2, flags)) {
        ImGui::TableSetupColumn("Service Label",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Codec",            ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int row_id  = 0;
        for (auto& subchannel: db.subchannels) {
            auto service_component = db.GetServiceComponent_Subchannel(subchannel.id);
            auto service = service_component ? db.GetService(service_component->service_reference) : NULL;
            auto service_label = service ? service->label.c_str() : "";

            const auto prot_label = GetSubchannelProtectionLabel(subchannel);
            const uint32_t bitrate_kbps = GetSubchannelBitrate(subchannel);

            ImGui::PushID(row_id++);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", service_label);
            ImGui::TableSetColumnIndex(1);
            auto* dab_plus_channel = radio.Get_DAB_Plus_Channel(subchannel.id);
            if (dab_plus_channel != NULL) {
                const auto& superframe_header = dab_plus_channel->GetSuperFrameHeader();
                const bool is_codec_found = superframe_header.sampling_rate != 0;
                if (is_codec_found) {
                    const char* mpeg_surround = GetMPEGSurroundString(superframe_header.mpeg_surround);
                    ImGui::TextWrapped("DAB+ %uHz %s %s %s", 
                        superframe_header.sampling_rate, 
                        superframe_header.is_stereo ? "Stereo" : "Mono",  
                        GetAACDescriptionString(superframe_header.SBR_flag, superframe_header.PS_flag),
                        mpeg_surround ? mpeg_surround : "");
                } else {
                    ImGui::TextWrapped("DAB+ (no codec info)");
                }

            } else {
                ImGui::TextWrapped("(not DAB/DAB+)");
            }

            if (dab_plus_channel != NULL) {
                auto& controls = dab_plus_channel->GetControls();
                const bool is_selected = controls.GetAllEnabled();
                ImGui::SameLine();
                if (ImGui::Selectable("###select_button", is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (is_selected) {
                        controls.StopAll();
                    } else {
                        controls.RunAll();
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
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

// void DrawConstellation(tcb::span<const std::complex<float>> data) {
//     auto* window = ImGui::GetCurrentWindow();
//     auto& style = ImGui::GetStyle();
//     auto min = window->DC.CursorPos;

//     auto size_arg = ImVec2(0, 0);
//     auto size = ImGui::CalcItemSize(size_arg, ImGui::CalcItemWidth(), ImGui::CalcItemWidth());
//     auto bb = ImRect(min, ImVec2(min.x + size.x, min.y + size.y));

//     ImGui::ItemSize(size, style.FramePadding.y);
//     if (!ImGui::ItemAdd(bb, 0)) {
//         return;
//     }

//     window->DrawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(0, 0, 0, 255));

//     auto color = ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f);
//     const size_t N = data.size();
//     const float A = 1.5f;
//     const float K = 4e6f;

//     for (size_t i = 0; i < N; i++) {
//         auto& x = data[i];
//         const float I = x.real() / K;
//         const float Q = x.imag() / K;
//         if (I > A || I < -A) { continue; }
//         if (Q > A || Q < -A) { continue; }
//         auto pos = ImVec2(
//             ((I/A + 1) * (size.x * 0.5f)) + min.x, 
//             ((Q/A + 1) * (size.y * 0.5f)) + min.y
//         );
//         window->DrawList->AddCircleFilled(pos, 2, color);
//     }
// }
