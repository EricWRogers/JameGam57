#include <Environment/VoxelTerrainChunk.hpp>

#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/InputManager.hpp>

#include <array>
#include <cmath>
#include <cstdlib>

namespace
{
    constexpr int kTerrainMaterialSlotCount = 4;

    int GetMaterialSlotIndex(TerrainBlockType _blockType)
    {
        switch (_blockType)
        {
        case TerrainBlockType::Rock: return 0;
        case TerrainBlockType::Bedrock: return 0;
        case TerrainBlockType::Ice: return 1;
        case TerrainBlockType::Gold: return 2;
        case TerrainBlockType::Uranium: return 3;
        case TerrainBlockType::Air:
        default:
            break;
        }

        return -1;
    }
}

ScriptConf voxelTerrainChunkConf = {};

void RegisterVoxelTerrainChunkScript(Canis::App& _app)
{
    DEFAULT_CONFIG(voxelTerrainChunkConf, VoxelTerrainChunk);

    voxelTerrainChunkConf.DEFAULT_DRAW_INSPECTOR(VoxelTerrainChunk,
        ImGui::Text("size: %d x %d x %d", component->sizeX, component->sizeY, component->sizeZ);
        ImGui::Text("blocks: %zu", component->blocks.size());
        ImGui::Text("runtimeModelId: %d", component->runtimeModelId);
    );

    _app.RegisterScript(voxelTerrainChunkConf);
}

DEFAULT_UNREGISTER_SCRIPT(voxelTerrainChunkConf, VoxelTerrainChunk)

void VoxelTerrainChunk::Create()
{
    if (blocks.empty())
        Resize(sizeX, sizeY, sizeZ);
}

void VoxelTerrainChunk::Ready() {}

void VoxelTerrainChunk::Destroy()
{
    if (runtimeModelId >= 0)
    {
        Canis::AssetManager::FreeModel(runtimeModelId);
        runtimeModelId = -1;
    }
}

void VoxelTerrainChunk::Update(float _dt)
{
    (void)_dt;
}

void VoxelTerrainChunk::Resize(int _sizeX, int _sizeY, int _sizeZ)
{
    sizeX = std::max(1, _sizeX);
    sizeY = std::max(1, _sizeY);
    sizeZ = std::max(1, _sizeZ);
    blocks.assign(static_cast<size_t>(sizeX * sizeY * sizeZ), static_cast<unsigned char>(TerrainBlockType::Air));
}

void VoxelTerrainChunk::SetBlock(int _x, int _y, int _z, TerrainBlockType _type)
{
    if (!IsInBounds(_x, _y, _z))
        return;

    blocks[static_cast<size_t>(GetIndex(_x, _y, _z))] = static_cast<unsigned char>(_type);
}

TerrainBlockType VoxelTerrainChunk::GetBlock(int _x, int _y, int _z) const
{
    if (!IsInBounds(_x, _y, _z))
        return TerrainBlockType::Air;

    return static_cast<TerrainBlockType>(blocks[static_cast<size_t>(GetIndex(_x, _y, _z))]);
}

bool VoxelTerrainChunk::RebuildMesh()
{
    if (runtimeModelId < 0)
        runtimeModelId = Canis::AssetManager::CreateModel();

    Canis::ModelAsset *modelAsset = Canis::AssetManager::GetModel(runtimeModelId);
    if (modelAsset == nullptr)
        return false;

    Canis::Model &model = entity.GetComponent<Canis::Model>();
    model.modelId = runtimeModelId;

    Canis::Material &material = entity.GetComponent<Canis::Material>();
    if (material.materialIds.size() != kTerrainMaterialSlotCount)
        material.materialIds.resize(kTerrainMaterialSlotCount, -1);
    material.materialIds[0] = rockMaterialId;
    material.materialIds[1] = iceMaterialId;
    material.materialIds[2] = goldMaterialId;
    material.materialIds[3] = uraniumMaterialId;

    if (entity.HasComponent<Canis::MeshCollider>())
        entity.GetComponent<Canis::MeshCollider>().modelId = runtimeModelId;

    std::array<std::vector<Canis::ModelAsset::RenderVertex3D>, kTerrainMaterialSlotCount> verticesBySlot = {};
    std::array<std::vector<unsigned int>, kTerrainMaterialSlotCount> indicesBySlot = {};

    auto appendQuad = [&](int _materialSlot, const Canis::Vector3 &_normal, const Canis::Vector3 &_v0, const Canis::Vector3 &_v1, const Canis::Vector3 &_v2, const Canis::Vector3 &_v3)
    {
        if (_materialSlot < 0 || _materialSlot >= kTerrainMaterialSlotCount)
            return;

        std::vector<Canis::ModelAsset::RenderVertex3D> &vertices = verticesBySlot[static_cast<size_t>(_materialSlot)];
        std::vector<unsigned int> &indices = indicesBySlot[static_cast<size_t>(_materialSlot)];
        const unsigned int startIndex = static_cast<unsigned int>(vertices.size());

        vertices.push_back({.position = _v0, .normal = _normal, .uv = Canis::Vector2(0.0f, 0.0f)});
        vertices.push_back({.position = _v1, .normal = _normal, .uv = Canis::Vector2(0.0f, 1.0f)});
        vertices.push_back({.position = _v2, .normal = _normal, .uv = Canis::Vector2(1.0f, 1.0f)});
        vertices.push_back({.position = _v3, .normal = _normal, .uv = Canis::Vector2(1.0f, 0.0f)});

        indices.push_back(startIndex + 0u);
        indices.push_back(startIndex + 1u);
        indices.push_back(startIndex + 2u);
        indices.push_back(startIndex + 0u);
        indices.push_back(startIndex + 2u);
        indices.push_back(startIndex + 3u);
    };

    for (int z = 0; z < sizeZ; ++z)
    {
        for (int y = 0; y < sizeY; ++y)
        {
            for (int x = 0; x < sizeX; ++x)
            {
                const TerrainBlockType blockType = GetBlock(x, y, z);
                if (blockType == TerrainBlockType::Air)
                    continue;

                const int materialSlot = GetMaterialSlotIndex(blockType);
                const Canis::Vector3 minCorner(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                const Canis::Vector3 maxCorner = minCorner + Canis::Vector3(1.0f);

                if (GetBlock(x + 1, y, z) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(1.0f, 0.0f, 0.0f),
                        Canis::Vector3(maxCorner.x, minCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(maxCorner.x, minCorner.y, maxCorner.z));
                }

                if (GetBlock(x - 1, y, z) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(-1.0f, 0.0f, 0.0f),
                        Canis::Vector3(minCorner.x, minCorner.y, maxCorner.z),
                        Canis::Vector3(minCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(minCorner.x, maxCorner.y, minCorner.z),
                        Canis::Vector3(minCorner.x, minCorner.y, minCorner.z));
                }

                if (GetBlock(x, y + 1, z) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(0.0f, 1.0f, 0.0f),
                        Canis::Vector3(minCorner.x, maxCorner.y, minCorner.z),
                        Canis::Vector3(minCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, minCorner.z));
                }

                if (GetBlock(x, y - 1, z) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(0.0f, -1.0f, 0.0f),
                        Canis::Vector3(minCorner.x, minCorner.y, maxCorner.z),
                        Canis::Vector3(minCorner.x, minCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, minCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, minCorner.y, maxCorner.z));
                }

                if (GetBlock(x, y, z + 1) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(0.0f, 0.0f, 1.0f),
                        Canis::Vector3(maxCorner.x, minCorner.y, maxCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(minCorner.x, maxCorner.y, maxCorner.z),
                        Canis::Vector3(minCorner.x, minCorner.y, maxCorner.z));
                }

                if (GetBlock(x, y, z - 1) == TerrainBlockType::Air)
                {
                    appendQuad(materialSlot, Canis::Vector3(0.0f, 0.0f, -1.0f),
                        Canis::Vector3(minCorner.x, minCorner.y, minCorner.z),
                        Canis::Vector3(minCorner.x, maxCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, maxCorner.y, minCorner.z),
                        Canis::Vector3(maxCorner.x, minCorner.y, minCorner.z));
                }
            }
        }
    }

    std::vector<Canis::ModelAsset::PrimitiveBuild3D> primitives = {};
    primitives.reserve(kTerrainMaterialSlotCount);
    for (int materialSlot = 0; materialSlot < kTerrainMaterialSlotCount; ++materialSlot)
    {
        if (verticesBySlot[static_cast<size_t>(materialSlot)].empty() || indicesBySlot[static_cast<size_t>(materialSlot)].empty())
            continue;

        Canis::ModelAsset::PrimitiveBuild3D primitive = {};
        primitive.vertices = std::move(verticesBySlot[static_cast<size_t>(materialSlot)]);
        primitive.indices = std::move(indicesBySlot[static_cast<size_t>(materialSlot)]);
        primitive.materialSlot = materialSlot;
        primitives.push_back(std::move(primitive));
    }

    return modelAsset->SetRuntimePrimitives(primitives, {"Rock", "Ice", "Gold", "Uranium"});
}

std::string VoxelTerrainChunk::GetMessage(const InteractionContext &_context)
{
    glm::ivec3 blockCoords = glm::ivec3(0);
    TerrainBlockType blockType = TerrainBlockType::Air;
    if (!ResolveTargetBlock(_context, blockCoords, blockType))
        return "";

    if (blockType == TerrainBlockType::Bedrock)
        return "Bedrock (Unbreakable)";

    return std::string("Left Click to Break ") + GetBlockName(blockType);
}

bool VoxelTerrainChunk::HandleInteraction(const InteractionContext &_context)
{
    InputManager& input = entity.scene.GetInputManager();
    if (!input.LeftClickReleased())
        return false;

    glm::ivec3 blockCoords = glm::ivec3(0);
    TerrainBlockType blockType = TerrainBlockType::Air;
    if (!ResolveTargetBlock(_context, blockCoords, blockType))
        return false;

    if (blockType == TerrainBlockType::Bedrock)
        return false;

    SetBlock(blockCoords.x, blockCoords.y, blockCoords.z, TerrainBlockType::Air);
    SpawnDrop(blockType, blockCoords);
    return RebuildMesh();
}

int VoxelTerrainChunk::GetIndex(int _x, int _y, int _z) const
{
    return (_y * sizeZ + _z) * sizeX + _x;
}

bool VoxelTerrainChunk::IsInBounds(int _x, int _y, int _z) const
{
    return _x >= 0 && _x < sizeX &&
        _y >= 0 && _y < sizeY &&
        _z >= 0 && _z < sizeZ;
}

bool VoxelTerrainChunk::ResolveTargetBlock(const InteractionContext &_context, glm::ivec3 &_blockCoords, TerrainBlockType &_blockType) const
{
    _blockCoords = glm::ivec3(0);
    _blockType = TerrainBlockType::Air;

    if (_context.hit == nullptr || _context.hit->entity != &entity)
        return false;

    Canis::Vector3 localPoint = _context.hit->point - (_context.hit->normal * 0.01f);
    if (entity.HasComponent<Canis::Transform>())
    {
        const Canis::Matrix4 inverseModel = glm::inverse(entity.GetComponent<Canis::Transform>().GetModelMatrix());
        const Canis::Vector4 localPoint4 = inverseModel * Canis::Vector4(localPoint, 1.0f);
        localPoint = Canis::Vector3(localPoint4.x, localPoint4.y, localPoint4.z);
    }

    const int x = static_cast<int>(std::floor(localPoint.x));
    const int y = static_cast<int>(std::floor(localPoint.y));
    const int z = static_cast<int>(std::floor(localPoint.z));
    if (!IsInBounds(x, y, z))
        return false;

    const TerrainBlockType blockType = GetBlock(x, y, z);
    if (blockType == TerrainBlockType::Air)
        return false;

    _blockCoords = glm::ivec3(x, y, z);
    _blockType = blockType;
    return true;
}

std::string VoxelTerrainChunk::GetBlockName(TerrainBlockType _blockType) const
{
    switch (_blockType)
    {
    case TerrainBlockType::Rock: return "Rock Block";
    case TerrainBlockType::Bedrock: return "Bedrock";
    case TerrainBlockType::Ice: return "Ice Block";
    case TerrainBlockType::Gold: return "Gold Block";
    case TerrainBlockType::Uranium: return "Uranium Block";
    case TerrainBlockType::Air:
    default:
        break;
    }

    return "Block";
}

Canis::SceneAssetHandle VoxelTerrainChunk::GetDropPrefab(TerrainBlockType _blockType) const
{
    switch (_blockType)
    {
    case TerrainBlockType::Rock: return rockDropPrefab;
    case TerrainBlockType::Ice: return iceDropPrefab;
    case TerrainBlockType::Gold: return goldDropPrefab;
    case TerrainBlockType::Uranium: return uraniumDropPrefab;
    case TerrainBlockType::Bedrock:
    case TerrainBlockType::Air:
    default:
        break;
    }

    return {};
}

void VoxelTerrainChunk::SpawnDrop(TerrainBlockType _blockType, const glm::ivec3 &_blockCoords)
{
    const Canis::SceneAssetHandle dropPrefab = GetDropPrefab(_blockType);
    if (dropPrefab.path.empty())
        return;

    Canis::Vector3 worldPosition(
        static_cast<float>(_blockCoords.x) + 0.5f,
        static_cast<float>(_blockCoords.y) + 0.55f,
        static_cast<float>(_blockCoords.z) + 0.5f);

    if (entity.HasComponent<Canis::Transform>())
    {
        const Canis::Vector4 worldPosition4 = entity.GetComponent<Canis::Transform>().GetModelMatrix() * Canis::Vector4(worldPosition, 1.0f);
        worldPosition = Canis::Vector3(worldPosition4.x, worldPosition4.y, worldPosition4.z);
    }

    for (Canis::Entity *root : entity.scene.Instantiate(dropPrefab))
    {
        if (root == nullptr || !root->HasComponent<Canis::Transform>())
            continue;

        Canis::Transform &transform = root->GetComponent<Canis::Transform>();
        Canis::Vector3 randomOffset = Canis::Vector3(
            (static_cast<float>(rand() % 100) * 0.0025f) - 0.125f,
            0.0f,
            (static_cast<float>(rand() % 100) * 0.0025f) - 0.125f);
        transform.position += worldPosition + randomOffset;
    }
}
