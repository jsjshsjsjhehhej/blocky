#pragma once

/**
 * Palette BrickColor stile Roblox Studio — nomi + RGB (0–255) per le Part.
 */

#include <cstring>
#include <glm/glm.hpp>

namespace brickcolor {

struct Entry {
    int number;
    const char* name;
    unsigned char r, g, b;
};

inline const Entry* palette(int* outCount = nullptr) {
    // Numeri/nomi allineati alla palette BrickColor classica + colori Studio comuni.
    static const Entry k[] = {
        {1, "White", 242, 243, 243},
        {2, "Grey", 161, 165, 162},
        {3, "Light blue", 180, 210, 228},
        {5, "Brick yellow", 215, 197, 154},
        {6, "Light green", 194, 218, 184},
        {9, "Light reddish violet", 232, 186, 200},
        {11, "Pastel Blue", 128, 187, 219},
        {18, "Nougat", 204, 142, 105},
        {21, "Bright red", 196, 40, 28},
        {22, "Med. reddish violet", 196, 112, 160},
        {23, "Bright blue", 13, 105, 172},
        {24, "Bright yellow", 245, 205, 48},
        {25, "Earth orange", 98, 71, 50},
        {26, "Black", 27, 42, 53},
        {28, "Dark green", 40, 127, 71},
        {29, "Medium green", 161, 196, 140},
        {37, "Bright green", 75, 151, 75},
        {38, "Dark orange", 160, 95, 53},
        {39, "Light bluish violet", 159, 173, 192},
        {45, "Light blue", 180, 210, 228},
        {101, "Medium red", 218, 134, 122},
        {102, "Medium blue", 110, 153, 202},
        {104, "Bright violet", 107, 50, 124},
        {105, "Br. yellowish orange", 226, 155, 64},
        {106, "Bright orange", 218, 133, 65},
        {107, "Bright bluish green", 0, 143, 156},
        {111, "Sand blue", 116, 134, 157},
        {112, "Sand violet", 135, 124, 144},
        {119, "Light orange", 234, 184, 146},
        {121, "Medium orange", 224, 178, 128},
        {123, "Br. yellowish green", 164, 189, 71},
        {124, "Bright reddish violet", 146, 57, 120},
        {125, "Light purple", 228, 173, 200},
        {127, "Gold", 220, 188, 129},
        {128, "Dark nougat", 174, 122, 89},
        {131, "Silver", 156, 163, 168},
        {133, "Neon orange", 213, 115, 61},
        {134, "Neon green", 216, 221, 86},
        {135, "Sand blue", 116, 134, 157},
        {138, "Sand yellow", 149, 138, 115},
        {140, "Earth blue", 32, 58, 86},
        {141, "Earth green", 39, 70, 45},
        {148, "Dark grey metallic", 87, 88, 87},
        {149, "Black metallic", 22, 29, 33},
        {150, "Light grey metallic", 171, 173, 172},
        {151, "Sand green", 120, 144, 129},
        {153, "Sand red", 149, 121, 119},
        {154, "Dark red", 123, 46, 47},
        {191, "Really black", 17, 17, 17},
        {192, "Really red", 255, 0, 0},
        {193, "Deep orange", 255, 176, 0},
        {194, "Lime green", 0, 255, 0},
        {195, "New Yeller", 255, 255, 0},
        {196, "Really blue", 0, 0, 255},
        {198, "Hot pink", 255, 0, 191},
        {199, "Black", 27, 42, 53},
        {208, "Light grey", 199, 193, 183},
        {209, "Dark grey", 109, 110, 108},
        {211, "Light yellow", 249, 233, 153},
        {212, "Light blue", 159, 195, 233},
        {213, "Light red", 238, 196, 182},
        {216, "Rust", 143, 76, 42},
        {217, "Brown", 124, 92, 70},
        {218, "Reddish brown", 105, 64, 40},
        {219, "Quill grey", 223, 223, 222},
        {220, "Olive", 121, 133, 73},
        {221, "Pastel Blue", 128, 187, 219},
        {222, "Pastel green", 204, 255, 204},
        {223, "Pastel violet", 177, 167, 255},
        {224, "Pastel light blue", 175, 221, 255},
        {225, "Pastel orange", 255, 201, 201},
        {226, "Pastel brown", 255, 204, 153},
        {232, "Carnation pink", 255, 152, 220},
        {268, "Medium lilac", 52, 43, 117},
        {301, "Slime green", 80, 109, 84},
        {302, "Smoky grey", 91, 93, 105},
        {303, "Teal", 18, 238, 212},
        {304, "Toothpaste", 0, 255, 255},
        {305, "Electric blue", 9, 137, 207},
        {306, "Cyan", 4, 175, 236},
        {307, "Lavender", 140, 91, 159},
        {308, "Royal purple", 98, 37, 209},
        {309, "Alder", 180, 128, 255},
        {310, "Lilac", 167, 94, 155},
        {311, "Sunrise", 212, 144, 189},
        {312, "Mulberry", 146, 57, 120},
        {313, "Fossil", 159, 161, 172},
        {314, "Cloudy grey", 171, 168, 158},
        {315, "Seashell", 233, 218, 193},
        {316, "Khaki", 175, 148, 131},
        {317, "Pine Cone", 108, 88, 75},
        {318, "Beige", 202, 191, 163},
        {319, "Cashmere", 211, 190, 150},
        {320, "Wheat", 241, 231, 199},
        {321, "Buttermilk", 254, 243, 187},
        {322, "Institutional white", 248, 248, 248},
        {323, "Mid gray", 163, 162, 165},
        {324, "Ghost grey", 202, 203, 209},
        {325, "Shamrock", 91, 154, 76},
        {326, "Moss", 124, 156, 107},
        {327, "Artichoke", 138, 171, 133},
        {328, "Sage green", 185, 196, 177},
        {329, "Olivine", 148, 190, 129},
        {330, "Laurel green", 168, 189, 153},
        {331, "Grime", 127, 142, 100},
        {332, "Crimson", 151, 0, 0},
        {333, "Maroon", 117, 0, 0},
        {334, "Persimmon", 255, 89, 89},
        {335, "Dusty Rose", 163, 75, 75},
        {336, "Daisy orange", 248, 217, 109},
        {337, "Burgundy", 136, 62, 62},
        {338, "Mauve", 224, 178, 208},
        {339, "Tawny", 150, 85, 85},
        {340, "Terra Cotta", 190, 104, 98},
        {341, "Flame reddish orange", 207, 96, 36},
        {342, "Hurricane grey", 149, 137, 136},
        {343, "Smoke", 145, 148, 136},
        {344, "Cork", 188, 155, 93},
        {345, "Burlap", 199, 172, 120},
        {346, "Storm blue", 51, 88, 130},
        {347, "Lapis", 16, 42, 220},
        {348, "Dark indigo", 61, 21, 133},
        {349, "Sea green", 52, 142, 64},
        {350, "Forest green", 31, 128, 29},
        {351, "Camo", 58, 125, 21},
        {352, "Dirt brown", 86, 66, 54},
        {353, "Bronze", 126, 104, 63},
        {354, "Flint", 105, 102, 92},
        {355, "Dove blue", 125, 157, 182},
        {356, "Fog", 199, 212, 228},
        {357, "Steel blue", 82, 124, 174},
        {358, "Parsley green", 44, 101, 29},
        {359, "Mint", 177, 229, 166},
        {360, "Pearl", 231, 231, 236},
        {361, "Cadet blue", 159, 173, 192},
        {362, "Deep blue", 33, 84, 185},
        {363, "Navy blue", 0, 32, 96},
        {364, "Magenta", 170, 0, 170},
        {365, "Pink", 255, 102, 204},
        {366, "Plum", 123, 47, 123},
        {367, "Purple", 124, 57, 160},
        {368, "Cool yellow", 253, 234, 141},
        {369, "Medium stone grey", 163, 162, 165},
        {370, "Dark stone grey", 99, 95, 98},
        {371, "Light stone grey", 229, 228, 223},
        {372, "Oyster", 187, 179, 178},
        {373, "Dark taupe", 90, 76, 66},
        {374, "Olive green", 193, 190, 66},
        {375, "Br. yellowish green", 164, 189, 71},
        {376, "Sand yellow", 149, 138, 115},
        {377, "Sand yellow metallic", 147, 135, 103},
        {378, "Sand blue metallic", 121, 136, 161},
        {379, "Sand violet metallic", 149, 142, 163},
        {380, "Gun metallic", 117, 108, 98},
        {381, "Lemon metalic", 130, 138, 93},
        {382, "Flu. Green", 120, 186, 59},
        {383, "Tr. Red", 205, 84, 75},
        {384, "Tr. Blue", 123, 182, 232},
        {385, "Tr. Yellow", 247, 241, 141},
        {386, "Tr. Green", 132, 182, 141},
        {387, "Tr. Flu. Blue", 207, 226, 247},
        {388, "Tr. Flu. Yellow", 255, 246, 123},
        {389, "Tr. Flu. Green", 248, 241, 132},
        {390, "Tr. Brown", 191, 183, 177},
        {391, "Tr. Lg blue", 193, 223, 240},
        {392, "Phosph. White", 236, 225, 188},
        {393, "Lig. Yellowich orange", 243, 207, 155},
        {394, "Light orange brown", 203, 132, 66},
        {395, "Medium reddish violet", 183, 85, 154},
        {396, "Earth yellow", 104, 92, 67},
        {397, "Reddish brown metallic", 87, 60, 45},
        {398, "Rusty orange", 213, 115, 61},
        {399, "Smoky grey", 91, 93, 105},
        {1001, "Cork", 188, 155, 93},
        {1002, "Burlap", 199, 172, 120},
        {1003, "Gold", 239, 184, 56},
        {1004, "Silver", 206, 212, 218},
        {1005, "Copper", 150, 103, 65},
        {1006, "Bronze", 170, 120, 55},
        {1007, "Ivory", 255, 250, 240},
        {1008, "Cream", 255, 253, 208},
        {1009, "Sand", 217, 192, 140},
        {1010, "Clay", 168, 119, 91},
        {1011, "Charcoal", 54, 57, 61},
        {1012, "Ash", 120, 120, 120},
        {1013, "Snow", 255, 255, 255},
        {1014, "Ice", 200, 230, 255},
        {1015, "Sky", 135, 206, 235},
        {1016, "Ocean", 0, 105, 148},
        {1017, "Sunset", 255, 94, 58},
        {1018, "Amber", 255, 191, 0},
        {1019, "Coral", 255, 127, 80},
        {1020, "Salmon", 250, 128, 114},
        {1021, "Peach", 255, 218, 185},
        {1022, "Rose", 255, 0, 127},
        {1023, "Berry", 142, 33, 89},
        {1024, "Grape", 111, 45, 168},
        {1025, "Indigo", 75, 0, 130},
        {1026, "Cobalt", 0, 71, 171},
        {1027, "Azure", 0, 127, 255},
        {1028, "Aqua", 0, 255, 255},
        {1029, "Turquoise", 64, 224, 208},
        {1030, "Emerald", 80, 200, 120},
        {1031, "Jade", 0, 168, 107},
        {1032, "Fern", 79, 121, 66},
        {1033, "Grass", 0, 154, 23},
        {1034, "Leaf", 113, 188, 120},
        {1035, "Chartreuse", 127, 255, 0},
        {1036, "Honey", 235, 178, 48},
        {1037, "Mustard", 255, 219, 88},
        {1038, "Sandstone", 191, 148, 95},
        {1039, "Concrete", 145, 142, 133},
        {1040, "Asphalt", 40, 40, 43},
        {1041, "Brass", 181, 166, 66},
        {1042, "Chrome", 201, 210, 220},
        {1043, "Gunmetal", 42, 52, 57},
        {1044, "Night", 10, 10, 20},
        {1045, "Blood", 102, 0, 0},
        {1046, "Scarlet", 255, 36, 0},
        {1047, "Tangerine", 242, 133, 0},
        {1048, "Banana", 255, 225, 53},
        {1049, "Lime", 50, 205, 50},
        {1050, "Forest", 34, 85, 34},
        {1051, "Midnight", 25, 25, 112},
        {1052, "Ultramarine", 18, 10, 143},
        {1053, "Violet", 143, 0, 255},
        {1054, "Fuchsia", 255, 0, 255},
        {1055, "Blush", 255, 192, 203},
        {1056, "Skin", 233, 183, 148},
        {1057, "Tan", 210, 180, 140},
        {1058, "Wood", 139, 90, 43},
        {1059, "Mahogany", 103, 47, 35},
        {1060, "Walnut", 90, 56, 32},
    };
    if (outCount) *outCount = (int)(sizeof(k) / sizeof(k[0]));
    return k;
}

inline glm::vec3 toLinear(const Entry& e) {
    return {e.r / 255.f, e.g / 255.f, e.b / 255.f};
}

inline bool nameMatch(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return *a == *b;
}

inline bool nameContains(const char* hay, const char* needle) {
    if (!hay || !needle || !needle[0]) return true;
    auto lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    };
    for (const char* h = hay; *h; ++h) {
        const char* a = h;
        const char* b = needle;
        while (*a && *b && lower(*a) == lower(*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

inline const Entry* findByNumber(int n) {
    int count = 0;
    const Entry* all = palette(&count);
    for (int i = 0; i < count; ++i)
        if (all[i].number == n) return &all[i];
    return nullptr;
}

inline const Entry* findByName(const char* name) {
    if (!name || !name[0]) return nullptr;
    int count = 0;
    const Entry* all = palette(&count);
    for (int i = 0; i < count; ++i)
        if (nameMatch(all[i].name, name)) return &all[i];
    return nullptr;
}

inline const Entry* nearest(const glm::vec3& c) {
    int count = 0;
    const Entry* all = palette(&count);
    const Entry* best = &all[0];
    float bestD = 1e9f;
    for (int i = 0; i < count; ++i) {
        glm::vec3 v = toLinear(all[i]);
        float dx = v.x - c.x, dy = v.y - c.y, dz = v.z - c.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) { bestD = d; best = &all[i]; }
    }
    return best;
}

} // namespace brickcolor
