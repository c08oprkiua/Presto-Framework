// Player Collision - SPG-accurate collision detection implementation
#include "player-collision.h"
#include "../../util/util-global.h"
#include "entity/player/player-player.h"
#include <math.h>
#include <raymath.h>

// Tiled flip flags
#define FLIPPED_HORIZONTALLY_FLAG 0x80000000
#define FLIPPED_VERTICALLY_FLAG   0x40000000
#define FLIPPED_DIAGONALLY_FLAG   0x20000000
#define TILE_ID_MASK              0x1FFFFFFF

// Global level collision data
LevelCollision g_LevelCollision = {NULL, 0, 0};

// Initialize collision system
void InitCollisionSystem(int** levelData, int levelWidth, int levelHeight) {
    g_LevelCollision.tileData = levelData;
    g_LevelCollision.width = levelWidth;
    g_LevelCollision.height = levelHeight;
}

// Get collision mode from angle (SPG four-mode system)
// Angles are 0-255 where 0=flat ground, 64=right wall, 128=ceiling, 192=left wall
Directions GetCollisionModeFromAngle(uint8_t angle) {
    // Convert to degrees for easier understanding: angle * 360 / 256 = angle * 1.40625
    // Floor: 0-45° (0-32) and 315-360° (224-255)
    // Right Wall: 46-134° (33-95)
    // Ceiling: 135-225° (96-160)
    // Left Wall: 226-314° (161-223)

    if (angle <= 32 || angle >= 224) {
        return DOWN;
    } else if (angle >= 33 && angle <= 95) {
        return RIGHT;
    } else if (angle >= 96 && angle <= 160) {
        return UP;
    } else {
        return LEFT;
    }
}

// Convert angle byte (0-255) to radians
float AngleByteToRadians(uint8_t angleByte) {
    return (angleByte / 256.0f) * 2.0f * PI;
}

// Convert angle byte to degrees
float AngleByteToDegrees(uint8_t angleByte) {
    return (angleByte / 256.0f) * 360.0f;
}

void GetPlayerSensorPositions(Player *player, PlayerSensors *sensors){

    //TODO: Wall mode ceiling sensors
    switch (player->collisionMode){
        case NONE:
        case DOWN:{
            // A is left, B is right, both at bottom
            sensors->groundA = (Vector2){
                player->position.x - player->widthRadius,
                player->position.y + player->heightRadius
            };
            sensors->groundB = (Vector2){
                player->position.x + player->widthRadius,
                player->position.y + player->heightRadius
            };

            sensors->pushE = (Vector2){
                player->position.x - player->pushRadius,
                player->position.y
            };
            sensors->pushF = (Vector2){
                player->position.x + player->pushRadius,
                player->position.y
            };

            sensors->ceilingC = (Vector2){
                player->position.x - player->widthRadius,
                player->position.y - player->heightRadius
            };
            sensors->ceilingD = (Vector2){
                player->position.x + player->widthRadius,
                player->position.y - player->heightRadius
            };
        }; break;
        case UP:{
            // A is left, B is right, both at bottom
            sensors->groundA = (Vector2){
                player->position.x + player->widthRadius,
                player->position.y - player->heightRadius
            };
            sensors->groundB = (Vector2){
                player->position.x - player->widthRadius,
                player->position.y - player->heightRadius
            };

            sensors->pushE = (Vector2){
                player->position.x + player->pushRadius,
                player->position.y
            };
            sensors->pushF = (Vector2){
                player->position.x - player->pushRadius,
                player->position.y
            };

            sensors->ceilingC = (Vector2){
                player->position.x + player->widthRadius,
                player->position.y + player->heightRadius
            };
            sensors->ceilingD = (Vector2){
                player->position.x - player->widthRadius,
                player->position.y + player->heightRadius
            };

        }; break;
        case RIGHT:{
            // Sensors rotated 90° CW - now on right side
            sensors->groundA = (Vector2){
                player->position.x + player->heightRadius,
                player->position.y + player->widthRadius
            };
            sensors->groundB = (Vector2){
                player->position.x + player->heightRadius,
                player->position.y - player->widthRadius
            };

            sensors->pushE = (Vector2){
                player->position.x,
                player->position.y + player->pushRadius
            };
            sensors->pushF = (Vector2){
                player->position.x,
                player->position.y - player->pushRadius
            };

        }; break;
        case LEFT:{

            // Sensors rotated 270° - now on left side
            sensors->groundA = (Vector2){
                player->position.x - player->heightRadius,
                player->position.y + player->widthRadius
            };
            sensors->groundB = (Vector2){
                player->position.x - player->heightRadius,
                player->position.y - player->widthRadius
            };

            sensors->pushE = (Vector2){
                player->position.x,
                player->position.y - player->pushRadius
            };
            sensors->pushF = (Vector2){
                player->position.x,
                player->position.y + player->pushRadius
            };

        }; break;
    }
}

//GetTileAtPosition but it uses tile coordinates instead of raw world coordinates
static int LocalGetTileAtPosition(int tileX, int tileY, bool* flipH, bool* flipV) {
    if (!g_LevelCollision.tileData) return 0;

    // Bounds check
    if (tileX < 0 || tileX >= g_LevelCollision.width ||
        tileY < 0 || tileY >= g_LevelCollision.height) {
        if (flipH) *flipH = false;
        if (flipV) *flipV = false;
        return 0;
    }

    uint32_t rawValue = (uint32_t)g_LevelCollision.tileData[tileY][tileX];

    // Extract flip flags
    if (flipH) *flipH = (rawValue & FLIPPED_HORIZONTALLY_FLAG) != 0;
    if (flipV) *flipV = (rawValue & FLIPPED_VERTICALLY_FLAG) != 0;

    // Return tile ID
    return (int)(rawValue & TILE_ID_MASK);
}

// Get tile at world position with flip flags
int GetTileAtPosition(int worldX, int worldY, bool* flipH, bool* flipV) {
    int tileX = worldX / TILE_SIZE;
    int tileY = worldY / TILE_SIZE;

    return LocalGetTileAtPosition(tileX, tileY, flipH, flipV);
}

// Check if tile is solid. This is a bounds check; any tile that is outside of the tileset size is non-solid (empty).
bool IsTileSolid(int tileId) {
    return tileId > 0 && tileId < TILESET_TILE_COUNT;
}

// Get height at X position within a tile (for floor/ceiling collision)
int GetTileHeightAtX(int tileId, int localX, bool flipH, bool flipV) {
    if (!IsTileSolid(tileId)) return 0;

    // Handle horizontal flip - mirror the X coordinate
    int x = Clamp(flipH ? (TILE_SIZE - 1 - localX) : localX, 0.0f, TILE_SIZE - 1);

    int height = TILESET_HEIGHTMAPS[tileId][x];

    // Handle vertical flip - invert height
    if (flipV && height > 0) {
        height = TILE_SIZE - height;
    }

    return height;
}

// Get width at Y position within a tile (for wall collision)
int GetTileWidthAtY(int tileId, int localY, bool flipH, bool flipV) {
    if (!IsTileSolid(tileId)) return 0;

    // Handle vertical flip - mirror the Y coordinate
    int y = Clamp(flipV ? (TILE_SIZE - 1 - localY) : localY, 0, TILE_SIZE - 1);

    int width = TILESET_WIDTHMAPS[tileId][y];

    // Handle horizontal flip - invert width
    if (flipH && width > 0) {
        width = TILE_SIZE - width;
    }

    return width;
}

// Core sensor check for floor mode (downward-pointing sensor)
static SensorResult CheckFloorSensor(Vector2 sensorPos) {
    SensorResult result = {0};
    result.found = false;
    result.distance = TILE_SIZE; // Default to "not found" distance

    // Get the tile we're in
    int tileX = (int)sensorPos.x / TILE_SIZE;
    int tileY = (int)sensorPos.y / TILE_SIZE;
    int localX = (int)sensorPos.x % TILE_SIZE;
    int localY = (int)sensorPos.y % TILE_SIZE;

    // Handle negative coordinates
    if (sensorPos.x < 0) {
        tileX--;
        localX = TILE_SIZE + ((int)sensorPos.x % TILE_SIZE);
        if (localX == TILE_SIZE) { localX = 0; tileX++; }
    }
    if (sensorPos.y < 0) {
        tileY--;
        localY = TILE_SIZE + ((int)sensorPos.y % TILE_SIZE);
        if (localY == TILE_SIZE) { localY = 0; tileY++; }
    }

    bool flipH, flipV;
    int tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int height = GetTileHeightAtX(tileId, localX, flipH, flipV);

        if (height > 0) {
            // Calculate distance from sensor to surface
            // Height is measured from bottom of tile
            int surfaceY;
            if (flipV) {
                // Flipped vertically - height is from top
                surfaceY = tileY * TILE_SIZE + height;
            } else {
                // Normal - height is from bottom
                surfaceY = (tileY + 1) * TILE_SIZE - height;
            }

            result.distance = surfaceY - (int)sensorPos.y;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = GetTileAngle(tileId);
            result.globalContactPoint = (Vector2){sensorPos.x, (float)surfaceY};

            // If height is 16 (full tile), check tile above for regression
            if (height == TILE_SIZE && result.distance >= 0) {
                // Check tile above
                int aboveTileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y - TILE_SIZE, &flipH, &flipV);
                if (IsTileSolid(aboveTileId)) {
                    int aboveHeight = GetTileHeightAtX(aboveTileId, localX, flipH, flipV);
                    if (aboveHeight > 0) {
                        int aboveSurfaceY;
                        if (flipV) {
                            aboveSurfaceY = (tileY - 1) * TILE_SIZE + aboveHeight;
                        } else {
                            aboveSurfaceY = tileY * TILE_SIZE - aboveHeight;
                        }
                        result.distance = aboveSurfaceY - (int)sensorPos.y;
                        result.tileY = tileY - 1;
                        result.tileId = aboveTileId;
                        result.angle = GetTileAngle(aboveTileId);
                        result.globalContactPoint.y = (float)aboveSurfaceY;
                    }
                }
            }

            return result;
        }
    }

    // No solid tile at sensor position - check tile below (extension)
    tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y + TILE_SIZE, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int height = GetTileHeightAtX(tileId, localX, flipH, flipV);

        if (height > 0) {
            int surfaceY;
            if (flipV) {
                surfaceY = (tileY + 1) * TILE_SIZE + height;
            } else {
                surfaceY = (tileY + 2) * TILE_SIZE - height;
            }

            result.distance = surfaceY - (int)sensorPos.y;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY + 1;
            result.tileId = tileId;
            result.angle = GetTileAngle(tileId);
            result.globalContactPoint = (Vector2){sensorPos.x, (float)surfaceY};
        }
    }

    return result;
}

// Core sensor check for ceiling mode (upward-pointing sensor)
static SensorResult CheckCeilingSensor(Vector2 sensorPos) {
    SensorResult result = {0};
    result.found = false;
    result.distance = TILE_SIZE;

    int tileX = (int)sensorPos.x / TILE_SIZE;
    int tileY = (int)sensorPos.y / TILE_SIZE;
    int localX = (int)sensorPos.x % TILE_SIZE;

    if (sensorPos.x < 0) {
        tileX--;
        localX = TILE_SIZE + ((int)sensorPos.x % TILE_SIZE);
        if (localX == TILE_SIZE) { localX = 0; tileX++; }
    }
    if (sensorPos.y < 0) {
        tileY--;
    }

    bool flipH, flipV;
    int tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int height = GetTileHeightAtX(tileId, localX, flipH, flipV);

        if (height > 0) {
            // For ceiling, we check from below
            int surfaceY;
            if (flipV) {
                surfaceY = (tileY + 1) * TILE_SIZE - height;
            } else {
                surfaceY = tileY * TILE_SIZE + height;
            }

            result.distance = (int)sensorPos.y - surfaceY;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = GetTileAngle(tileId);
            result.globalContactPoint = (Vector2){sensorPos.x, (float)surfaceY};

            return result;
        }
    }

    // Check tile above for extension
    tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y - TILE_SIZE, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int height = GetTileHeightAtX(tileId, localX, flipH, flipV);

        if (height > 0) {
            int surfaceY;
            if (flipV) {
                surfaceY = tileY * TILE_SIZE - height;
            } else {
                surfaceY = (tileY - 1) * TILE_SIZE + height;
            }

            result.distance = (int)sensorPos.y - surfaceY;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY - 1;
            result.tileId = tileId;
            result.angle = GetTileAngle(tileId);
            result.globalContactPoint = (Vector2){sensorPos.x, (float)surfaceY};
        }
    }

    return result;
}

// Core sensor check for right wall mode (rightward-pointing sensor)
static SensorResult CheckRightWallSensor(Vector2 sensorPos) {
    SensorResult result = {0};
    result.found = false;
    result.distance = TILE_SIZE;

    int tileX = (int)sensorPos.x / TILE_SIZE;
    int tileY = (int)sensorPos.y / TILE_SIZE;
    int localY = (int)sensorPos.y % TILE_SIZE;

    if (sensorPos.x < 0) {
        tileX--;
    }
    if (sensorPos.y < 0) {
        tileY--;
        localY = TILE_SIZE + ((int)sensorPos.y % TILE_SIZE);
        if (localY == TILE_SIZE) { localY = 0; tileY++; }
    }

    bool flipH, flipV;
    int tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int width = GetTileWidthAtY(tileId, localY, flipH, flipV);

        if (width > 0) {
            // Width is from left side of tile
            int surfaceX = tileX * TILE_SIZE + width;

            if (flipH) {
                surfaceX = (tileX + 1) * TILE_SIZE - width;
            }

            result.distance = surfaceX - (int)sensorPos.x;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = (uint8_t)TILESET_WIDTH_ANGLES[tileId];
            result.globalContactPoint = (Vector2){(float)surfaceX, sensorPos.y};

            return result;
        }
    }

    // Check tile to the right for extension
    tileId = GetTileAtPosition((int)sensorPos.x + TILE_SIZE, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int width = GetTileWidthAtY(tileId, localY, flipH, flipV);

        if (width > 0) {
            int surfaceX = (tileX + 1) * TILE_SIZE + width;

            if (flipH) {
                surfaceX = (tileX + 2) * TILE_SIZE - width;
            }

            result.distance = surfaceX - (int)sensorPos.x;
            result.found = true;
            result.tileX = tileX + 1;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = (uint8_t)TILESET_WIDTH_ANGLES[tileId];
            result.globalContactPoint = (Vector2){(float)surfaceX, sensorPos.y};
        }
    }

    return result;
}

// Core sensor check for left wall mode (leftward-pointing sensor)
static SensorResult CheckLeftWallSensor(Vector2 sensorPos) {
    SensorResult result = {0};
    result.found = false;
    result.distance = TILE_SIZE;

    int tileX = (int)sensorPos.x / TILE_SIZE;
    int tileY = (int)sensorPos.y / TILE_SIZE;
    int localY = (int)sensorPos.y % TILE_SIZE;

    if (sensorPos.x < 0) {
        tileX--;
    }
    if (sensorPos.y < 0) {
        tileY--;
        localY = TILE_SIZE + ((int)sensorPos.y % TILE_SIZE);
        if (localY == TILE_SIZE) { localY = 0; tileY++; }
    }

    bool flipH, flipV;
    int tileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int width = GetTileWidthAtY(tileId, localY, flipH, flipV);

        if (width > 0) {
            int surfaceX = (tileX + 1) * TILE_SIZE - width;

            if (flipH) {
                surfaceX = tileX * TILE_SIZE + width;
            }

            result.distance = (int)sensorPos.x - surfaceX;
            result.found = true;
            result.tileX = tileX;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = (uint8_t)TILESET_WIDTH_ANGLES[tileId];
            result.globalContactPoint = (Vector2){(float)surfaceX, sensorPos.y};

            return result;
        }
    }

    // Check tile to the left for extension
    tileId = GetTileAtPosition((int)sensorPos.x - TILE_SIZE, (int)sensorPos.y, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        int width = GetTileWidthAtY(tileId, localY, flipH, flipV);

        if (width > 0) {
            int surfaceX = tileX * TILE_SIZE - width;

            if (flipH) {
                surfaceX = (tileX - 1) * TILE_SIZE + width;
            }

            result.distance = (int)sensorPos.x - surfaceX;
            result.found = true;
            result.tileX = tileX - 1;
            result.tileY = tileY;
            result.tileId = tileId;
            result.angle = (uint8_t)TILESET_WIDTH_ANGLES[tileId];
            result.globalContactPoint = (Vector2){(float)surfaceX, sensorPos.y};
        }
    }

    return result;
}

// Generic sensor check based on collision mode
SensorResult CheckSensor(Vector2 position, Vector2 direction, Directions mode) {
    switch (mode) {
        case DOWN:
            return CheckFloorSensor(position);
        case UP:
            return CheckCeilingSensor(position);
        case RIGHT:
            return CheckRightWallSensor(position);
        case LEFT:
            return CheckLeftWallSensor(position);
        default:
            return CheckFloorSensor(position);
    }
}

// Check ground sensors A and B, return the winning result
SensorResult CheckGroundSensors(Vector2 *playerPos, Vector2 *sensorAPos, Vector2 *sensorBPos,
                                 SensorResult* outSensorA, SensorResult* outSensorB) {

    // Perform sensor checks
    SensorResult resultA = CheckAnySensor(playerPos, sensorAPos, DOWN);
    SensorResult resultB = CheckAnySensor(playerPos, sensorBPos, DOWN);

    // Output individual results if requested
    if (outSensorA) *outSensorA = resultA;
    if (outSensorB) *outSensorB = resultB;

    // Determine winner - smallest distance wins, A wins ties
    if (!resultA.found && !resultB.found) {
        // Neither found anything
        SensorResult empty = {0};
        empty.found = false;
        empty.distance = TILE_SIZE * 2; // Max distance
        return empty;
    }

    if (!resultA.found) return resultB;
    if (!resultB.found) return resultA;

    // Both found - compare distances (SPG: if equal, A wins)
    if (resultA.distance <= resultB.distance) {
        return resultA;
    }
    return resultB;
}

// Check ceiling sensors C and D
SensorResult CheckCeilingSensors(Vector2 playerPos, float widthRadius, float heightRadius,
                                  Directions mode, uint8_t currentAngle,
                                  SensorResult* outSensorC, SensorResult* outSensorD) {
    Vector2 sensorCPos, sensorDPos;

    switch (mode) {
        case DOWN:
            // C is left, D is right, both at top
            sensorCPos = (Vector2){
                playerPos.x - widthRadius,
                playerPos.y - heightRadius
            };
            sensorDPos = (Vector2){
                playerPos.x + widthRadius,
                playerPos.y - heightRadius
            };
            break;

        case RIGHT:
            sensorCPos = (Vector2){
                playerPos.x - heightRadius,
                playerPos.y - widthRadius
            };
            sensorDPos = (Vector2){
                playerPos.x - heightRadius,
                playerPos.y + widthRadius
            };
            break;

        case UP:
            sensorCPos = (Vector2){
                playerPos.x + widthRadius,
                playerPos.y + heightRadius
            };
            sensorDPos = (Vector2){
                playerPos.x - widthRadius,
                playerPos.y + heightRadius
            };
            break;

        case LEFT:
            sensorCPos = (Vector2){
                playerPos.x + heightRadius,
                playerPos.y + widthRadius
            };
            sensorDPos = (Vector2){
                playerPos.x + heightRadius,
                playerPos.y - widthRadius
            };
            break;
    }

    // Check as ceiling sensors (upward)
    Directions ceilingMode;
    switch (mode) {
        case DOWN: ceilingMode = UP; break;
        case RIGHT: ceilingMode = LEFT; break;
        case UP: ceilingMode = DOWN; break;
        case LEFT: ceilingMode = RIGHT; break;
        default: ceilingMode = UP; break;
    }

    SensorResult resultC = CheckSensor(sensorCPos, (Vector2){0, -1}, ceilingMode);
    SensorResult resultD = CheckSensor(sensorDPos, (Vector2){0, -1}, ceilingMode);

    if (outSensorC) *outSensorC = resultC;
    if (outSensorD) *outSensorD = resultD;

    if (!resultC.found && !resultD.found) {
        SensorResult empty = {0};
        empty.found = false;
        empty.distance = TILE_SIZE * 2;
        return empty;
    }

    if (!resultC.found) return resultD;
    if (!resultD.found) return resultC;

    if (resultC.distance <= resultD.distance) {
        return resultC;
    }
    return resultD;
}

// Check wall/push sensors E and F
void CheckWallSensors(Vector2 playerPos, Vector2 *sensorEPos, Vector2 *sensorFPos,
                      SensorResult* outSensorE, SensorResult* outSensorF) {

    // E checks left wall, F checks right wall
    //SensorResult resultE = CheckLeftWallSensor(sensorEPos);
    //SensorResult resultF = CheckRightWallSensor(sensorFPos);
    SensorResult resultE = CheckAnySensor(&playerPos, sensorEPos, LEFT);
    SensorResult resultF = CheckAnySensor(&playerPos, sensorFPos, RIGHT);

    if (outSensorE) *outSensorE = resultE;
    if (outSensorF) *outSensorF = resultF;
}

//Origin is an absolute coordinate, target is relative to origin, mode is which directions to check
SensorResult CheckAnySensor(Vector2 *origin, Vector2 *target, Directions mode){
    SensorResult result = {0};
    result.found = false;
    result.distance = TILE_SIZE; // Default to "not found" distance

    Vector2 sensorPos = Vector2Add(*origin, *target);

    // Get the coordinates to the tile we're in
    int tileCoordX = (int) sensorPos.x / TILE_SIZE;
    int tileCoordY = (int) sensorPos.y / TILE_SIZE;

    bool flipH, flipV;
    int tileId = LocalGetTileAtPosition(tileCoordX, tileCoordY, &flipH, &flipV);

    if (IsTileSolid(tileId)) {
        if (mode & ANY_Y){
            // local offsets of the sensor within the tile
            int localX = (int) sensorPos.x % TILE_SIZE;

            // Handle negative coordinates
            if (sensorPos.x < 0) {
                tileCoordX -= 1;
                localX += TILE_SIZE;

                if (localX == TILE_SIZE) { localX = 0; tileCoordX += 1; }
            }

            int tileHeight = GetTileHeightAtX(tileId, localX, flipH, flipV);

            if (tileHeight > 0) {
                // Calculate distance from sensor to surface
                // Height is measured from bottom of tile

                int surfaceY;
                if (flipV) {
                    // Flipped vertically - height is from top
                    surfaceY = tileCoordY * TILE_SIZE + tileHeight;

                } else {
                    // Normal - height is from bottom
                    surfaceY = (tileCoordY + 1) * TILE_SIZE - tileHeight;
                }

                result.distance = surfaceY - (int)sensorPos.y;
                result.found = true;
                result.tileX = tileCoordX;
                result.tileY = tileCoordY;
                result.tileId = tileId;
                result.angle = GetTileAngle(tileId);

                result.globalContactPoint = (Vector2){sensorPos.x, (float)surfaceY};

                //if height is 16, make sure the target isn't clipped *into* the tiles by checking the
                //contextually next-closest tile to the source position.
                if (tileHeight == TILE_SIZE && result.distance >= 0) {
                    int nextTileID;
                    int nextTileHeight;


                    if (mode & UP){
                        //Check the tile above the player in space, since they're on the ceiling
                        nextTileID = LocalGetTileAtPosition(tileCoordX, tileCoordY - 1, &flipH, &flipV);
                        nextTileHeight = GetTileHeightAtX(nextTileID, localX, flipH, flipV);
                    }
                    if (mode & DOWN){
                        //Check the tile below the player in space, since they're on the ground
                        nextTileID = LocalGetTileAtPosition(tileCoordX, tileCoordY + 1, &flipH, &flipV);
                        nextTileHeight = GetTileHeightAtX(nextTileID, localX, flipH, flipV);
                    }

                    if (nextTileHeight > 0) {
                        int aboveSurfaceY;
                        if (flipV) {
                            aboveSurfaceY = (tileCoordY - 1) * TILE_SIZE + nextTileHeight;
                        } else {
                            aboveSurfaceY = tileCoordY * TILE_SIZE - nextTileHeight;
                        }
                        result.distance = aboveSurfaceY - (int)sensorPos.y;
                        result.tileY = tileCoordY - 1;
                        result.tileId = nextTileID;
                        result.angle = GetTileAngle(nextTileID);
                        result.globalContactPoint.y = (float)aboveSurfaceY;
                    }
                }

                // If height is 16 (full tile), check tile above for regression
                if (tileHeight == TILE_SIZE && result.distance >= 0) {
                    // Check tile above
                    int aboveTileId = GetTileAtPosition((int)sensorPos.x, (int)sensorPos.y - TILE_SIZE, &flipH, &flipV);

                    if (IsTileSolid(aboveTileId)) {

                        int aboveHeight = GetTileHeightAtX(aboveTileId, localX, flipH, flipV);
                        if (aboveHeight > 0) {
                            int aboveSurfaceY;
                            if (flipV) {
                                aboveSurfaceY = (tileCoordY - 1) * TILE_SIZE + aboveHeight;
                            } else {
                                aboveSurfaceY = tileCoordY * TILE_SIZE - aboveHeight;
                            }
                            result.distance = aboveSurfaceY - (int)sensorPos.y;
                            result.tileY = tileCoordY - 1;
                            result.tileId = aboveTileId;
                            result.angle = GetTileAngle(aboveTileId);
                            result.globalContactPoint.y = (float)aboveSurfaceY;
                        }
                    }
                }

                return result;
            }

        }

        if(mode & ANY_X){
            // local offsets of the sensor within the tile
            int localY = (int) sensorPos.y % TILE_SIZE;

            // Handle negative coordinates
            if (sensorPos.y < 0) {
                tileCoordX -= 1;
                localY += TILE_SIZE;

                if (localY == TILE_SIZE) { localY = 0; tileCoordY += 1; }
            }


        }

    }

    return result;
}
