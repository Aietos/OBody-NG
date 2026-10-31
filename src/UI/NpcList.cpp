#include "UI/NpcList.h"
#include "SKSEMenuFramework.h"
#include "UI/Translations.h"
#include "UI/Util.h"
#include "UI/Widgets.h"
#include "Body/Body.h"
#include "Preview/Preview.h"

namespace {
    struct NpcEntry {
        RE::FormID formID;
        std::string name;
        std::wstring searchText;
        float distance;  // from the player, negative when not loaded
    };

    // Game units in a meter, as the distances are shown
    constexpr float kMetersPerUnit{0.01428f};

    // The nearby NPCs are the ones within this many meters of the player
    // TODO: Can consider making this adjustable
    constexpr float kNearbyMeters{40.0f};

    // Header colours
    constexpr ImGuiMCP::ImVec4 kHeaderBackground{0.26f, 0.21f, 0.14f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kHeaderText{0.96f, 0.84f, 0.58f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kHintText{0.7f, 0.7f, 0.75f, 1.0f};

    // A section's title, with its button on the same line
    bool SectionTitle(const char* a_title, const char* a_button) {
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kHeaderText);
        ImGuiMCP::TextUnformatted(a_title);
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        return ImGuiMCP::Button(a_button);
    }

    std::vector<NpcEntry> g_nearby;
    std::vector<NpcEntry> g_all;
    bool g_allScanned{false};

    char g_nearbySearch[128] = "";
    char g_allSearch[128] = "";

    bool IsListed(RE::Actor* a_actor) {
        return UI::Util::IsPresetTarget(a_actor) && !Preview::Session::GetInstance().IsAnyCopy(a_actor);
    }

    std::wstring Query(const char* a_search) { return UI::Util::ToLowerWide(UI::Util::Utf8ToWide(a_search)); }

    NpcEntry MakeEntry(RE::Actor* a_actor, const float a_distance) {
        const char* const displayName{a_actor->GetDisplayFullName()};
        std::string name{displayName && *displayName ? displayName : std::format("{:08X}", a_actor->GetFormID())};
        auto searchText{UI::Util::ToLowerWide(UI::Util::Utf8ToWide(name))};
        return {a_actor->GetFormID(), std::move(name), std::move(searchText), a_distance};
    }

    void ScanNearby() {
        g_nearby.clear();

        auto* const player{RE::PlayerCharacter::GetSingleton()};
        g_nearby.push_back(MakeEntry(player, 0.0f));

        if (auto* const processLists{RE::ProcessLists::GetSingleton()}) {
            for (const auto& handle : processLists->highActorHandles) {
                const auto actor{handle.get()};
                if (!actor || !actor->Is3DLoaded() || !IsListed(actor.get())) continue;
                const float distance{player->GetPosition().GetDistance(actor->GetPosition())};
                if (distance * kMetersPerUnit > kNearbyMeters) continue;
                g_nearby.push_back(MakeEntry(actor.get(), distance));
            }
        }

        std::ranges::sort(g_nearby, {}, &NpcEntry::distance);
    }

    // Every actor the game keeps in memory, which includes the persistent NPCs of cells that aren't loaded
    void ScanAll() {
        std::vector<RE::Actor*> actors;
        {
            const auto [map, lock]{RE::TESForm::GetAllForms()};
            RE::BSReadLockGuard guard{lock.get()};
            if (map) {
                for (const auto& [formID, form] : *map) {
                    if (form && form->GetFormType() == RE::FormType::ActorCharacter) {
                        actors.push_back(form->As<RE::Actor>());
                    }
                }
            }
        }

        g_all.clear();
        auto* const player{RE::PlayerCharacter::GetSingleton()};
        for (auto* const actor : actors) {
            if (!actor || actor->IsDeleted() || !IsListed(actor)) continue;
            g_all.push_back(MakeEntry(
                actor, actor->Is3DLoaded() ? player->GetPosition().GetDistance(actor->GetPosition()) : -1.0f));
        }

        std::ranges::sort(g_all, [](const NpcEntry& a, const NpcEntry& b) {
            // The search text is the name in lowercase, the comparison ignores case anyway
            const int cmp{UI::Util::CompareLinguistic(a.searchText, b.searchText)};
            return cmp != 0 ? cmp < 0 : a.formID < b.formID;
        });
        g_allScanned = true;

        logger::info("NPC scan found {} NPCs", g_all.size());
    }

    bool RenderRow(const NpcEntry& a_entry, RE::Actor* a_actor, const RE::Actor* a_target) {
        bool picked{false};

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();

        ImGuiMCP::PushID(static_cast<int>(a_entry.formID));
        if (ImGuiMCP::Selectable(a_entry.name.c_str(), a_actor == a_target,
                                 ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns)) {
            picked = true;
        }
        ImGuiMCP::PopID();

        ImGuiMCP::TableNextColumn();
        if (a_entry.distance >= 0.0f) {
            // Units to meters
            ImGuiMCP::Text("%.0f m", a_entry.distance * kMetersPerUnit);
        } else {
            ImGuiMCP::TextDisabled("%s", UI::Translations::Get("obody_npc_unloaded").c_str());
        }

        ImGuiMCP::TableNextColumn();
        const auto presetName{
            ActorTracker::Registry::GetInstance().GetPresetNameForActor(a_actor, Body::OBody::IsFemale(a_actor))};
        ImGuiMCP::TextUnformatted(presetName ? presetName->c_str() : "-");

        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextDisabled("%08X", a_entry.formID);

        return picked;
    }

    RE::Actor* RenderTable(const char* a_id, const std::vector<NpcEntry>& a_entries, const std::wstring& a_query,
                           const RE::Actor* a_target, const float a_height) {
        std::vector<const NpcEntry*> shown;
        shown.reserve(a_entries.size());
        for (const auto& entry : a_entries) {
            if (a_query.empty() || entry.searchText.find(a_query) != std::wstring::npos) {
                shown.push_back(&entry);
            }
        }

        constexpr auto flags{ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_BordersInnerV |
                             ImGuiMCP::ImGuiTableFlags_BordersOuterH | ImGuiMCP::ImGuiTableFlags_ScrollY |
                             ImGuiMCP::ImGuiTableFlags_SizingStretchProp};

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_CellPadding, ImGuiMCP::ImVec2{8.0f, 5.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TableHeaderBg, kHeaderBackground);
        if (!ImGuiMCP::BeginTable(a_id, 4, flags, ImGuiMCP::ImVec2{0.0f, a_height})) {
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::PopStyleVar();
            return nullptr;
        }

        ImGuiMCP::TableSetupColumn(UI::Translations::Get("obody_npc_name").c_str(), 0, 3.0f);
        ImGuiMCP::TableSetupColumn(UI::Translations::Get("obody_npc_distance").c_str(), 0, 1.0f);
        ImGuiMCP::TableSetupColumn(UI::Translations::Get("obody_npc_preset").c_str(), 0, 3.0f);
        ImGuiMCP::TableSetupColumn("ID", 0, 1.2f);

        // The header stays in place while the rows scroll
        ImGuiMCP::TableSetupScrollFreeze(0, 1);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kHeaderText);
        ImGuiMCP::TableHeadersRow();
        ImGuiMCP::PopStyleColor();

        RE::Actor* picked{nullptr};

        // Only the visible rows are drawn, the scan of every NPC can find thousands
        auto* const clipper{ImGuiMCP::ImGuiListClipperManager::Create()};
        ImGuiMCP::ImGuiListClipperManager::Begin(clipper, static_cast<int>(shown.size()), -1.0f);
        while (ImGuiMCP::ImGuiListClipperManager::Step(clipper)) {
            for (int i = clipper->DisplayStart; i < clipper->DisplayEnd; ++i) {
                const auto& entry{*shown[static_cast<std::size_t>(i)]};
                auto* const actor{RE::TESForm::LookupByID<RE::Actor>(entry.formID)};
                if (actor && RenderRow(entry, actor, a_target)) {
                    picked = actor;
                }
            }
        }
        ImGuiMCP::ImGuiListClipperManager::End(clipper);
        ImGuiMCP::ImGuiListClipperManager::Destroy(clipper);

        ImGuiMCP::EndTable();
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::PopStyleVar();
        return picked;
    }
}  // namespace

RE::Actor* UI::NpcList::Render(const RE::Actor* a_target, const bool a_appearing) {
    if (a_appearing) {
        ScanNearby();
    }

    if (SectionTitle(UI::Translations::Get("obody_nearby_npcs").c_str(),
                     UI::Translations::Get("obody_npc_refresh").c_str())) {
        ScanNearby();
    }

    UI::Widgets::SearchField(UI::Translations::Get("obody_search_npcs").c_str(), "##NearbyNpcSearch", g_nearbySearch,
                             sizeof(g_nearbySearch));

    const float available{ImGuiMCP::GetContentRegionAvail().y};
    RE::Actor* picked{RenderTable("##NearbyNpcs", g_nearby, Query(g_nearbySearch), a_target, available * 0.4f)};

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    if (SectionTitle(UI::Translations::Get("obody_all_npcs").c_str(),
                     UI::Translations::Get("obody_npc_scan").c_str())) {
        ScanAll();
    }
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kHintText);
    ImGuiMCP::TextWrapped("%s", UI::Translations::Get("obody_all_npcs_hint").c_str());
    ImGuiMCP::PopStyleColor();
    ImGuiMCP::Spacing();

    if (g_allScanned && !g_all.empty()) {
        UI::Widgets::SearchField(UI::Translations::Get("obody_search_npcs").c_str(), "##AllNpcSearch", g_allSearch,
                                 sizeof(g_allSearch));

        if (auto* const actor{RenderTable("##AllNpcs", g_all, Query(g_allSearch), a_target, 0.0f)}) {
            picked = actor;
        }
    }

    return picked;
}
