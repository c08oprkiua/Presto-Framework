// Player header file - SPG-accurate Sonic physics
#ifndef PLAYER_PLAYER_H
#define PLAYER_PLAYER_H

#include "raylib.h"
#include "player-var.h"
#include "../entity-sprite_object.h"
#include "../../managers/managers-animation.h"
#include "../../data/data-data.h"
#include <math.h>
#include <stdint.h>

typedef enum Directions {
    NONE = 0,
    DOWN = 1 << 0,
    RIGHT = 1 << 1,
    UP = 1 << 2,
    LEFT = 1 << 3,
} Directions;

typedef enum ComboDirections {
    DOWN_RIGHT = DOWN | RIGHT,
    UP_RIGHT = UP | RIGHT,
    UP_LEFT = UP | LEFT,
    DOWN_LEFT = DOWN | LEFT,
    ANY = DOWN | UP | LEFT | RIGHT
} ComboDirections;

// ========== Player Structures and Enums ==========
// Player State
typedef enum {
    IDLE,
    WALK,
    CROUCH,
    LOOK_UP,
    SKID,
    RUN,
    DASH,
    PUSH,
    JUMP,
    FALL,
    ROLL,
    ROLL_FALL,
    SPINDASH,
    PEELOUT,
    FLY,
    GLIDE,
    CLIMB,
    SWIM,
    MONKEYBARS,
    WALLJUMP,
    SPRING_RISE,
    SLIDE,
    HURT,
    DEAD
} PlayerState;

typedef enum {
    IDLE_NORMAL,
    IMPATIENT_LOOK,
    TIRED,
    IMPATIENT_ANIMATION
} PlayerIdleState;

typedef enum {
    ANIM_IDLE,
    ANIM_IMPATIENT_LOOK,
    ANIM_TIRED,
    ANIM_IMPATIENT,
    ANIM_IM_OUTTA_HERE_LOOK,
    ANIM_IM_OUTTA_HERE,
    ANIM_JUMP_OFF_SCREEN_1,
    ANIM_JUMP_OFF_SCREEN_2,
    ANIM_WALK,
    ANIM_CROUCH,
    ANIM_LOOK_UP,
    ANIM_LOOK_DOWN,
    ANIM_SKID,
    ANIM_RUN,
    ANIM_DASH,
    ANIM_PUSH,
    ANIM_JUMP,
    ANIM_FALL,
    ANIM_ROLL,
    ANIM_ROLL_FALL,
    ANIM_SPINDASH,
    ANIM_PEELOUT,
    ANIM_PEELOUT_CHARGED,
    ANIM_FLY,
    ANIM_GLIDE,
    ANIM_CLIMB,
    ANIM_SWIM,
    ANIM_MONKEYBARS,
    ANIM_MONKEYBARS_MOVE,
    ANIM_WALLJUMP,
    ANIM_SLIDE,
    ANIM_AIRBUBBLE,
    ANIM_HURT,
    ANIM_DEAD,
    ANIM_WOBBLE_FRONT,
    ANIM_WOBBLE_BACK,
    ANIM_SPRING_1,
    ANIM_SPRING_2,
    ANIM_BURN,
    ANIM_DROWN,
    ANIM_FANSPIN,
    ANIM_FANSPIN_FALL,
    ANIM_TAUNT,
    ANIM_CELEBRATE,
    ANIM_SURPRISED
} PlayerAnimationState;

// Slip Angle Type
typedef enum {
    SONIC_1_2_CD,
    SONIC_3K
} SlipAngleType;

// Player Structure - SPG-accurate physics
typedef struct {
    // Core position and velocity
    Vector2 position;           // Center position of player
    Vector2 velocity;           // X and Y speed (pixels per frame)

    // SPG ground speed (magnitude along ground surface)
    float groundSpeed;          // Speed along the ground surface
    uint8_t groundAngle;        // Ground angle (0-255, where 0=flat, 64=right wall, 128=ceiling, 192=left wall)
    Directions collisionMode; // Current collision mode based on angle

    // Hitbox radii (SPG style)
    float widthRadius;          // Half-width of collision box
    float heightRadius;         // Half-height of collision box
    float pushRadius;           // Radius for wall push sensors (usually 10)
    float defaultWidthRadius;   // Default width (for restoring after roll)
    float defaultHeightRadius;  // Default height (for restoring after roll)

    // State flags
    bool isOnGround;
    bool isJumping;
    bool hasJumped;
    bool isFalling;
    bool isRolling;
    bool isCrouching;
    bool isLookingUp;
    bool isSpindashing;
    bool isSuper;
    bool isPeelOut;
    bool isFlying;
    bool isGliding;
    bool isClimbing;
    bool isHurt;
    bool isDead;

    // Input state
    bool inputLeft;
    bool inputRight;
    bool inputUp;
    bool inputDown;
    bool inputJump;
    bool inputJumpPressed;      // True only on the frame jump was pressed

    // Facing direction: 1 = right, -1 = left
    int8_t facing;

    // Player type and state machine
    PlayerType type;
    PlayerState state;
    PlayerIdleState idleState;

    // Timers
    uint8_t controlLockTimer;   // Frames to lock controls (slope slip)
    uint8_t invincibilityTimer; // Frames of invincibility after being hurt
    uint8_t jumpButtonHoldTimer;
    uint8_t blinkTimer;
    uint8_t blinkInterval;
    uint8_t blinkDuration;
    float idleTimer;
    float idleLookTimer;

    // Spindash
    uint8_t spindashCharge;

    // Animation
    SpriteObject* sprite;
    PlayerAnimationState animationState;
    AnimationManager* animationManager;

    // Misc
    bool isImpatient;
    float impatientTimer;
    uint16_t slipAngleType;

    // Legacy (kept for compatibility)
    bool jumpPressed;
    Rectangle collisionBox;
} Player;

// Player Function Prototypes
void InitPlayer(Player* player, PlayerType type, Vector2 startPosition);
void UpdatePlayer(Player* player, float deltaTime);
void DrawPlayer(const Player* player);
void SetPlayerAnimation(Player* player, PlayerAnimationState newState);
//Get the calculated sensor positions for the player based on their current mode.
void GetPlayerSensorPositions(Player *player, Vector2 *groundSensorA, Vector2 *groundSensorB, Vector2 *wallSensor);
void HandlePlayerInput(Player* player);
void UpdatePlayerState(Player* player);
void UpdatePlayerAnimation(Player* player, float deltaTime);
void ResetPlayer(Player* player, Vector2 startPosition);

#endif // PLAYER_PLAYER_H
