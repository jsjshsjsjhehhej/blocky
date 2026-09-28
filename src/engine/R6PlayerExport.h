#pragma once


#include "GameHub.h"
#include "Humanoid.h"
#include "render/BeveledCube.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <commdlg.h>
#include <shlobj.h>
#endif

inline std::string r6ObjSafeName(const std::string& n) {
    std::string o;
    for (char c : n) {
        if (c == ' ') o.push_back('_');
        else o.push_back(c);
    }
    return o;
}

inline bool exportDefaultR6PlayerObj(const std::string& objPath, std::string& err) {
    if (objPath.empty()) {
        err = "percorso vuoto";
        return false;
    }
    Humanoid h = Humanoid::createR6Blocky();
    std::vector<float> unit;
    pushBeveledCubePositions(unit, 0.11f);
    const int nVerts = (int)(unit.size() / 3);

    std::string mtlPath = objPath;
    auto dot = mtlPath.find_last_of('.');
    if (dot != std::string::npos) mtlPath = mtlPath.substr(0, dot);
    mtlPath += ".mtl";
    auto slash = mtlPath.find_last_of("\\/");
    std::string mtlFile = (slash == std::string::npos) ? mtlPath : mtlPath.substr(slash + 1);

    std::ofstream mtl(mtlPath);
    if (!mtl) {
        err = "impossibile scrivere MTL";
        return false;
    }
    for (const auto& p : h.parts) {
        if (p.name == "HumanoidRootPart") continue;
        std::string mat = r6ObjSafeName(p.name);
        mtl << "newmtl " << mat << "\n"
            << "Kd " << p.color.r << " " << p.color.g << " " << p.color.b << "\n"
            << "d 1.0\n\n";
    }
    mtl.close();

    std::ofstream obj(objPath);
    if (!obj) {
        err = "impossibile scrivere OBJ";
        return false;
    }
    obj << "# Blocky R6 Default Player (beveled body)\n"
        << "# Importa in Blender → anima → Action \"WalkingAnimation\"\n"
        << "mtllib " << mtlFile << "\n\n";

    int vBase = 1;
    for (const auto& p : h.parts) {
        if (p.name == "HumanoidRootPart") continue;
        std::string oname = r6ObjSafeName(p.name);
        obj << "o " << oname << "\n";
        obj << "usemtl " << oname << "\n";
        for (int i = 0; i < nVerts; ++i) {
            float lx = unit[i * 3 + 0] * p.size.x;
            float ly = unit[i * 3 + 1] * p.size.y;
            float lz = unit[i * 3 + 2] * p.size.z;
            float wx = p.offset.x + lx;
            float wy = p.offset.y + ly;
            float wz = p.offset.z + lz;
            obj << "v " << wx << " " << wy << " " << wz << "\n";
        }
        for (int i = 0; i < nVerts; i += 3) {
            obj << "f " << (vBase + i) << " " << (vBase + i + 1) << " " << (vBase + i + 2) << "\n";
        }
        vBase += nVerts;
        obj << "\n";
    }
    obj.close();

    std::string readme = objPath;
    if (dot != std::string::npos) readme = objPath.substr(0, objPath.find_last_of('.'));
    readme += "_LEGGIMI.txt";
    std::ofstream rd(readme);
    if (rd) {
        rd << "Blocky — Player R6 default (corpo beveled)\n"
           << "==========================================\n\n"
           << "1. Apri questo .obj in Blender (File → Import → Wavefront)\n"
           << "2. Gli object si chiamano: Head, Torso, Left_Arm, Right_Arm, Left_Leg, Right_Leg\n"
           << "   (in Blender puoi rinominarli in \"Left Arm\" ecc. se preferisci)\n"
           << "3. Crea un'Action chiamata esattamente: WalkingAnimation\n"
           << "4. Anima la rotazione di braccia/gambe (asse X = swing)\n"
           << "5. Esporta GLB o FBX CON animazioni\n"
           << "6. In Blocky Game Engine → Avatar Catalog → Importa WalkingAnimation\n"
           << "7. Rinomina e premi Rilascia Walking Animation\n";
    }
    err.clear();
    return true;
}

#ifdef _WIN32
inline bool saveR6PlayerObjDialog(std::string& outPath, void* hwndOwner = nullptr) {
    wchar_t file[MAX_PATH] = L"Blocky_R6_Default.obj";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFilter = L"Wavefront OBJ (*.obj)\0*.obj\0Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Blocky — Scarica Player R6 default";
    ofn.lpstrDefExt = L"obj";
    if (!GetSaveFileNameW(&ofn)) return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return false;
    outPath.resize((size_t)n - 1);
    WideCharToMultiByte(CP_UTF8, 0, file, -1, outPath.data(), n, nullptr, nullptr);
    return true;
}
#else
inline bool saveR6PlayerObjDialog(std::string&, void* = nullptr) { return false; }
#endif

inline bool exportDefaultR6ToLibrary(std::string& outPath, std::string& err) {
    std::string dir = blockyDataDir() + "/models";
#ifdef _WIN32
    _mkdir(dir.c_str());
#endif
    outPath = dir + "/Blocky_R6_Default.obj";
    return exportDefaultR6PlayerObj(outPath, err);
}
