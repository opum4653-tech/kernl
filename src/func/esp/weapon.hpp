#pragma once
#include "common.hpp"

namespace func::esp {

    // Resolves accurate Free Fire weapon names by ID (with category fallback)
    inline const char* get_weapon_name(int wp) {
        switch (wp) {
            case 1:     return "FIST";
            case 2:     return "M4A1";
            case 80:    return "M4A1-I";
            case 81:    return "M4A1-II";
            case 82:    return "M4A1-III";
            case 12:    return "SCAR";
            case 178:   return "SCAR-I";
            case 179:   return "SCAR-II";
            case 180:   return "SCAR-III";
            case 14:    return "GROZA";
            case 70:    return "GROZA-X";
            case 24:    return "FAMAS";
            case 67:    return "FAMAS-I";
            case 130:   return "FAMAS-II";
            case 131:   return "FAMAS-III";
            case 39:    return "PLASMA";
            case 46:    return "AUG";
            case 11:    return "M14";
            case 63:    return "M14-I";
            case 126:   return "M14-II";
            case 127:   return "M14-III";
            case 19:    return "M249";
            case 71:    return "M249-X";
            case 30:    return "M60";
            case 61:    return "M60-I";
            case 122:   return "M60-II";
            case 123:   return "M60-III";
            case 54:    return "KORD";
            case 13:    return "VSS";
            case 93:    return "HEAL PISTOL";
            case 21001: return "HEAL PISTOL-Y";
            case 6:     return "AK-47";
            case 28:    return "XM8";
            case 33:    return "AN94";
            case 47:    return "PARAFAL";
            case 57:    return "KINGFISHER";
            case 73:    return "G36";
            case 99:    return "SHIELD GUN";
            case 18:    return "SKS";
            case 26:    return "SVD";
            case 72:    return "SVD-Y";
            case 48:    return "WOODPECKER";
            case 89:    return "AC80";
            case 7:     return "UMP";
            case 8:     return "MP5";
            case 60:    return "MP5-I";
            case 120:   return "MP5-II";
            case 121:   return "MP5-III";
            case 15:    return "MP40";
            case 32:    return "P90";
            case 35:    return "CG15";
            case 43:    return "THOMPSON";
            case 49:    return "VECTOR";
            case 88:    return "MAC10";
            case 150:   return "BIZON";
            case 5:     return "M1014";
            case 184:   return "M1014-I";
            case 185:   return "M1014-II";
            case 186:   return "M1014-III";
            case 29:    return "SPAS12";
            case 41:    return "M1887";
            case 119:   return "M1887-X";
            case 50:    return "MAG-7";
            case 86:    return "CHARGE BUSTER";
            case 181:   return "TROGON";
            case 21002: return "M590";
            case 4:     return "AWM";
            case 65:    return "AWM-Y";
            case 30060: return "AWM-INF";
            case 21:    return "KAR98K";
            case 64:    return "KAR98K-I";
            case 128:   return "KAR98K-II";
            case 129:   return "KAR98K-III";
            case 45:    return "M82B";
            case 75:    return "M24";
            case 197:   return "VSK94";
            case 78:    return "HEAL SNIPER";
            case 3:     return "USP";
            case 9:     return "DESERT EAGLE";
            case 10:    return "G18";
            case 20:    return "M1873";
            case 25:    return "M500";
            case 55:    return "M1917";
            case 56:    return "USP-2";
            case 58:    return "MINI UZI";
            case 100:   return "FGL-24";
            case 16:    return "PAN";
            case 17:    return "PARANG";
            case 27:    return "BAT";
            case 34:    return "KATANA";
            case 51:    return "SCYTHE";
            case 1201:  return "GLOO WALL";
            case 617:   return "GLOO MELTER";
            case 601:   return "GRENADE";
            default:    break;
        }

        // Fallback categories if ID is index 0-10
        static const char* kCategories[] = {
            "AR","SNIPER","SMG","SHOTGUN","PISTOL","MELEE",
            "GRENADE","LMG","RIFLE","MARKSMAN","SPECIAL"
        };
        if (wp >= 0 && wp < (int)(sizeof(kCategories)/sizeof(*kCategories))) {
            return kCategories[wp];
        }
        return "WPN";
    }

    // WEAPON PILL (RENDERED BELOW PLAYER FEET)
    inline void drawWeapon(ImDrawList* dl, const PlayerData& p, ImVec2 foot) {
        if (!cfg::esp::weapon || p.weaponType < 0) return;

        const char* wn = get_weapon_name(p.weaponType);

        const float fs   = ESP_FONT_SIZE - 2.f;
        const float padX = 6.f;
        const float padY = 3.f;

        ImVec2 ts = ImGui::CalcTextSize(wn);
        float pw = ts.x + padX * 2.f;
        float ph = fs + padY * 2.f;
        float px = foot.x - pw * 0.5f;
        float py = foot.y + 4.f;

        ImU32 wCol = toU32(cfg::esp::weapon_col);
        ImU32 bg   = p.isVisible ? IM_COL32(0, 0, 0, 210) : IM_COL32(0, 0, 0, 150);

        dl->AddRectFilled({px + 1, py + 1}, {px + pw + 1, py + ph + 1}, C_SHADOW, 5.f);
        dl->AddRectFilled({px, py}, {px + pw, py + ph}, bg, 5.f);
        dl->AddRect({px, py}, {px + pw, py + ph}, C_BORDER, 5.f, 0, 1.f);
        dl->AddText(ImGui::GetFont(), fs, {px + padX, py + padY}, wCol, wn);
    }

} // namespace func::esp
