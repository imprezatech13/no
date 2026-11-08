#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <3ds.h>

#define CLEAR_COLOR 0x68C080FF // Classic 2013 Roblox green-ish sky/background color

static C3D_Mtx projection;
static C3D_Mtx view;
static C3D_Mtx model;

// Game Menu Configuration (Matching the 2013 Roblox Game Menu layout)
int selectedMenuIndex = 0;
#define MENU_ITEMS 7

const char* menuLabels[MENU_ITEMS] = {
    "Resume Game",
    "Reset Character",
    "Game Settings",
    "Help",
    "Screenshot",
    "Record Video",
    "Leave Game"
};

// Player & Camera State
typedef struct {
    float x, y, z;
    float rotY;
    float velocityY;
    bool isGrounded;
} Player;

Player player = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true};
float cameraYaw = 0.0f;
float cameraPitch = 20.0f;

void initGraphics() {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL); // Initializes text console for the bottom screen UI
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    
    C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24);
    C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_RenderTargetRefreshRate(target, 60);
    C3D_SetTarget(target, GFX_TOP, GFX_LEFT);

    Mtx_Identity(&model);
    Mtx_Identity(&view);
    GuPerspective(&projection, 60.0f, (float)400 / 240, 0.1f, 1000.0f);
}

void updateGameLogic() {
    hidScanInput();
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();

    // 1. Bottom Screen Menu Control (D-Pad Up / Down)
    if (kDown & KEY_DUP) {
        selectedMenuIndex = (selectedMenuIndex - 1 + MENU_ITEMS) % MENU_ITEMS;
    }
    if (kDown & KEY_DDOWN) {
        selectedMenuIndex = (selectedMenuIndex + 1 + MENU_ITEMS) % MENU_ITEMS;
    }

    // 2. Walking: Circle Pad Movement
    circlePosition cPos;
    hidCircleRead(&cPos);
    float speed = 0.15f;
    if (abs(cPos.dx) > 15 || abs(cPos.dy) > 15) {
        float rad = cameraYaw * (M_PI / 180.0f);
        float dx = (cPos.dx / 150.0f) * speed;
        float dz = (cPos.dy / 150.0f) * speed;
        
        player.x += dx * cosf(rad) - dz * sinf(rad);
        player.z += dx * sinf(rad) + dz * cosf(rad);
    }

    // 3. Camera: New 3DS C-Stick Rotation
    circlestickPosition cStickPos;
    hidCstickRead(&cStickPos);
    if (abs(cStickPos.dx) > 10) {
        cameraYaw += (cStickPos.dx / 30.0f);
    }
    if (abs(cStickPos.dy) > 10) {
        cameraPitch -= (cStickPos.dy / 30.0f);
        if (cameraPitch < 5.0f) cameraPitch = 5.0f;
        if (cameraPitch > 85.0f) cameraPitch = 85.0f;
    }

    // 4. Jumping: A Button
    if ((kDown & KEY_A) && player.isGrounded) {
        player.velocityY = 0.35f;
        player.isGrounded = false;
    }

    // Apply Gravity & Physics
    if (!player.isGrounded) {
        player.velocityY -= 0.02f; // Gravity constant
        player.y += player.velocityY;
        if (player.y <= 0.0f) {
            player.y = 0.0f;
            player.velocityY = 0.0f;
            player.isGrounded = true;
        }
    }
}

void renderTopScreen3D() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_Mtx mtxView, mtxModel;
    
    Mtx_Identity(&mtxView);
    
    // Position third-person camera behind the blocky avatar
    float camDist = 6.0f;
    float cx = player.x - camDist * sinf(cameraYaw * (M_PI / 180.0f));
    float cz = player.z - camDist * cosf(cameraYaw * (M_PI / 180.0f));
    float cy = player.y + 3.0f;
    
    GuLookAt(&mtxView, 
        &(vector3d){cx, cy, cz}, 
        &(vector3d){player.x, player.y + 1.5f, player.z}, 
        &(vector3d){0.0f, 1.0f, 0.0f});

    C3D_FrameEnd(0);
}

void renderBottomScreenUI() {
    gfxFlushBuffers();
    gspWaitForVBlank();
    gfxSwapBuffers();

    // Render 2013 Roblox Game Menu interface on the bottom screen
    consoleClear();
    printf("\n\x1b[30;47m      === Game Menu ===     \x1b[0m\n\n");
    
    for (int i = 0; i < MENU_ITEMS; i++) {
        if (i == selectedMenuIndex) {
            // Highlight active option with a simulated 2013 Roblox red selection box
            printf(" \x1b[41;1m > %-18s < \x1b[0m\n", menuLabels[i]);
        } else {
            printf("    %-18s   \n", menuLabels[i]);
        }
        printf("\n");
    }
}

int main() {
    initGraphics();

    // Main 3DS Game Loop
    while (aptMainLoop()) {
        updateGameLogic();

        if (hidKeysDown() & KEY_START) break; // Exit application back to Homebrew Menu

        renderTopScreen3D();
        renderBottomScreenUI();
    }

    C3D_Fini();
    gfxExit();
    return 0;
}
