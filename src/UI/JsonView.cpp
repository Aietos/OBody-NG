#include "UI/JsonView.h"
#include "SKSEMenuFramework.h"
#include "UI/Translations.h"

namespace {
    const ImGuiMCP::ImVec4 kKey{0.60f, 0.80f, 1.00f, 1.0f};
    const ImGuiMCP::ImVec4 kString{0.90f, 0.68f, 0.45f, 1.0f};
    const ImGuiMCP::ImVec4 kNumber{0.70f, 0.90f, 0.60f, 1.0f};
    const ImGuiMCP::ImVec4 kKeyword{0.78f, 0.60f, 0.95f, 1.0f};  // true / false / null
    const ImGuiMCP::ImVec4 kPunct{0.62f, 0.62f, 0.66f, 1.0f};
    const ImGuiMCP::ImVec4 kDim{0.45f, 0.45f, 0.48f, 1.0f};

    // 0 = nothing, 1 = expand all, -1 = collapse all
    int g_forceOpen = 0;

    void Seg(const std::string& text, const ImGuiMCP::ImVec4& color) {
        ImGuiMCP::SameLine(0.0f, 0.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
        ImGuiMCP::TextUnformatted(text.c_str());
        ImGuiMCP::PopStyleColor();
    }

    void BeginLeafLine(const char* id) {
        ImGuiMCP::TreeNodeEx(id, ImGuiMCP::ImGuiTreeNodeFlags_Leaf | ImGuiMCP::ImGuiTreeNodeFlags_NoTreePushOnOpen);
    }

    void KeyPrefix(const char* key) {
        if (!key) return;
        Seg(std::format("\"{}\"", key), kKey);
        Seg(": ", kPunct);
    }

    void Comma(bool comma) {
        if (comma) Seg(",", kPunct);
    }

    void Scalar(const rapidjson::Value& v) {
        if (v.IsString())
            Seg(std::format("\"{}\"", v.GetString()), kString);
        else if (v.IsBool())
            Seg(v.GetBool() ? "true" : "false", kKeyword);
        else if (v.IsNull())
            Seg("null", kKeyword);
        else if (v.IsInt64())
            Seg(std::to_string(v.GetInt64()), kNumber);
        else if (v.IsUint64())
            Seg(std::to_string(v.GetUint64()), kNumber);
        else
            Seg(std::format("{}", v.GetDouble()), kNumber);
    }

    void Node(const char* key, const rapidjson::Value& v, bool comma);

    void Children(const rapidjson::Value& v) {
        if (v.IsObject()) {
            for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
                ImGuiMCP::PushID(it->name.GetString());
                Node(it->name.GetString(), it->value, std::next(it) != v.MemberEnd());
                ImGuiMCP::PopID();
            }
        } else if (v.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v.Size(); ++i) {
                ImGuiMCP::PushID(static_cast<int>(i));
                Node(nullptr, v[i], i + 1 < v.Size());
                ImGuiMCP::PopID();
            }
        }
    }

    void Node(const char* key, const rapidjson::Value& v, bool comma) {
        const bool isObj = v.IsObject();
        const bool isArr = v.IsArray();

        if ((!isObj && !isArr) || (isObj && v.MemberCount() == 0) || (isArr && v.Empty())) {
            BeginLeafLine("##leaf");
            KeyPrefix(key);
            if (isObj)
                Seg("{}", kPunct);
            else if (isArr)
                Seg("[]", kPunct);
            else
                Scalar(v);
            Comma(comma);
            return;
        }

        const char* open = isObj ? "{" : "[";
        const char* close = isObj ? "}" : "]";

        if (g_forceOpen != 0)
            ImGuiMCP::SetNextItemOpen(g_forceOpen > 0, ImGuiMCP::ImGuiCond_Always);
        else
            ImGuiMCP::SetNextItemOpen(true, ImGuiMCP::ImGuiCond_Once);

        const bool opened = ImGuiMCP::TreeNodeEx("##node", ImGuiMCP::ImGuiTreeNodeFlags_None);
        KeyPrefix(key);
        Seg(open, kPunct);

        if (!opened) {
            const auto count = isObj ? v.MemberCount() : v.Size();
            Seg(" … ", kDim);
            Seg(close, kPunct);
            Comma(comma);
            Seg(std::format("  // {} {}", count, isObj ? "keys" : "items"), kDim);
            return;
        }

        Children(v);
        ImGuiMCP::TreePop();

        BeginLeafLine("##close");
        Seg(close, kPunct);
        Comma(comma);
    }
}  // namespace

void UI::JsonView::Render(const rapidjson::Value& root) {
    if (ImGuiMCP::Button(UI::Translations::Get("obody_expand_all").c_str())) g_forceOpen = 1;
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button(UI::Translations::Get("obody_collapse_all").c_str())) g_forceOpen = -1;
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button(UI::Translations::Get("obody_copy_json").c_str())) {
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
        writer.SetIndent(' ', 2);
        root.Accept(writer);
        ImGuiMCP::SetClipboardText(sb.GetString());
    }
    ImGuiMCP::Separator();

    if (ImGuiMCP::BeginChild("##ConfigViewerScroll")) {
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_IndentSpacing, 18.0f);

        const bool isObj = root.IsObject();
        const bool isArr = root.IsArray();

        if ((isObj && root.MemberCount() > 0) || (isArr && !root.Empty())) {
            // Root is a fixed and non-collapsible
            BeginLeafLine("##rootOpen");
            Seg(isObj ? "{" : "[", kPunct);

            ImGuiMCP::Indent();
            Children(root);
            ImGuiMCP::Unindent();

            BeginLeafLine("##rootClose");
            Seg(isObj ? "}" : "]", kPunct);
        } else {
            Node(nullptr, root, false);
        }

        ImGuiMCP::PopStyleVar();
    }
    ImGuiMCP::EndChild();

    g_forceOpen = 0;
}
