#include <Player/PlayerController.hpp>

#include <Canis/App.hpp>
#include <Canis/Debug.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Window.hpp>

ScriptConf playerConf = {};

void RegisterPlayerControllerScript(App& _app)
{
    REGISTER_PROPERTY(playerConf, PlayerController, walkingSpeed);
    REGISTER_PROPERTY(playerConf, PlayerController, sprintingSpeed);
    REGISTER_PROPERTY(playerConf, PlayerController, turnSpeed);
    REGISTER_PROPERTY(playerConf, PlayerController, maxLookAngle);
    REGISTER_PROPERTY(playerConf, PlayerController, jumpImpulse);
    REGISTER_PROPERTY(playerConf, PlayerController, groundCheckDistance);
    REGISTER_PROPERTY(playerConf, PlayerController, groundCollisionMask);
    REGISTER_PROPERTY(playerConf, PlayerController, pickupRadius);
    
    DEFAULT_CONFIG_AND_REQUIRED(playerConf, PlayerController, Transform, Rigidbody, CapsuleCollider);

    playerConf.DEFAULT_DRAW_INSPECTOR(PlayerController);

    _app.RegisterScript(playerConf);
}

DEFAULT_UNREGISTER_SCRIPT(playerConf, PlayerController)

void PlayerController::Create() {}

void PlayerController::Ready()
{
    m_cameraEntity = entity.scene.GetEntityWithTag("MainCamera");

    if (m_cameraEntity != nullptr && m_cameraEntity->HasComponent<Transform>())
    {
        Transform& cameraTransform = m_cameraEntity->GetComponent<Transform>();
        m_cameraPitch = std::clamp(RAD2DEG * cameraTransform.rotation.x, -maxLookAngle, maxLookAngle);
        cameraTransform.rotation = Vector3(DEG2RAD * m_cameraPitch, 0.0f, 0.0f);
    }

    entity.scene.GetWindow().LockMouse(true);
}

void PlayerController::Destroy()
{
    entity.scene.GetWindow().LockMouse(false);
}

void PlayerController::Update(float _dt)
{
    if (!entity.HasComponents<Transform, Rigidbody>())
        return;

    Transform& transform = entity.GetComponent<Transform>();
    Rigidbody& rigidbody = entity.GetComponent<Rigidbody>();

    InputManager& input = entity.scene.GetInputManager();
    Window& window = entity.scene.GetWindow();
    
    RaycastHit groundHit = {};
    const Vector3 groundRayOrigin = transform.GetGlobalPosition() + Vector3(0.0f, 0.1f, 0.0f);
    grounded = entity.scene.Raycast(groundRayOrigin, Vector3(0.0f, -1.0f, 0.0f), groundHit, groundCheckDistance, groundCollisionMask);

    Vector3 inputDirection = Vector3(0.0f);

    if (input.JustPressedKey(Key::ESCAPE))
    {
        window.LockMouse(!window.IsMouseLocked());
    }

    if (!window.IsMouseLocked() && input.JustLeftClicked())
    {
        window.LockMouse(true);
    }

    if (window.IsMouseLocked())
    {
        window.CenterMouse();
        transform.rotation.y -= DEG2RAD * input.mouseRel.x * turnSpeed;

        if (m_cameraEntity && m_cameraEntity->HasComponent<Transform>())
        {
            Transform& cameraTransform = m_cameraEntity->GetComponent<Transform>();

            m_cameraPitch -= input.mouseRel.y * turnSpeed;
            m_cameraPitch = std::clamp(m_cameraPitch, -maxLookAngle, maxLookAngle);

            cameraTransform.rotation = Vector3(DEG2RAD * m_cameraPitch, 0.0f, 0.0f);
        }
    }

    if (input.GetKey(Key::A) || input.GetKey(Key::LEFT))
        inputDirection.x -= 1.0f;
    if (input.GetKey(Key::D) || input.GetKey(Key::RIGHT))
        inputDirection.x += 1.0f;
    if (input.GetKey(Key::W) || input.GetKey(Key::UP))
        inputDirection.z -= 1.0f;
    if (input.GetKey(Key::S) || input.GetKey(Key::DOWN))
        inputDirection.z += 1.0f;
    
    bool sprint = input.GetKey(Key::LSHIFT);

    if (grounded && input.JustPressedKey(Key::SPACE))
        rigidbody.AddForce(Vector3(0.0f, jumpImpulse, 0.0f), Rigidbody3DForceMode::IMPULSE);

    Vector3 forward = transform.GetForward();
    Vector3 right = transform.GetRight();
    forward.y = 0.0f;
    right.y = 0.0f;

    if (glm::dot(forward, forward) > 0.0001f)
        forward = glm::normalize(forward);
    if (glm::dot(right, right) > 0.0001f)
        right = glm::normalize(right);

    Vector3 desiredVelocity = Vector3(0.0f);
    if (inputDirection != Vector3(0.0f))
    {
        Vector3 movement = (forward * inputDirection.z) + (right * inputDirection.x);
        movement = glm::normalize(movement);
        float speed = sprint ? sprintingSpeed : walkingSpeed;
        desiredVelocity = movement * speed;
    }

    Vector3 horizontalVelocity = Vector3(rigidbody.linearVelocity.x, 0.0f, rigidbody.linearVelocity.z);
    Vector3 horizontalVelocityChange = desiredVelocity - horizontalVelocity;
    rigidbody.AddForce(Vector3(horizontalVelocityChange.x, 0.0f, horizontalVelocityChange.z), Rigidbody3DForceMode::VELOCITY_CHANGE);
}
