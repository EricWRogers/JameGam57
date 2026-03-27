#pragma once

#include <Canis/Entity.hpp>

#include <cstdint>
#include <limits>
#include <unordered_map>

class VoxelTerrainChunk;

class GenerateTerrain : public Canis::ScriptableEntity
{
private:
    bool m_generated = false;
    Canis::Entity* m_playerEntity = nullptr;
    int m_lastCenterChunkX = std::numeric_limits<int>::max();
    int m_lastCenterChunkZ = std::numeric_limits<int>::max();
    int m_rockMaterialId = -1;
    int m_iceMaterialId = -1;
    int m_goldMaterialId = -1;
    int m_uraniumMaterialId = -1;
    std::unordered_map<std::uint64_t, int> m_loadedChunkEntities = {};

public:
    static constexpr const char* ScriptName = "GenerateTerrain";

    int seed = 1337;
    int chunksX = 4;
    int chunksZ = 4;
    int chunkSize = 16;
    int chunkHeight = 24;
    int baseHeight = 6;
    int maxHeightVariation = 10;
    int hillHeightBoost = 6;
    int bedrockLayerHeight = 1;
    int surfaceIceHeight = 11;
    float heightNoiseScale = 0.075f;
    float detailNoiseScale = 0.16f;
    float hillNoiseScale = 0.03f;
    float caveNoiseScale = 0.12f;

    Canis::SceneAssetHandle rockDropPrefab = {};
    Canis::SceneAssetHandle iceDropPrefab = {};
    Canis::SceneAssetHandle goldDropPrefab = {};
    Canis::SceneAssetHandle uraniumDropPrefab = {};

    GenerateTerrain(Canis::Entity &_entity) : Canis::ScriptableEntity(_entity) {}

    void Create();
    void Ready();
    void Destroy();
    void Update(float _dt);

private:
    void CacheMaterials();
    void EnsurePlayerEntity();
    Canis::Vector3 GetStreamingFocusPosition() const;
    void RefreshLoadedChunks(bool _forceRefresh);
    Canis::Entity* CreateChunkEntity(int _chunkX, int _chunkZ);
    void PopulateChunk(VoxelTerrainChunk &_chunk, int _chunkX, int _chunkZ) const;
};

extern void RegisterGenerateTerrainScript(Canis::App& _app);
extern void UnRegisterGenerateTerrainScript(Canis::App& _app);
