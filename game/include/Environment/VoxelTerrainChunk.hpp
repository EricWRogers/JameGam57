#pragma once

#include <I_Interactable.hpp>

#include <Canis/AssetHandle.hpp>
#include <Canis/Entity.hpp>

#include <vector>

enum class TerrainBlockType : unsigned char
{
    Air = 0,
    Rock = 1,
    Ice = 2,
    Gold = 3,
    Uranium = 4,
    Bedrock = 5,
};

class VoxelTerrainChunk : public Canis::ScriptableEntity, public I_Interactable
{
public:
    static constexpr const char* ScriptName = "VoxelTerrainChunk";

    explicit VoxelTerrainChunk(Canis::Entity &_entity) : Canis::ScriptableEntity(_entity) {}

    int sizeX = 16;
    int sizeY = 24;
    int sizeZ = 16;
    int runtimeModelId = -1;

    int rockMaterialId = -1;
    int iceMaterialId = -1;
    int goldMaterialId = -1;
    int uraniumMaterialId = -1;

    Canis::SceneAssetHandle rockDropPrefab = {};
    Canis::SceneAssetHandle iceDropPrefab = {};
    Canis::SceneAssetHandle goldDropPrefab = {};
    Canis::SceneAssetHandle uraniumDropPrefab = {};

    std::vector<unsigned char> blocks = {};

    void Create();
    void Ready();
    void Destroy();
    void Update(float _dt);

    void Resize(int _sizeX, int _sizeY, int _sizeZ);
    void SetBlock(int _x, int _y, int _z, TerrainBlockType _type);
    TerrainBlockType GetBlock(int _x, int _y, int _z) const;
    bool RebuildMesh();

    std::string GetMessage(const InteractionContext &_context) override;
    bool HandleInteraction(const InteractionContext &_context) override;

private:
    int GetIndex(int _x, int _y, int _z) const;
    bool IsInBounds(int _x, int _y, int _z) const;
    bool ResolveTargetBlock(const InteractionContext &_context, glm::ivec3 &_blockCoords, TerrainBlockType &_blockType) const;
    std::string GetBlockName(TerrainBlockType _blockType) const;
    Canis::SceneAssetHandle GetDropPrefab(TerrainBlockType _blockType) const;
    void SpawnDrop(TerrainBlockType _blockType, const glm::ivec3 &_blockCoords);
};

extern void RegisterVoxelTerrainChunkScript(Canis::App& _app);
extern void UnRegisterVoxelTerrainChunkScript(Canis::App& _app);
