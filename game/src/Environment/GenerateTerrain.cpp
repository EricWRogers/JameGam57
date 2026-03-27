#include <Environment/GenerateTerrain.hpp>

#include <Environment/VoxelTerrainChunk.hpp>

#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ConfigHelper.hpp>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <unordered_set>

namespace
{
    unsigned int HashU32(unsigned int _value)
    {
        _value ^= _value >> 16u;
        _value *= 0x7feb352du;
        _value ^= _value >> 15u;
        _value *= 0x846ca68bu;
        _value ^= _value >> 16u;
        return _value;
    }

    float Hash01(int _x, int _y, int _z, int _seed)
    {
        unsigned int value = static_cast<unsigned int>(_seed);
        value ^= HashU32(static_cast<unsigned int>(_x) * 0x1f123bb5u);
        value ^= HashU32(static_cast<unsigned int>(_y) * 0x9e3779b9u);
        value ^= HashU32(static_cast<unsigned int>(_z) * 0x94d049bbu);
        return static_cast<float>(HashU32(value) & 0x00ffffffu) / static_cast<float>(0x01000000u);
    }

    float SmoothStep(float _t)
    {
        return _t * _t * (3.0f - (2.0f * _t));
    }

    float Lerp(float _a, float _b, float _t)
    {
        return _a + ((_b - _a) * _t);
    }

    float ValueNoise2D(float _x, float _z, int _seed)
    {
        const int x0 = static_cast<int>(std::floor(_x));
        const int z0 = static_cast<int>(std::floor(_z));
        const int x1 = x0 + 1;
        const int z1 = z0 + 1;

        const float tx = SmoothStep(_x - static_cast<float>(x0));
        const float tz = SmoothStep(_z - static_cast<float>(z0));

        const float v00 = Hash01(x0, 0, z0, _seed);
        const float v10 = Hash01(x1, 0, z0, _seed);
        const float v01 = Hash01(x0, 0, z1, _seed);
        const float v11 = Hash01(x1, 0, z1, _seed);

        return Lerp(Lerp(v00, v10, tx), Lerp(v01, v11, tx), tz);
    }

    float ValueNoise3D(float _x, float _y, float _z, int _seed)
    {
        const int x0 = static_cast<int>(std::floor(_x));
        const int y0 = static_cast<int>(std::floor(_y));
        const int z0 = static_cast<int>(std::floor(_z));
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;
        const int z1 = z0 + 1;

        const float tx = SmoothStep(_x - static_cast<float>(x0));
        const float ty = SmoothStep(_y - static_cast<float>(y0));
        const float tz = SmoothStep(_z - static_cast<float>(z0));

        const float v000 = Hash01(x0, y0, z0, _seed);
        const float v100 = Hash01(x1, y0, z0, _seed);
        const float v010 = Hash01(x0, y1, z0, _seed);
        const float v110 = Hash01(x1, y1, z0, _seed);
        const float v001 = Hash01(x0, y0, z1, _seed);
        const float v101 = Hash01(x1, y0, z1, _seed);
        const float v011 = Hash01(x0, y1, z1, _seed);
        const float v111 = Hash01(x1, y1, z1, _seed);

        const float ix00 = Lerp(v000, v100, tx);
        const float ix10 = Lerp(v010, v110, tx);
        const float ix01 = Lerp(v001, v101, tx);
        const float ix11 = Lerp(v011, v111, tx);

        return Lerp(Lerp(ix00, ix10, ty), Lerp(ix01, ix11, ty), tz);
    }

    float FractalNoise2D(float _x, float _z, int _seed, int _octaves)
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float normalization = 0.0f;

        for (int octave = 0; octave < _octaves; ++octave)
        {
            total += ValueNoise2D(_x * frequency, _z * frequency, _seed + (octave * 31)) * amplitude;
            normalization += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        return normalization > 0.0f ? (total / normalization) : 0.0f;
    }

    float RidgedNoise2D(float _x, float _z, int _seed, int _octaves)
    {
        return 1.0f - std::abs((FractalNoise2D(_x, _z, _seed, _octaves) * 2.0f) - 1.0f);
    }

    float FractalNoise3D(float _x, float _y, float _z, int _seed, int _octaves)
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float normalization = 0.0f;

        for (int octave = 0; octave < _octaves; ++octave)
        {
            total += ValueNoise3D(_x * frequency, _y * frequency, _z * frequency, _seed + (octave * 57)) * amplitude;
            normalization += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        return normalization > 0.0f ? (total / normalization) : 0.0f;
    }

    float Saturate(float _value)
    {
        return std::clamp(_value, 0.0f, 1.0f);
    }

    float RidgedNoise3D(float _x, float _y, float _z, int _seed, int _octaves)
    {
        return 1.0f - std::abs((FractalNoise3D(_x, _y, _z, _seed, _octaves) * 2.0f) - 1.0f);
    }

    int GetTerrainHeight(const GenerateTerrain &_terrain, int _globalX, int _globalZ)
    {
        const float broad = FractalNoise2D(
            static_cast<float>(_globalX) * _terrain.heightNoiseScale,
            static_cast<float>(_globalZ) * _terrain.heightNoiseScale,
            _terrain.seed,
            4);
        const float detail = FractalNoise2D(
            static_cast<float>(_globalX) * _terrain.detailNoiseScale,
            static_cast<float>(_globalZ) * _terrain.detailNoiseScale,
            _terrain.seed + 101,
            2);
        const float blended = std::clamp((broad * 0.75f) + (detail * 0.25f), 0.0f, 1.0f);

        const float hillScale = std::max(0.005f, _terrain.hillNoiseScale);
        const float hillShape = RidgedNoise2D(
            static_cast<float>(_globalX) * hillScale,
            static_cast<float>(_globalZ) * hillScale,
            _terrain.seed + 173,
            4);
        const float hillMask = FractalNoise2D(
            static_cast<float>(_globalX) * hillScale * 0.55f,
            static_cast<float>(_globalZ) * hillScale * 0.55f,
            _terrain.seed + 211,
            2);
        const float hillStrength = Saturate((hillShape - 0.38f) / 0.62f) * Saturate((hillMask - 0.42f) / 0.58f);
        const float hillBoost = std::pow(hillStrength, 1.35f) * static_cast<float>(std::max(0, _terrain.hillHeightBoost));

        return _terrain.baseHeight
            + static_cast<int>(std::round((blended * static_cast<float>(_terrain.maxHeightVariation)) + hillBoost));
    }

    Canis::Vector3 GetCaveWarpedPosition(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ)
    {
        const float warpScale = std::max(0.005f, _terrain.caveNoiseScale * 0.38f);
        const float warpStrength = 6.0f;
        const float x = static_cast<float>(_globalX);
        const float y = static_cast<float>(_y);
        const float z = static_cast<float>(_globalZ);

        const float warpX = (FractalNoise3D(
            (x + 17.31f) * warpScale,
            (y - 4.73f) * warpScale,
            (z + 11.19f) * warpScale,
            _terrain.seed + 421,
            2) * 2.0f) - 1.0f;
        const float warpY = (FractalNoise3D(
            (x - 23.14f) * warpScale,
            (y + 8.62f) * warpScale,
            (z - 19.47f) * warpScale,
            _terrain.seed + 463,
            2) * 2.0f) - 1.0f;
        const float warpZ = (FractalNoise3D(
            (x + 5.61f) * warpScale,
            (y + 13.08f) * warpScale,
            (z - 29.53f) * warpScale,
            _terrain.seed + 509,
            2) * 2.0f) - 1.0f;

        return Canis::Vector3(
            x + (warpX * warpStrength),
            y + (warpY * warpStrength * 0.55f),
            z + (warpZ * warpStrength));
    }

    float GetSpaghettiTunnelField(const GenerateTerrain &_terrain, const Canis::Vector3 &_warpedPosition)
    {
        const float tunnelScale = std::max(0.01f, _terrain.caveNoiseScale * 1.28f);
        const float x = _warpedPosition.x;
        const float y = _warpedPosition.y;
        const float z = _warpedPosition.z;

        const float primarySlice = std::abs((FractalNoise3D(
            x * tunnelScale,
            y * tunnelScale * 0.56f,
            z * tunnelScale,
            _terrain.seed + 401,
            3) * 2.0f) - 1.0f);
        const float branchSlice = std::abs((FractalNoise3D(
            (x + 41.0f) * tunnelScale * 1.22f,
            (y - 12.0f) * tunnelScale * 0.72f,
            (z - 31.0f) * tunnelScale * 1.22f,
            _terrain.seed + 487,
            3) * 2.0f) - 1.0f);

        const float thicknessNoise = FractalNoise3D(
            (x - 13.0f) * tunnelScale * 0.64f,
            (y + 7.0f) * tunnelScale * 0.64f,
            (z + 17.0f) * tunnelScale * 0.64f,
            _terrain.seed + 533,
            2);
        const float branchNoise = FractalNoise3D(
            (x + 29.0f) * tunnelScale * 0.58f,
            (y - 5.0f) * tunnelScale * 0.58f,
            (z - 23.0f) * tunnelScale * 0.58f,
            _terrain.seed + 557,
            2);

        const float primaryThickness = Lerp(0.075f, 0.11f, thicknessNoise);
        const float branchThickness = Lerp(0.055f, 0.09f, branchNoise);

        const float primaryTunnel = 1.0f - Saturate(primarySlice / std::max(0.001f, primaryThickness));
        const float branchTunnel = 1.0f - Saturate(branchSlice / std::max(0.001f, branchThickness));
        return Saturate(std::max(primaryTunnel, branchTunnel * 0.94f));
    }

    float GetTallTunnelField(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ)
    {
        float strongestTunnel = 0.0f;
        for (int verticalOffset = -1; verticalOffset <= 1; ++verticalOffset)
        {
            const Canis::Vector3 warpedPosition = GetCaveWarpedPosition(_terrain, _globalX, _y + verticalOffset, _globalZ);
            const float tunnelField = GetSpaghettiTunnelField(_terrain, warpedPosition);
            const float weight = (verticalOffset == 0) ? 1.0f : 0.92f;
            strongestTunnel = std::max(strongestTunnel, tunnelField * weight);
        }

        return strongestTunnel;
    }

    float GetChamberField(const GenerateTerrain &_terrain, const Canis::Vector3 &_warpedPosition)
    {
        const float chamberScale = std::max(0.006f, _terrain.caveNoiseScale * 0.48f);
        const float x = _warpedPosition.x;
        const float y = _warpedPosition.y;
        const float z = _warpedPosition.z;

        const float chamberNoise = FractalNoise3D(
            x * chamberScale,
            y * chamberScale * 0.72f,
            z * chamberScale,
            _terrain.seed + 611,
            4);
        const float chamberMask = FractalNoise2D(
            x * chamberScale * 0.55f,
            z * chamberScale * 0.55f,
            _terrain.seed + 643,
            3);

        const float chamberShape = Saturate((chamberNoise - 0.70f) / 0.30f);
        const float chamberPresence = Saturate((chamberMask - 0.58f) / 0.42f);
        return Saturate(chamberShape * chamberPresence);
    }

    float GetCaveField(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ)
    {
        // Layered, warped noise keeps the cave network tunnel-first, with sparse chambers.
        const Canis::Vector3 warpedPosition = GetCaveWarpedPosition(_terrain, _globalX, _y, _globalZ);
        const float tunnelField = GetTallTunnelField(_terrain, _globalX, _y, _globalZ);
        const float chamberField = GetChamberField(_terrain, warpedPosition);
        return Saturate(std::max(tunnelField, chamberField));
    }

    bool IsTerrainAir(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ, int _columnHeight)
    {
        if (_y < 0)
            return false;

        if (_y < _terrain.bedrockLayerHeight)
            return false;

        if (_y >= _columnHeight)
            return true;

        const int depthFromSurface = (_columnHeight - 1) - _y;
        if (_y <= 1 || depthFromSurface <= 0)
            return false;

        const float caveField = GetCaveField(_terrain, _globalX, _y, _globalZ);
        float caveThreshold = 0.60f;
        if (depthFromSurface <= 4)
        {
            const float surfaceBias = static_cast<float>(depthFromSurface) / 4.0f;
            caveThreshold += Lerp(0.22f, 0.06f, surfaceBias);
        }
        if (_y <= 3)
            caveThreshold += 0.16f;

        const float normalizedHeight = static_cast<float>(_y) / static_cast<float>(std::max(1, _terrain.chunkHeight - 1));
        caveThreshold += std::abs(normalizedHeight - 0.43f) * 0.18f;

        return caveField > caveThreshold;
    }

    bool IsTerrainAir(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ)
    {
        const int columnHeight = std::min(_terrain.chunkHeight, GetTerrainHeight(_terrain, _globalX, _globalZ));
        return IsTerrainAir(_terrain, _globalX, _y, _globalZ, columnHeight);
    }

    bool IsAdjacentToAir(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ)
    {
        static constexpr int kNeighborOffsets[6][3] = {
            { 1, 0, 0 },
            { -1, 0, 0 },
            { 0, 1, 0 },
            { 0, -1, 0 },
            { 0, 0, 1 },
            { 0, 0, -1 },
        };

        for (const auto &offset : kNeighborOffsets)
        {
            if (IsTerrainAir(_terrain, _globalX + offset[0], _y + offset[1], _globalZ + offset[2]))
                return true;
        }

        return false;
    }

    float GetOreField(float _x, float _y, float _z, float _scale, int _seed)
    {
        const float clusterNoise = FractalNoise3D(_x * _scale, _y * _scale, _z * _scale, _seed, 3);
        const float veinNoise = RidgedNoise3D(_x * _scale * 2.4f, _y * _scale * 2.4f, _z * _scale * 2.4f, _seed + 19, 2);
        return Saturate((clusterNoise * 0.28f) + (veinNoise * 0.72f));
    }

    std::uint64_t MakeChunkKey(int _chunkX, int _chunkZ)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(_chunkX)) << 32u) |
            static_cast<std::uint32_t>(_chunkZ);
    }

    int GetChunkWindowStart(int _centerChunk, int _windowSize)
    {
        return _centerChunk - (_windowSize / 2);
    }

    TerrainBlockType GetTerrainBlockType(const GenerateTerrain &_terrain, int _globalX, int _y, int _globalZ, int _columnHeight)
    {
        const bool isSurface = (_y == (_columnHeight - 1));
        if (_y < _terrain.bedrockLayerHeight)
            return TerrainBlockType::Bedrock;

        if (IsTerrainAir(_terrain, _globalX, _y, _globalZ, _columnHeight))
            return TerrainBlockType::Air;

        if (isSurface && _columnHeight >= _terrain.surfaceIceHeight)
            return TerrainBlockType::Ice;

        const float blockX = static_cast<float>(_globalX);
        const float blockY = static_cast<float>(_y);
        const float blockZ = static_cast<float>(_globalZ);

        if (_y < (_columnHeight / 2))
        {
            const float uraniumNoise = GetOreField(blockX, blockY, blockZ, 0.18f, _terrain.seed + 601);
            if (uraniumNoise > 0.88f)
                return TerrainBlockType::Uranium;
            if (uraniumNoise > 0.79f && IsAdjacentToAir(_terrain, _globalX, _y, _globalZ))
                return TerrainBlockType::Uranium;
        }

        if (_y < (_columnHeight - 1))
        {
            const float goldNoise = GetOreField(blockX, blockY, blockZ, 0.14f, _terrain.seed + 777);
            if (goldNoise > 0.86f)
                return TerrainBlockType::Gold;
            if (goldNoise > 0.76f && IsAdjacentToAir(_terrain, _globalX, _y, _globalZ))
                return TerrainBlockType::Gold;
        }

        return TerrainBlockType::Rock;
    }
}

ScriptConf generateTerrainConf = {};

void RegisterGenerateTerrainScript(Canis::App& _app)
{
    DEFAULT_CONFIG_AND_REQUIRED(generateTerrainConf, GenerateTerrain, Transform);

    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, seed);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, chunksX);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, chunksZ);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, chunkSize);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, chunkHeight);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, baseHeight);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, maxHeightVariation);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, hillHeightBoost);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, bedrockLayerHeight);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, surfaceIceHeight);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, heightNoiseScale);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, detailNoiseScale);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, hillNoiseScale);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, caveNoiseScale);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, rockDropPrefab);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, iceDropPrefab);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, goldDropPrefab);
    REGISTER_PROPERTY(generateTerrainConf, GenerateTerrain, uraniumDropPrefab);

    generateTerrainConf.DEFAULT_DRAW_INSPECTOR(GenerateTerrain);

    _app.RegisterScript(generateTerrainConf);
}

DEFAULT_UNREGISTER_SCRIPT(generateTerrainConf, GenerateTerrain)

void GenerateTerrain::Create() {}

void GenerateTerrain::CacheMaterials()
{
    if (m_rockMaterialId < 0)
        m_rockMaterialId = Canis::AssetManager::LoadMaterial("assets/materials/terrain_rock.material");
    if (m_iceMaterialId < 0)
        m_iceMaterialId = Canis::AssetManager::LoadMaterial("assets/materials/terrain_ice.material");
    if (m_goldMaterialId < 0)
        m_goldMaterialId = Canis::AssetManager::LoadMaterial("assets/materials/terrain_gold.material");
    if (m_uraniumMaterialId < 0)
        m_uraniumMaterialId = Canis::AssetManager::LoadMaterial("assets/materials/terrain_uranium.material");
}

void GenerateTerrain::EnsurePlayerEntity()
{
    if (m_playerEntity != nullptr &&
        m_playerEntity->active &&
        m_playerEntity->HasComponent<Canis::Transform>())
        return;

    m_playerEntity = entity.scene.GetEntityWithTag("Player");
    if (m_playerEntity != nullptr &&
        (!m_playerEntity->active || !m_playerEntity->HasComponent<Canis::Transform>()))
        m_playerEntity = nullptr;
}

Canis::Vector3 GenerateTerrain::GetStreamingFocusPosition() const
{
    if (m_playerEntity != nullptr &&
        m_playerEntity->active &&
        m_playerEntity->HasComponent<Canis::Transform>())
    {
        return m_playerEntity->GetComponent<Canis::Transform>().GetGlobalPosition();
    }

    if (entity.HasComponent<Canis::Transform>())
        return entity.GetComponent<Canis::Transform>().GetGlobalPosition();

    return Canis::Vector3(0.0f);
}

void GenerateTerrain::PopulateChunk(VoxelTerrainChunk &_chunk, int _chunkX, int _chunkZ) const
{
    _chunk.Resize(chunkSize, chunkHeight, chunkSize);

    for (int localZ = 0; localZ < chunkSize; ++localZ)
    {
        for (int localX = 0; localX < chunkSize; ++localX)
        {
            const int globalX = (_chunkX * chunkSize) + localX;
            const int globalZ = (_chunkZ * chunkSize) + localZ;
            const int columnHeight = std::min(chunkHeight, GetTerrainHeight(*this, globalX, globalZ));

            for (int y = 0; y < columnHeight; ++y)
            {
                const TerrainBlockType blockType = GetTerrainBlockType(*this, globalX, y, globalZ, columnHeight);
                if (blockType != TerrainBlockType::Air)
                    _chunk.SetBlock(localX, y, localZ, blockType);
            }
        }
    }
}

Canis::Entity* GenerateTerrain::CreateChunkEntity(int _chunkX, int _chunkZ)
{
    Canis::Entity *chunkEntity = entity.scene.CreateEntity(
        "TerrainChunk_" + std::to_string(_chunkX) + "_" + std::to_string(_chunkZ));
    if (chunkEntity == nullptr)
        return nullptr;

    Canis::Transform &chunkTransform = chunkEntity->GetComponent<Canis::Transform>();
    chunkTransform.SetParent(&entity);
    chunkTransform.position = Canis::Vector3(
        static_cast<float>(_chunkX * chunkSize),
        0.0f,
        static_cast<float>(_chunkZ * chunkSize));

    Canis::Rigidbody &rigidbody = chunkEntity->GetComponent<Canis::Rigidbody>();
    rigidbody.motionType = Canis::RigidbodyMotionType::STATIC;
    rigidbody.useGravity = false;
    rigidbody.layer = 5u;

    Canis::MeshCollider &meshCollider = chunkEntity->GetComponent<Canis::MeshCollider>();
    meshCollider.active = true;
    meshCollider.useAttachedModel = true;

    chunkEntity->GetComponent<Canis::Model>();
    chunkEntity->GetComponent<Canis::Material>();

    VoxelTerrainChunk *chunk = chunkEntity->AddScript<VoxelTerrainChunk>();
    if (chunk == nullptr)
    {
        entity.scene.Destroy(*chunkEntity);
        return nullptr;
    }

    chunk->sizeX = chunkSize;
    chunk->sizeY = chunkHeight;
    chunk->sizeZ = chunkSize;
    chunk->rockMaterialId = m_rockMaterialId;
    chunk->iceMaterialId = m_iceMaterialId;
    chunk->goldMaterialId = m_goldMaterialId;
    chunk->uraniumMaterialId = m_uraniumMaterialId;
    chunk->rockDropPrefab = rockDropPrefab;
    chunk->iceDropPrefab = iceDropPrefab;
    chunk->goldDropPrefab = goldDropPrefab;
    chunk->uraniumDropPrefab = uraniumDropPrefab;

    PopulateChunk(*chunk, _chunkX, _chunkZ);
    chunk->RebuildMesh();
    return chunkEntity;
}

void GenerateTerrain::RefreshLoadedChunks(bool _forceRefresh)
{
    if (!entity.HasComponent<Canis::Transform>())
        return;

    EnsurePlayerEntity();

    const Canis::Vector3 focusPosition = GetStreamingFocusPosition();
    const Canis::Vector3 terrainOrigin = entity.GetComponent<Canis::Transform>().GetGlobalPosition();

    const int centerChunkX = static_cast<int>(std::floor((focusPosition.x - terrainOrigin.x) / static_cast<float>(chunkSize)));
    const int centerChunkZ = static_cast<int>(std::floor((focusPosition.z - terrainOrigin.z) / static_cast<float>(chunkSize)));

    if (!_forceRefresh &&
        centerChunkX == m_lastCenterChunkX &&
        centerChunkZ == m_lastCenterChunkZ)
        return;

    const int startChunkX = GetChunkWindowStart(centerChunkX, chunksX);
    const int startChunkZ = GetChunkWindowStart(centerChunkZ, chunksZ);

    std::unordered_set<std::uint64_t> desiredChunkKeys = {};
    desiredChunkKeys.reserve(static_cast<size_t>(chunksX * chunksZ));

    for (int chunkOffsetZ = 0; chunkOffsetZ < chunksZ; ++chunkOffsetZ)
    {
        for (int chunkOffsetX = 0; chunkOffsetX < chunksX; ++chunkOffsetX)
        {
            const int chunkX = startChunkX + chunkOffsetX;
            const int chunkZ = startChunkZ + chunkOffsetZ;
            const std::uint64_t chunkKey = MakeChunkKey(chunkX, chunkZ);
            desiredChunkKeys.insert(chunkKey);

            if (m_loadedChunkEntities.find(chunkKey) != m_loadedChunkEntities.end())
                continue;

            if (Canis::Entity *chunkEntity = CreateChunkEntity(chunkX, chunkZ))
                m_loadedChunkEntities[chunkKey] = chunkEntity->id;
        }
    }

    std::vector<std::uint64_t> keysToUnload = {};
    keysToUnload.reserve(m_loadedChunkEntities.size());
    for (const auto &[chunkKey, entityId] : m_loadedChunkEntities)
    {
        (void)entityId;
        if (desiredChunkKeys.find(chunkKey) == desiredChunkKeys.end())
            keysToUnload.push_back(chunkKey);
    }

    for (const std::uint64_t chunkKey : keysToUnload)
    {
        auto it = m_loadedChunkEntities.find(chunkKey);
        if (it == m_loadedChunkEntities.end())
            continue;

        if (Canis::Entity *chunkEntity = entity.scene.GetEntity(it->second))
            entity.scene.Destroy(*chunkEntity);

        m_loadedChunkEntities.erase(it);
    }

    m_lastCenterChunkX = centerChunkX;
    m_lastCenterChunkZ = centerChunkZ;
}

void GenerateTerrain::Ready()
{
    if (m_generated || !entity.HasComponent<Canis::Transform>())
        return;

    chunksX = std::max(1, chunksX);
    chunksZ = std::max(1, chunksZ);
    chunkSize = std::max(4, chunkSize);
    chunkHeight = std::max(4, chunkHeight);
    baseHeight = std::clamp(baseHeight, 1, chunkHeight - 1);
    maxHeightVariation = std::max(1, maxHeightVariation);
    hillHeightBoost = std::max(0, hillHeightBoost);
    bedrockLayerHeight = std::clamp(bedrockLayerHeight, 0, chunkHeight);
    surfaceIceHeight = std::max(1, surfaceIceHeight);
    m_loadedChunkEntities.clear();
    m_lastCenterChunkX = std::numeric_limits<int>::max();
    m_lastCenterChunkZ = std::numeric_limits<int>::max();

    CacheMaterials();
    EnsurePlayerEntity();
    RefreshLoadedChunks(true);

    m_generated = true;
}

void GenerateTerrain::Destroy()
{
    for (const auto &[chunkKey, entityId] : m_loadedChunkEntities)
    {
        (void)chunkKey;
        if (Canis::Entity *chunkEntity = entity.scene.GetEntity(entityId))
            entity.scene.Destroy(*chunkEntity);
    }

    m_loadedChunkEntities.clear();
    m_playerEntity = nullptr;
    m_lastCenterChunkX = std::numeric_limits<int>::max();
    m_lastCenterChunkZ = std::numeric_limits<int>::max();
    m_generated = false;
}

void GenerateTerrain::Update(float _dt)
{
    (void)_dt;

    if (!m_generated)
        return;

    RefreshLoadedChunks(false);
}
