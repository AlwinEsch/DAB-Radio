#include "render_simple_view.h"
#include "basic_radio/basic_radio.h"

#include "imgui.h"
#include <fmt/core.h>
#include "formatters.h"

#include "render_common.h"

void RenderSimple_ServiceList(BasicRadio* radio, SimpleViewController* controller);
void RenderSimple_Service(BasicRadio* radio, SimpleViewController* controller, Service* service);
void RenderSimple_ServiceComponentList(BasicRadio* radio, SimpleViewController* controller, Service* service);
void RenderSimple_ServiceComponent(BasicRadio* radio, SimpleViewController* controller, ServiceComponent* component);
void RenderSimple_BasicAudioChannel(BasicRadio* radio, SimpleViewController* controller, BasicAudioChannel* channel, const service_id_t service_id);
void RenderSimple_LinkServices(BasicRadio* radio, SimpleViewController* controller);
void RenderSimple_LinkService(BasicRadio* radio, SimpleViewController* controller, LinkService* link_service);
void RenderSimple_GlobalBasicAudioChannelControls(BasicRadio* radio);

// Render a list of the services
void RenderSimple_Root(BasicRadio* radio, SimpleViewController* controller) {
    auto db = radio->GetDatabaseManager()->GetDatabase();
    if (ImGui::Begin("Simple View")) 
    {
        ImGuiID dockspace_id = ImGui::GetID("Simple View Dockspace");
        ImGui::DockSpace(dockspace_id);

        auto* selected_service = db->GetService(controller->selected_service);

        RenderSimple_ServiceList(radio, controller);
        RenderSimple_Service(radio, controller, selected_service);

        RenderEnsemble(radio);
        RenderDateTime(radio);
        RenderDatabaseStatistics(radio);

        RenderSimple_GlobalBasicAudioChannelControls(radio);
        RenderOtherEnsembles(radio);
        RenderSimple_LinkServices(radio, controller);
        RenderSimple_ServiceComponentList(radio, controller, selected_service);
    }
    ImGui::End();
}

void RenderSimple_ServiceList(BasicRadio* radio, SimpleViewController* controller) {
    auto db = radio->GetDatabaseManager()->GetDatabase();
    const auto window_title = fmt::format("Services ({})###Services panel", db->services.size());
    if (ImGui::Begin(window_title.c_str())) {
        auto& search_filter = controller->services_filter;
        search_filter.Draw("###Services search filter", -1.0f);
        if (ImGui::BeginListBox("###Services list", ImVec2(-1,-1))) {
            for (auto& service: db->services) {
                if (!search_filter.PassFilter(service.label.c_str())) {
                    continue;
                }
                const int service_id = static_cast<int>(service.reference);
                const bool is_selected = (service_id == controller->selected_service);
                auto label = fmt::format("{}###{}", service.label, service.reference);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    controller->selected_service = is_selected ? -1 : service_id;
                }
            }
            ImGui::EndListBox();
        }
    }
    ImGui::End();
}

void RenderSimple_Service(BasicRadio* radio, SimpleViewController* controller, Service* service) {
    auto* db = radio->GetDatabaseManager()->GetDatabase();

    if (ImGui::Begin("Service Description") && service) {
        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("Service Description", 2, flags)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            int row_id = 0;
            #define FIELD_MACRO(name, fmt, ...) {\
                ImGui::PushID(row_id++);\
                ImGui::TableNextRow();\
                ImGui::TableSetColumnIndex(0);\
                ImGui::TextWrapped(name);\
                ImGui::TableSetColumnIndex(1);\
                ImGui::TextWrapped(fmt, ##__VA_ARGS__);\
                ImGui::PopID();\
            }\

            FIELD_MACRO("Name", "%.*s", service->label.length(), service->label.c_str());
            FIELD_MACRO("ID", "%u", service->reference);
            FIELD_MACRO("Country ID", "%u", service->country_id);
            FIELD_MACRO("Extended Country Code", "0x%02X", service->extended_country_code);
            FIELD_MACRO("Programme Type", "%u", service->programme_type);
            FIELD_MACRO("Language", "%u", service->language);
            FIELD_MACRO("Closed Caption", "%u", service->closed_caption);

            #undef FIELD_MACRO

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void RenderSimple_ServiceComponentList(BasicRadio* radio, SimpleViewController* controller, Service* service) {
    auto* db = radio->GetDatabaseManager()->GetDatabase();

    // Render the service components along with their associated subchannel
    auto* components = service ? db->GetServiceComponents(service->reference) : NULL;
    const auto window_label = fmt::format("Service Components ({})###Service Components Panel",
        components ? components->size() : 0);
    if (ImGui::Begin(window_label.c_str()) && components) {
        for (auto& component: *components) {
            RenderSimple_ServiceComponent(radio, controller, component);
        }
    }
    ImGui::End();
}

void RenderSimple_ServiceComponent(BasicRadio* radio, SimpleViewController* controller, ServiceComponent* component) {
    auto* db = radio->GetDatabaseManager()->GetDatabase();
    const auto subchannel_id = component->subchannel_id;
    auto* subchannel = db->GetSubchannel(subchannel_id);
    
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Component", 2, flags)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int row_id  = 0;
        #define FIELD_MACRO(name, fmt, ...) {\
            ImGui::PushID(row_id++);\
            ImGui::TableNextRow();\
            ImGui::TableSetColumnIndex(0);\
            ImGui::TextWrapped(name);\
            ImGui::TableSetColumnIndex(1);\
            ImGui::TextWrapped(fmt, ##__VA_ARGS__);\
            ImGui::PopID();\
        }\

        const bool is_audio_type = (component->transport_mode == TransportMode::STREAM_MODE_AUDIO);
        const char* type_str = is_audio_type ? 
            GetAudioTypeString(component->audio_service_type) :
            GetDataTypeString(component->data_service_type);
        
        FIELD_MACRO("Label", "%.*s", component->label.length(), component->label.c_str());
        FIELD_MACRO("Component ID", "%u", component->component_id);
        FIELD_MACRO("Global ID", "%u", component->global_id);
        FIELD_MACRO("Transport Mode", "%s", GetTransportModeString(component->transport_mode));
        FIELD_MACRO("Type", "%s", type_str);
        FIELD_MACRO("Subchannel ID", "%u", component->subchannel_id);

        if (subchannel != NULL) {
            const auto prot_label = GetSubchannelProtectionLabel(*subchannel);
            const uint32_t bitrate_kbps = GetSubchannelBitrate(*subchannel);
            FIELD_MACRO("Start Address", "%u", subchannel->start_address);
            FIELD_MACRO("Capacity Units", "%u", subchannel->length);
            FIELD_MACRO("Protection", "%.*s", prot_label.length(), prot_label.c_str());
            FIELD_MACRO("Bitrate", "%u kb/s", bitrate_kbps);
        }

        #undef FIELD_MACRO
        ImGui::EndTable();
    }

    auto* channel = radio->GetAudioChannel(component->subchannel_id);
    if (channel != NULL) {
        RenderSimple_BasicAudioChannel(radio, controller, channel, component->service_reference);
    }
}

void RenderSimple_BasicAudioChannel(BasicRadio* radio, SimpleViewController* controller, BasicAudioChannel* channel, service_id_t service_id) {
    // Channel controls
    auto& controls = channel->GetControls();
    if (ImGui::Button("Run All")) {
        controls.RunAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop All")) {
        controls.StopAll();
    }
    bool v = false;
    v = controls.GetIsDecodeAudio();
    if (ImGui::Checkbox("Decode audio", &v)) {
        controls.SetIsDecodeAudio(v);
    }
    v = controls.GetIsDecodeData();
    ImGui::SameLine();
    if (ImGui::Checkbox("Decode data", &v)) {
        controls.SetIsDecodeData(v);
    }
    v = controls.GetIsPlayAudio();
    ImGui::SameLine();
    if (ImGui::Checkbox("Play audio", &v)) {
        controls.SetIsPlayAudio(v);
    }

    // Programme associated data
    // 1. Dynamic label
    // 2. MOT slideshow
    auto& label = channel->GetDynamicLabel();
    ImGui::Text("Dynamic label: %.*s", label.length(), label.c_str());

    auto& slideshow_controller = controller->slideshow_controller;
    auto& slideshow_manager = channel->GetSlideshowManager();
    auto& slideshows = slideshow_manager.GetSlideshows();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    if (ImGui::BeginChild("Slideshow", ImVec2(0, 0), true, window_flags)) {
        for (auto& [transport_id, slideshow]: slideshows) {
            auto* texture = slideshow_controller.AddSlideshow(
                { service_id, transport_id},
                slideshow.data, slideshow.nb_data_bytes);

            if (texture != NULL) {
                const auto texture_id = reinterpret_cast<ImTextureID>(texture->GetTextureID());
                const auto texture_size = ImVec2(
                    static_cast<float>(texture->GetWidth()), 
                    static_cast<float>(texture->GetHeight())
                );
                ImGui::SameLine();
                ImGui::Image(texture_id, texture_size);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%.*s", slideshow.name.length(), slideshow.name.c_str());
                }
            }
        }
    }
    ImGui::EndChild();
}

void RenderSimple_LinkServices(BasicRadio* radio, SimpleViewController* controller) {
    auto* db = radio->GetDatabaseManager()->GetDatabase();
    const auto selected_service_id = controller->selected_service;
    auto* service = (selected_service_id == -1) ? NULL : db->GetService(selected_service_id);

    auto* linked_services = service ? db->GetServiceLSNs(service->reference) : NULL;
    const size_t nb_linked_services = linked_services ? linked_services->size() : 0;
    auto window_label = fmt::format("Linked Services ({})###Linked Services", nb_linked_services);

    if (ImGui::Begin(window_label.c_str()) && linked_services) {
        const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
        for (auto& linked_service: *linked_services) {
            RenderSimple_LinkService(radio, controller, linked_service);
        }
    }
    ImGui::End();
}

void RenderSimple_LinkService(BasicRadio* radio, SimpleViewController* controller, LinkService* link_service) {
    auto db = radio->GetDatabaseManager()->GetDatabase();
    auto label = fmt::format("###lsn_{}", link_service->id);

    #define FIELD_MACRO(name, fmt, ...) {\
        ImGui::PushID(row_id++);\
        ImGui::TableNextRow();\
        ImGui::TableSetColumnIndex(0);\
        ImGui::TextWrapped(name);\
        ImGui::TableSetColumnIndex(1);\
        ImGui::TextWrapped(fmt, ##__VA_ARGS__);\
        ImGui::PopID();\
    }\

    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
    if (ImGui::BeginChild(label.c_str(), ImVec2(-1, 0))) {
        static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;

        // Description header
        ImGui::Text("Link Service Description");
        if (ImGui::BeginTable("LSN Description", 2, flags)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            int row_id = 0;
            FIELD_MACRO("LSN", "%u", link_service->id);
            FIELD_MACRO("Active", "%s", link_service->is_active_link ? "Yes" : "No");
            FIELD_MACRO("Hard Link", "%s", link_service->is_hard_link ? "Yes": "No");
            FIELD_MACRO("International", "%s", link_service->is_international ? "Yes" : "No");
            ImGui::EndTable();
        }

        // FM Services
        auto* fm_services = db->Get_LSN_FM_Services(link_service->id);
        if (fm_services) {
            const auto fm_label = fmt::format("FM Services ({})###FM Services", fm_services->size());
            if (ImGui::CollapsingHeader(fm_label.c_str(), ImGuiTreeNodeFlags_None)) {
                if (ImGui::BeginTable("FM Table", 3, flags)) {
                    ImGui::TableSetupColumn("Callsign",         ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    int row_id  = 0;
                    for (auto& fm_service: *fm_services) {
                        ImGui::PushID(row_id++);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextWrapped("%04X", fm_service->RDS_PI_code);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextWrapped("%s", fm_service->is_time_compensated ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(2);
                        auto& frequencies = fm_service->frequencies;
                        for (auto& freq: frequencies) {
                            ImGui::Text("%3.3f MHz", static_cast<float>(freq)*1e-6f);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        }

        // DRM Services
        auto* drm_services = db->Get_LSN_DRM_Services(link_service->id);
        if (drm_services) {
            const auto drm_label = fmt::format("DRM Services ({})###DRM Services", drm_services->size());
            if (ImGui::CollapsingHeader(drm_label.c_str())) {
                if (ImGui::BeginTable("DRM Table", 3, flags)) {
                    ImGui::TableSetupColumn("ID",               ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Time compensated", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Frequencies",      ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    int row_id  = 0;
                    for (auto& drm_service: *drm_services) {
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
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    #undef FIELD_MACRO
}

void RenderSimple_GlobalBasicAudioChannelControls(BasicRadio* radio) {
    auto db = radio->GetDatabaseManager()->GetDatabase();
    auto& subchannels = db->subchannels;

    static bool decode_audio = true;
    static bool decode_data = true;
    static bool play_audio = false;

    bool is_changed = false;

    if (ImGui::Begin("Global Channel Controls")) {
        if (ImGui::Button("Apply Settings")) {
            is_changed = true;
        }
        ImGui::Checkbox("Decode Audio", &decode_audio);
        ImGui::SameLine();
        ImGui::Checkbox("Decode Data", &decode_data);
        ImGui::SameLine();
        ImGui::Checkbox("Play Audio", &play_audio);
    }
    ImGui::End();

    if (!is_changed) {
        return;
    }

    for (auto& subchannel: subchannels) {
        auto* channel = radio->GetAudioChannel(subchannel.id);
        if (channel == NULL) continue;

        auto& control = channel->GetControls();
        control.SetIsDecodeAudio(decode_audio);
        control.SetIsDecodeData(decode_data);
        control.SetIsPlayAudio(play_audio);
    }
}