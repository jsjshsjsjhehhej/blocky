
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cstdio>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <functional>

static void collectNodeNames(const aiNode* node, std::set<std::string>& names, int depth) {
    for (int i = 0; i < depth; ++i) std::printf("  ");
    std::printf("- %s\n", node->mName.C_Str());
    names.insert(node->mName.C_Str());
    for (unsigned i = 0; i < node->mNumChildren; ++i)
        collectNodeNames(node->mChildren[i], names, depth + 1);
}

static bool isIdentity(const aiMatrix4x4& m) {
    aiMatrix4x4 id;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (fabs(m[i][j] - id[i][j]) > 1e-5) return false;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("Uso: inspect <file>\n"); return 1; }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_LimitBoneWeights);
    if (!scene || !scene->mRootNode) {
        std::printf("ERRORE: %s\n", importer.GetErrorString());
        return 1;
    }

    std::printf("=== ROOT: '%s' (transform %s) ===\n",
                scene->mRootNode->mName.C_Str(),
                isIdentity(scene->mRootNode->mTransformation) ? "IDENTITA'" : "NON identita'");

    std::printf("\n--- GERARCHIA NODI ---\n");
    std::set<std::string> nodeNames;
    collectNodeNames(scene->mRootNode, nodeNames, 0);

    std::printf("\n--- MESH e OSSA ---\n");
    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        std::printf("mesh[%u] '%s' (%u ossa):\n", m, mesh->mName.C_Str(), mesh->mNumBones);
        for (unsigned b = 0; b < mesh->mNumBones; ++b) {
            const char* bn = mesh->mBones[b]->mName.C_Str();
            bool inNodes = nodeNames.count(bn) > 0;
            std::printf("    osso '%s' -> nodo %s\n", bn, inNodes ? "TROVATO" : "*** MANCANTE ***");
        }
    }

    std::printf("\n--- CANALI ANIMAZIONE ---\n");
    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* an = scene->mAnimations[a];
        std::printf("anim[%u] '%s':\n", a, an->mName.C_Str());
        for (unsigned c = 0; c < an->mNumChannels; ++c) {
            const char* cn = an->mChannels[c]->mNodeName.C_Str();
            bool inNodes = nodeNames.count(cn) > 0;
            std::printf("    canale '%s' -> nodo %s\n", cn, inNodes ? "ok" : "*** MANCANTE ***");
        }
    }

    std::printf("\n--- TEST BIND POSE: centro di ogni mesh dopo lo skinning ---\n");
    std::map<std::string, aiMatrix4x4> globalBind;
    std::function<void(const aiNode*, aiMatrix4x4)> build =
        [&](const aiNode* node, aiMatrix4x4 parent) {
            aiMatrix4x4 g = parent * node->mTransformation;
            globalBind[node->mName.C_Str()] = g;
            for (unsigned i = 0; i < node->mNumChildren; ++i)
                build(node->mChildren[i], g);
        };
    build(scene->mRootNode, aiMatrix4x4());

    const float BIG = 1e9f;
    aiVector3D rawMin(BIG, BIG, BIG), rawMax(-BIG, -BIG, -BIG);
    aiVector3D skMin(BIG, BIG, BIG),  skMax(-BIG, -BIG, -BIG);

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        if (mesh->mNumBones == 0) continue;
        std::vector<aiVector3D> sumPos(mesh->mNumVertices, aiVector3D(0, 0, 0));
        std::vector<float> sumW(mesh->mNumVertices, 0.0f);
        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D& v = mesh->mVertices[i];
            rawMin.x = std::min(rawMin.x, v.x); rawMin.y = std::min(rawMin.y, v.y); rawMin.z = std::min(rawMin.z, v.z);
            rawMax.x = std::max(rawMax.x, v.x); rawMax.y = std::max(rawMax.y, v.y); rawMax.z = std::max(rawMax.z, v.z);
        }
        for (unsigned b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            aiMatrix4x4 finalM = globalBind[bone->mName.C_Str()] * bone->mOffsetMatrix;
            for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                unsigned vid = bone->mWeights[w].mVertexId;
                float wt = bone->mWeights[w].mWeight;
                sumPos[vid] += (finalM * mesh->mVertices[vid]) * wt;
                sumW[vid] += wt;
            }
        }
        aiVector3D c(0, 0, 0); int n = 0;
        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
            if (sumW[i] > 0.0f) {
                aiVector3D p = sumPos[i] * (1.0f / sumW[i]);
                c += p; ++n;
                skMin.x = std::min(skMin.x, p.x); skMin.y = std::min(skMin.y, p.y); skMin.z = std::min(skMin.z, p.z);
                skMax.x = std::max(skMax.x, p.x); skMax.y = std::max(skMax.y, p.y); skMax.z = std::max(skMax.z, p.z);
            }
        if (n > 0) c *= (1.0f / n);
        std::printf("  mesh[%u] '%s' centro = (%.2f, %.2f, %.2f)\n",
                    m, mesh->mName.C_Str(), c.x, c.y, c.z);
    }

    std::printf("\nBBOX vertici GREZZI : min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)\n",
                rawMin.x, rawMin.y, rawMin.z, rawMax.x, rawMax.y, rawMax.z);
    std::printf("BBOX dopo SKINNING  : min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)\n",
                skMin.x, skMin.y, skMin.z, skMax.x, skMax.y, skMax.z);

    for (unsigned m = 1; m <= 1 && m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const aiBone* bone = mesh->mBones[0];
        aiMatrix4x4 M = globalBind[bone->mName.C_Str()] * bone->mOffsetMatrix;
        std::printf("\nglobalBind*offset per mesh[%u] osso '%s' (atteso ~ identita'):\n",
                    m, bone->mName.C_Str());
        std::printf("  [%.3f %.3f %.3f %.3f]\n", M.a1, M.a2, M.a3, M.a4);
        std::printf("  [%.3f %.3f %.3f %.3f]\n", M.b1, M.b2, M.b3, M.b4);
        std::printf("  [%.3f %.3f %.3f %.3f]\n", M.c1, M.c2, M.c3, M.c4);
        std::printf("  [%.3f %.3f %.3f %.3f]\n", M.d1, M.d2, M.d3, M.d4);
    }

    return 0;
}
