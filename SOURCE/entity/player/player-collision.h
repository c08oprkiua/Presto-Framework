// Player Collision Header - SPG-accurate collision detection
#ifndef PLAYER_COLLISION_H
#define PLAYER_COLLISION_H

#include "raylib.h"
#include "player-player.h"
#include <stdint.h>
#include <stdbool.h>

// Sensor result from a single sensor check
typedef struct {
    // Did we find a tile?
    bool found;
    //Distance to contact point (negative = inside, 0 = touching, positive = gap)
    int distance;
    // Angle of the tile found
    uint8_t angle;
    // Tile x coordinate on the tilemap
    int tileX;
    // Tile y coordinate on the tilemap
    int tileY;
    // The tile ID found
    int tileId;
    //Global coordinates of the contact point.
    Vector2 globalContactPoint;
} SensorResult;

// All sensor results for a frame
typedef struct {
    SensorResult groundA;   // Left ground sensor
    SensorResult groundB;   // Right ground sensor
    SensorResult ceilingC;  // Left ceiling sensor
    SensorResult ceilingD;  // Right ceiling sensor
    SensorResult pushE;     // Left wall sensor
    SensorResult pushF;     // Right wall sensor
} PlayerSensorResults;

// Level collision data reference (set by game screen)
typedef struct {
    int** tileData;
    int width;
    int height;
} LevelCollision;

// Global level collision reference
extern LevelCollision g_LevelCollision;

// Initialize the collision system with level data
void InitCollisionSystem(int** levelData, int levelWidth, int levelHeight);

// Get collision mode from angle (SPG four-mode system)
Directions GetCollisionModeFromAngle(uint8_t angle);

// Convert angle byte (0-255) to radians
float AngleByteToRadians(uint8_t angleByte);

// Convert angle byte to degrees
float AngleByteToDegrees(uint8_t angleByte);

//Get the calculated sensor positions for the player based on their current mode.
void GetPlayerSensorPositions(Player *player, PlayerSensors *sensors);

// Check two sensors, and returns the closer colliding of the two, or a zero-initialized SensorResult if neither collide.
SensorResult CheckCloserSensor(Vector2 *playerPos, Vector2 *sensorAPos, Vector2 *sensorBPos,
                                 SensorResult* outSensorA, SensorResult* outSensorB);

//Check any sensor based on an origin position and a target position.
SensorResult CheckAnySensor(Vector2 *origin, Vector2 *target);

// Get height at a specific X position within a tile
int GetTileHeightAtX(int tileId, int localX, bool flipH, bool flipV);

// Get width at a specific Y position within a tile
int GetTileWidthAtY(int tileId, int localY, bool flipH, bool flipV);

// Get tile at world position (handles bounds checking)
int GetTileAtPosition(int worldX, int worldY, bool* flipH, bool* flipV);

// Check if a tile is solid (non-zero)
bool IsTileSolid(int tileId);

#endif // PLAYER_COLLISION_H
