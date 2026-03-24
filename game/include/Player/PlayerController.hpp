#pragma once

#include <Canis/Entity.hpp>

class PlayerController : public Canis::ScriptableEntity
{
private:
    Canis::Entity* m_cameraEntity = nullptr;
    float m_cameraPitch = 0.0f;

public:
    static constexpr const char* ScriptName = "PlayerController";

    float walkingSpeed = 6.0f;
    float sprintingSpeed = 10.0f;
    float turnSpeed = 0.12f;
    float maxLookAngle = 89.0f;
    float jumpImpulse = 7.5f;
    float groundCheckDistance = 0.75f;
    Canis::Mask groundCollisionMask = Canis::Rigidbody::DefaultLayer;
    float pickupRadius = 1.15f;

    bool grounded = false;

    PlayerController(Canis::Entity &_entity) : Canis::ScriptableEntity(_entity) {}

    void Create();
    void Ready();
    void Destroy();
    void Update(float _dt);
};

extern void RegisterPlayerControllerScript(Canis::App& _app);
extern void UnRegisterPlayerControllerScript(Canis::App& _app);
