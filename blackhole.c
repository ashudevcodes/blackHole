#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define MAX_STARS 1000

// Black hole parameters
typedef struct {
  Vector3 position;
  float mass;
  float schwarzschildRadius;
  float iscoRadius; // Innermost Stable Circular Orbit
} BlackHole;

// Star structure for background
typedef struct {
  Vector3 position;
  float brightness;
  Color color;
} Star;

// Accretion disk parameters
typedef struct {
  float innerRadius;
  float outerRadius;
  float rotationSpeed;
  float temperature;
  Color hotColor;
  Color coolColor;
} AccretionDisk;

// Camera and simulation state
typedef struct {
  Camera3D camera;
  BlackHole blackHole;
  AccretionDisk disk;
  Star stars[MAX_STARS];
  float time;
  float timeDilation;
  bool showLensing;
  bool showDisk;
  bool showTimeEffects;
} SimulationState;

// Shader strings
const char *lensVertexShader =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "out vec2 fragTexCoord;\n"
    "void main() {\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

const char *lensFragmentShader =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 lensCenter;\n"
    "uniform float lensStrength;\n"
    "uniform vec2 screenSize;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 uv = fragTexCoord;\n"
    "    vec2 screenPos = uv * screenSize;\n"
    "    vec2 delta = screenPos - lensCenter;\n"
    "    float r = length(delta);\n"
    "    \n"
    "    if (r > 0.0) {\n"
    "        float deflection = lensStrength / r;\n"
    "        float newR = r - deflection;\n"
    "        if (newR > 0.0) {\n"
    "            vec2 newDelta = normalize(delta) * newR;\n"
    "            vec2 newScreenPos = lensCenter + newDelta;\n"
    "            vec2 newUV = newScreenPos / screenSize;\n"
    "            \n"
    "            if (newUV.x >= 0.0 && newUV.x <= 1.0 && newUV.y >= 0.0 && "
    "newUV.y <= 1.0) {\n"
    "                finalColor = texture(texture0, newUV);\n"
    "            } else {\n"
    "                finalColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "            }\n"
    "        } else {\n"
    "            finalColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "        }\n"
    "    } else {\n"
    "        finalColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    }\n"
    "}\n";

const char *diskVertexShader =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "out vec2 fragTexCoord;\n"
    "out vec3 worldPos;\n"
    "void main() {\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    worldPos = vertexPosition;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

const char *diskFragmentShader =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 worldPos;\n"
    "uniform float time;\n"
    "uniform vec3 blackHolePos;\n"
    "uniform float innerRadius;\n"
    "uniform float outerRadius;\n"
    "uniform vec3 hotColor;\n"
    "uniform vec3 coolColor;\n"
    "out vec4 finalColor;\n"
    "\n"
    "float noise(vec2 p) {\n"
    "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 pos = worldPos.xz - blackHolePos.xz;\n"
    "    float r = length(pos);\n"
    "    float angle = atan(pos.y, pos.x);\n"
    "    \n"
    "    if (r < innerRadius || r > outerRadius) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    // Temperature gradient\n"
    "    float temp = 1.0 - (r - innerRadius) / (outerRadius - innerRadius);\n"
    "    temp = pow(temp, 0.5);\n"
    "    \n"
    "    // Rotation and turbulence\n"
    "    float rotSpeed = 1.0 / sqrt(r);\n"
    "    angle += time * rotSpeed;\n"
    "    \n"
    "    // Noise for turbulence\n"
    "    vec2 noiseCoord = vec2(angle * 3.0, r * 0.1) + vec2(time * 0.1, "
    "0.0);\n"
    "    float turbulence = noise(noiseCoord) * 0.5 + noise(noiseCoord * 2.0) "
    "* 0.25;\n"
    "    \n"
    "    // Doppler effect (simplified)\n"
    "    float dopplerShift = sin(angle) * 0.3;\n"
    "    \n"
    "    vec3 color = mix(coolColor, hotColor, temp + turbulence * 0.3);\n"
    "    color *= (1.0 + dopplerShift);\n"
    "    \n"
    "    float alpha = temp * (0.7 + turbulence * 0.3);\n"
    "    finalColor = vec4(color, alpha);\n"
    "}\n";

// Function prototypes
void InitSimulation(SimulationState *sim);
void UpdateSimulation(SimulationState *sim);
void DrawSimulation(SimulationState *sim);
void DrawStarfield(SimulationState *sim);
void DrawAccretionDisk(SimulationState *sim, Shader diskShader);
void DrawBlackHole(SimulationState *sim);
float CalculateTimeDilation(Vector3 position, BlackHole bh);
Vector3 CartesianToPolar(Vector3 pos);

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Black Hole Simulation - Raylib");
  SetTargetFPS(60);

  SimulationState sim = {0};
  InitSimulation(&sim);

  // Load shaders
  Shader lensShader =
      LoadShaderFromMemory(lensVertexShader, lensFragmentShader);
  Shader diskShader =
      LoadShaderFromMemory(diskVertexShader, diskFragmentShader);

  // Create render texture for lens effect
  RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

  // Get shader locations
  int lensCenterLoc = GetShaderLocation(lensShader, "lensCenter");
  int lensStrengthLoc = GetShaderLocation(lensShader, "lensStrength");
  int screenSizeLoc = GetShaderLocation(lensShader, "screenSize");

  int diskTimeLoc = GetShaderLocation(diskShader, "time");
  int diskBHPosLoc = GetShaderLocation(diskShader, "blackHolePos");
  int diskInnerLoc = GetShaderLocation(diskShader, "innerRadius");
  int diskOuterLoc = GetShaderLocation(diskShader, "outerRadius");
  int diskHotColorLoc = GetShaderLocation(diskShader, "hotColor");
  int diskCoolColorLoc = GetShaderLocation(diskShader, "coolColor");

  while (!WindowShouldClose()) {
    UpdateSimulation(&sim);

    if (IsKeyPressed(KEY_L))
      sim.showLensing = !sim.showLensing;
    if (IsKeyPressed(KEY_O))
      sim.showDisk = !sim.showDisk;
    if (IsKeyPressed(KEY_T))
      sim.showTimeEffects = !sim.showTimeEffects;

    if (IsKeyDown(KEY_W)) {
      Vector3 forward = Vector3Subtract(sim.camera.target, sim.camera.position);
      forward = Vector3Normalize(forward);
      forward = Vector3Scale(forward, 0.5f);
      sim.camera.position = Vector3Add(sim.camera.position, forward);
    }
    if (IsKeyDown(KEY_S)) {
      Vector3 backward =
          Vector3Subtract(sim.camera.position, sim.camera.target);
      backward = Vector3Normalize(backward);
      backward = Vector3Scale(backward, 0.5f);
      sim.camera.position = Vector3Add(sim.camera.position, backward);
    }
    if (IsKeyDown(KEY_A)) {
      Vector3 right = Vector3CrossProduct(
          Vector3Subtract(sim.camera.target, sim.camera.position),
          sim.camera.up);
      right = Vector3Normalize(right);
      right = Vector3Scale(right, -0.5f);
      sim.camera.position = Vector3Add(sim.camera.position, right);
    }
    if (IsKeyDown(KEY_D)) {
      Vector3 right = Vector3CrossProduct(
          Vector3Subtract(sim.camera.target, sim.camera.position),
          sim.camera.up);
      right = Vector3Normalize(right);
      right = Vector3Scale(right, 0.5f);
      sim.camera.position = Vector3Add(sim.camera.position, right);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    if (sim.showLensing) {
      BeginTextureMode(target);
      ClearBackground(BLACK);
      BeginMode3D(sim.camera);
      DrawStarfield(&sim);
      if (sim.showDisk)
        DrawAccretionDisk(&sim, diskShader);
      DrawBlackHole(&sim);
      EndMode3D();
      EndTextureMode();

      // Apply lens shader
      Vector2 lensCenter = {SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
      float lensStrength = sim.blackHole.schwarzschildRadius * 5000.0f;
      Vector2 screenSize = {SCREEN_WIDTH, SCREEN_HEIGHT};

      SetShaderValue(lensShader, lensCenterLoc, &lensCenter,
                     SHADER_UNIFORM_VEC2);
      SetShaderValue(lensShader, lensStrengthLoc, &lensStrength,
                     SHADER_UNIFORM_FLOAT);
      SetShaderValue(lensShader, screenSizeLoc, &screenSize,
                     SHADER_UNIFORM_VEC2);

      BeginShaderMode(lensShader);
      DrawTextureRec(
          target.texture,
          (Rectangle){0, 0, target.texture.width, -target.texture.height},
          (Vector2){0, 0}, WHITE);
      EndShaderMode();
    } else {
      // Normal rendering
      BeginMode3D(sim.camera);
      DrawStarfield(&sim);
      if (sim.showDisk) {
        // Set disk shader uniforms
        SetShaderValue(diskShader, diskTimeLoc, &sim.time,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(diskShader, diskBHPosLoc, &sim.blackHole.position,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(diskShader, diskInnerLoc, &sim.disk.innerRadius,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(diskShader, diskOuterLoc, &sim.disk.outerRadius,
                       SHADER_UNIFORM_FLOAT);

        float hotColor[3] = {1.0f, 0.8f, 0.2f};
        float coolColor[3] = {0.8f, 0.2f, 0.1f};
        SetShaderValue(diskShader, diskHotColorLoc, hotColor,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(diskShader, diskCoolColorLoc, coolColor,
                       SHADER_UNIFORM_VEC3);

        DrawAccretionDisk(&sim, diskShader);
      }
      DrawBlackHole(&sim);
      EndMode3D();
    }

    // UI
    DrawText("Black Hole Simulation", 10, 10, 20, WHITE);
    DrawText("L - Toggle Lensing", 10, 40, 16, GRAY);
    DrawText("D - Toggle Accretion Disk", 10, 60, 16, GRAY);
    DrawText("T - Toggle Time Effects", 10, 80, 16, GRAY);
    DrawText("WASD - Move Camera", 10, 100, 16, GRAY);

    if (sim.showTimeEffects) {
      char timeText[64];
      sprintf(timeText, "Time Dilation: %.3f", sim.timeDilation);
      DrawText(timeText, 10, 140, 16, YELLOW);
    }

    char fpsText[32];
    sprintf(fpsText, "FPS: %d", GetFPS());
    DrawText(fpsText, SCREEN_WIDTH - 100, 10, 16, GREEN);

    EndDrawing();
  }

  UnloadRenderTexture(target);
  UnloadShader(lensShader);
  UnloadShader(diskShader);
  CloseWindow();

  return 0;
}

void InitSimulation(SimulationState *sim) {
  // Initialize camera
  sim->camera.position = (Vector3){0.0f, 5.0f, 30.0f};
  sim->camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  sim->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  sim->camera.fovy = 45.0f;
  sim->camera.projection = CAMERA_PERSPECTIVE;

  // Initialize black hole
  sim->blackHole.position = (Vector3){0.0f, 0.0f, 0.0f};
  sim->blackHole.mass = 1.0f;
  sim->blackHole.schwarzschildRadius = 2.0f;
  sim->blackHole.iscoRadius = 6.0f;

  // Initialize accretion disk
  sim->disk.innerRadius = sim->blackHole.iscoRadius;
  sim->disk.outerRadius = 20.0f;
  sim->disk.rotationSpeed = 1.0f;
  sim->disk.hotColor = (Color){255, 200, 50, 255};
  sim->disk.coolColor = (Color){200, 50, 25, 255};

  // Generate random starfield
  for (int i = 0; i < MAX_STARS; i++) {
    sim->stars[i].position = (Vector3){(float)(GetRandomValue(-100, 100)),
                                       (float)(GetRandomValue(-100, 100)),
                                       (float)(GetRandomValue(-100, 100))};
    sim->stars[i].brightness = (float)GetRandomValue(50, 255) / 255.0f;
    sim->stars[i].color =
        (Color){(unsigned char)(sim->stars[i].brightness * 255),
                (unsigned char)(sim->stars[i].brightness * 255),
                (unsigned char)(sim->stars[i].brightness * 255), 255};
  }

  sim->time = 0.0f;
  sim->timeDilation = 1.0f;
  sim->showLensing = true;
  sim->showDisk = true;
  sim->showTimeEffects = true;
}

void UpdateSimulation(SimulationState *sim) {
  float deltaTime = GetFrameTime();

  // Calculate time dilation based on camera distance to black hole
  if (sim->showTimeEffects) {
    sim->timeDilation =
        CalculateTimeDilation(sim->camera.position, sim->blackHole);
    deltaTime *= sim->timeDilation;
  }

  sim->time += deltaTime;
}

void DrawSimulation(SimulationState *sim) {
  BeginMode3D(sim->camera);

  DrawStarfield(sim);
  if (sim->showDisk) {
    // DrawAccretionDisk will be called with shader
  }
  DrawBlackHole(sim);

  EndMode3D();
}

void DrawStarfield(SimulationState *sim) {
  for (int i = 0; i < MAX_STARS; i++) {
    Vector3 pos = sim->stars[i].position;
    float distance = Vector3Distance(pos, sim->blackHole.position);

    // Don't draw stars too close to black hole
    if (distance > sim->blackHole.schwarzschildRadius * 2.0f) {
      DrawCube(pos, 0.2f, 0.2f, 0.2f, sim->stars[i].color);
    }
  }
}

void DrawAccretionDisk(SimulationState *sim, Shader diskShader) {
  BeginShaderMode(diskShader);

  // Draw disk as a large flat cylinder
  DrawCylinderEx(
      (Vector3){sim->blackHole.position.x, sim->blackHole.position.y - 0.1f,
                sim->blackHole.position.z},
      (Vector3){sim->blackHole.position.x, sim->blackHole.position.y + 0.1f,
                sim->blackHole.position.z},
      sim->disk.outerRadius, sim->disk.innerRadius, 32, ORANGE);

  EndShaderMode();
}

void DrawBlackHole(SimulationState *sim) {
  // Draw event horizon as black sphere
  DrawSphere(sim->blackHole.position, sim->blackHole.schwarzschildRadius,
             BLACK);

  // Draw ISCO as faint ring
  DrawCircle3D(sim->blackHole.position, sim->blackHole.iscoRadius,
               (Vector3){1, 0, 0}, 90.0f, Fade(YELLOW, 0.3f));
}

float CalculateTimeDilation(Vector3 position, BlackHole bh) {
  float distance = Vector3Distance(position, bh.position);
  float rs = bh.schwarzschildRadius;

  if (distance <= rs)
    return 0.0f; // At or inside event horizon

  // Simplified gravitational time dilation: sqrt(1 - rs/r)
  float factor = 1.0f - (rs / distance);
  if (factor <= 0.0f)
    return 0.0f;

  return sqrtf(factor);
}

Vector3 CartesianToPolar(Vector3 pos) {
  float r = sqrtf(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
  float theta = atan2f(pos.z, pos.x);
  float phi = acosf(pos.y / r);
  return (Vector3){r, theta, phi};
}
