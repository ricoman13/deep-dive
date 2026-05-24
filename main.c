#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W        960
#define SCREEN_H        544
#define WORLD_W         4000
#define WORLD_H         4000

#define PLAYER_SPEED    200.0f
#define OXYGEN_MAX      100.0f
#define OXYGEN_DRAIN    3.5f
#define OXYGEN_REFILL   25.0f
#define DEADZONE        0.12f

#define MAX_RESOURCES   30
#define MAX_OXY_PODS    8
#define MAX_FISH        18
#define MAX_BUBBLES     60
#define MAX_PARTICLES   40

typedef enum { STATE_PLAYING, STATE_GAMEOVER, STATE_WIN } GameState;

typedef struct { float x, y, vx, vy, oxygen; int res[3]; int total; } Player;
typedef struct { float x, y; int type; int alive; } Resource;
typedef struct { float x, y, radius; int alive; } OxyPod;
typedef struct { float x, y, vx, vy, phase; int color; } Fish;
typedef struct { float x, y, vy, alpha; } Bubble;
typedef struct { float x, y, vx, vy, life, maxlife; int r, g, b; } Particle;

static Player      player;
static Resource    resources[MAX_RESOURCES];
static OxyPod      oxypods[MAX_OXY_PODS];
static Fish        fish[MAX_FISH];
static Bubble      bubbles[MAX_BUBBLES];
static Particle    particles[MAX_PARTICLES];
static GameState   state;
static float       cam_x, cam_y;
static int         flash_timer;

static void spawn_particle(float x, float y, int r, int g, int b) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0) {
            particles[i].x    = x;
            particles[i].y    = y;
            particles[i].vx   = ((rand() % 200) - 100) * 0.01f;
            particles[i].vy   = ((rand() % 200) - 100) * 0.01f;
            particles[i].life = particles[i].maxlife = 0.6f;
            particles[i].r = r; particles[i].g = g; particles[i].b = b;
            break;
        }
    }
}

static void init_world(void) {
    srand(42);
    for (int i = 0; i < MAX_RESOURCES; i++) {
        resources[i].x     = 250 + rand() % (WORLD_W - 500);
        resources[i].y     = 250 + rand() % (WORLD_H - 500);
        resources[i].type  = rand() % 3;
        resources[i].alive = 1;
    }
    for (int i = 0; i < MAX_OXY_PODS; i++) {
        oxypods[i].x      = 300 + rand() % (WORLD_W - 600);
        oxypods[i].y      = 300 + rand() % (WORLD_H - 600);
        oxypods[i].radius = 70.0f;
        oxypods[i].alive  = 1;
    }
    for (int i = 0; i < MAX_FISH; i++) {
        fish[i].x     = rand() % WORLD_W;
        fish[i].y     = rand() % WORLD_H;
        fish[i].vx    = (float)((rand() % 120) - 60);
        fish[i].vy    = 0;
        fish[i].phase = (rand() % 628) * 0.01f;
        fish[i].color = rand() % 4;
    }
    for (int i = 0; i < MAX_BUBBLES; i++) {
        bubbles[i].x     = rand() % WORLD_W;
        bubbles[i].y     = rand() % WORLD_H;
        bubbles[i].vy    = -(15.0f + rand() % 35);
        bubbles[i].alpha = 0.25f + (rand() % 60) * 0.01f;
    }
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;
}

static void init_player(void) {
    player.x      = WORLD_W * 0.5f;
    player.y      = WORLD_H * 0.5f;
    player.vx     = player.vy = 0;
    player.oxygen = OXYGEN_MAX;
    player.res[0] = player.res[1] = player.res[2] = 0;
    player.total  = 0;
    cam_x = player.x - SCREEN_W * 0.5f;
    cam_y = player.y - SCREEN_H * 0.5f;
    flash_timer   = 0;
}

static void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void draw_background(SDL_Renderer *r) {
    for (int y = 0; y < SCREEN_H; y += 6) {
        float depth = (cam_y + y) / (float)WORLD_H;
        if (depth < 0) depth = 0;
        if (depth > 1) depth = 1;
        Uint8 blue  = (Uint8)(55 + (1.0f - depth) * 45);
        Uint8 green = (Uint8)(70 + (1.0f - depth) * 30);
        SDL_SetRenderDrawColor(r, 4, green, blue, 255);
        SDL_RenderDrawLine(r, 0, y, SCREEN_W, y);
    }
}

static void draw_decorations(SDL_Renderer *r, float t) {
    for (int i = 0; i < 50; i++) {
        int wx = (i * 79 + 30) % WORLD_W;
        int sx = wx - (int)cam_x;
        if (sx < -30 || sx > SCREEN_W + 30) continue;

        int base_wy = WORLD_H - 80;
        int sy = base_wy - (int)cam_y;
        if (sy < -100 || sy > SCREEN_H + 20) continue;

        float sway = sinf(t * 1.2f + i * 0.7f) * 6.0f;
        SDL_SetRenderDrawColor(r, 25, 140 + (i % 60), 70, 210);
        for (int seg = 0; seg < 6; seg++) {
            SDL_Rect s = {
                sx + (int)(sway * seg * 0.18f) - 3,
                sy - seg * 11,
                7, 12
            };
            SDL_RenderFillRect(r, &s);
        }
    }
    for (int i = 0; i < 25; i++) {
        int wx = (i * 163 + 80) % WORLD_W;
        int sx = wx - (int)cam_x;
        if (sx < -40 || sx > SCREEN_W + 40) continue;

        int wy = WORLD_H - 50;
        int sy = wy - (int)cam_y;
        if (sy < -60 || sy > SCREEN_H + 30) continue;

        Uint8 cr = 170 + (i * 37) % 85;
        Uint8 cg = 40 + (i * 73) % 70;
        Uint8 cb = 90 + (i * 53) % 110;
        SDL_SetRenderDrawColor(r, cr, cg, cb, 220);
        draw_filled_circle(r, sx, sy, 12 + i % 14);
    }
}

static void draw_world(SDL_Renderer *r, float t) {
    draw_background(r);

    for (int i = 0; i < MAX_BUBBLES; i++) {
        int bsx = (int)(bubbles[i].x - cam_x);
        int bsy = (int)(bubbles[i].y - cam_y);
        if (bsx < -6 || bsx > SCREEN_W + 6 || bsy < -6 || bsy > SCREEN_H + 6) continue;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 190, 230, 255, (Uint8)(bubbles[i].alpha * 190));
        draw_filled_circle(r, bsx, bsy, 3);
    }

    for (int i = 0; i < MAX_OXY_PODS; i++) {
        if (!oxypods[i].alive) continue;
        int osx = (int)(oxypods[i].x - cam_x);
        int osy = (int)(oxypods[i].y - cam_y);
        if (osx < -100 || osx > SCREEN_W + 100 || osy < -100 || osy > SCREEN_H + 100) continue;

        float pulse = 0.65f + 0.35f * sinf(t * 2.2f + i);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 80, 190, 255, (Uint8)(55 * pulse));
        draw_filled_circle(r, osx, osy, (int)(oxypods[i].radius * pulse));
        SDL_SetRenderDrawColor(r, 140, 220, 255, 210);
        draw_filled_circle(r, osx, osy, 14);
        SDL_SetRenderDrawColor(r, 220, 250, 255, 240);
        draw_filled_circle(r, osx, osy, 7);
    }

    static const int rc[3][3] = {{90, 140, 255}, {80, 210, 90}, {255, 195, 45}};
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!resources[i].alive) continue;
        int rsx = (int)(resources[i].x - cam_x);
        int rsy = (int)(resources[i].y - cam_y);
        if (rsx < -25 || rsx > SCREEN_W + 25 || rsy < -25 || rsy > SCREEN_H + 25) continue;

        int t2 = resources[i].type;
        float bob = sinf(t * 1.8f + i * 1.1f) * 3.0f;
        int by = rsy + (int)bob;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, rc[t2][0], rc[t2][1], rc[t2][2], 55);
        draw_filled_circle(r, rsx, by, 20);
        SDL_SetRenderDrawColor(r, rc[t2][0], rc[t2][1], rc[t2][2], 230);
        SDL_Rect gem = {rsx - 9, by - 9, 18, 18};
        SDL_RenderFillRect(r, &gem);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 110);
        SDL_Rect hi = {rsx - 5, by - 5, 5, 5};
        SDL_RenderFillRect(r, &hi);
    }

    static const int fish_cols[4][3] = {
        {220, 110, 80}, {100, 200, 180}, {200, 180, 80}, {150, 100, 220}
    };
    for (int i = 0; i < MAX_FISH; i++) {
        int fsx = (int)(fish[i].x - cam_x);
        int fsy = (int)(fish[i].y - cam_y);
        if (fsx < -40 || fsx > SCREEN_W + 40 || fsy < -20 || fsy > SCREEN_H + 20) continue;

        int ci = fish[i].color;
        int flip = fish[i].vx >= 0 ? 1 : -1;
        SDL_SetRenderDrawColor(r, fish_cols[ci][0], fish_cols[ci][1], fish_cols[ci][2], 210);
        SDL_Rect body = {fsx - 11 * flip, fsy - 5, 20, 10};
        SDL_RenderFillRect(r, &body);
        SDL_Rect tail = {fsx + 9 * flip, fsy - 7, 8, 14};
        SDL_RenderFillRect(r, &tail);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 140);
        SDL_Rect eye = {fsx - 5 * flip, fsy - 2, 3, 3};
        SDL_RenderFillRect(r, &eye);
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0) continue;
        float a = particles[i].life / particles[i].maxlife;
        int psx = (int)(particles[i].x - cam_x);
        int psy = (int)(particles[i].y - cam_y);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, particles[i].r, particles[i].g, particles[i].b, (Uint8)(200 * a));
        draw_filled_circle(r, psx, psy, (int)(6 * a));
    }

    int psx = SCREEN_W / 2;
    int psy = SCREEN_H / 2;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 40, 190, 170, 255);
    SDL_Rect pbody = {psx - 13, psy - 11, 26, 22};
    SDL_RenderFillRect(r, &pbody);
    SDL_SetRenderDrawColor(r, 130, 220, 255, 240);
    draw_filled_circle(r, psx, psy - 15, 11);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 120);
    SDL_Rect phl = {psx - 4, psy - 21, 5, 5};
    SDL_RenderFillRect(r, &phl);
    SDL_SetRenderDrawColor(r, 25, 140, 190, 230);
    SDL_Rect lf = {psx - 21, psy + 4, 11, 6};
    SDL_Rect rf = {psx + 10, psy + 4, 11, 6};
    SDL_RenderFillRect(r, &lf);
    SDL_RenderFillRect(r, &rf);

    draw_decorations(r, t);
}

static void draw_minimap(SDL_Renderer *r) {
    int mmx = SCREEN_W - 132, mmy = SCREEN_H - 132;
    int mmw = 122, mmh = 122;
    float sx = (float)mmw / WORLD_W;
    float sy = (float)mmh / WORLD_H;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 8, 35, 185);
    SDL_Rect bg = {mmx, mmy, mmw, mmh};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 80, 130, 200, 200);
    SDL_RenderDrawRect(r, &bg);

    for (int i = 0; i < MAX_OXY_PODS; i++) {
        if (!oxypods[i].alive) continue;
        int mx = mmx + (int)(oxypods[i].x * sx);
        int my = mmy + (int)(oxypods[i].y * sy);
        SDL_SetRenderDrawColor(r, 80, 200, 255, 200);
        SDL_Rect d = {mx - 2, my - 2, 5, 5};
        SDL_RenderFillRect(r, &d);
    }

    static const int rc[3][3] = {{90, 140, 255}, {80, 210, 90}, {255, 195, 45}};
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!resources[i].alive) continue;
        int mx = mmx + (int)(resources[i].x * sx);
        int my = mmy + (int)(resources[i].y * sy);
        int t = resources[i].type;
        SDL_SetRenderDrawColor(r, rc[t][0], rc[t][1], rc[t][2], 200);
        SDL_RenderDrawPoint(r, mx, my);
    }

    int pmx = mmx + (int)(player.x * sx);
    int pmy = mmy + (int)(player.y * sy);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect pd = {pmx - 2, pmy - 2, 5, 5};
    SDL_RenderFillRect(r, &pd);

    int vx2 = mmx + (int)(cam_x * sx);
    int vy2 = mmy + (int)(cam_y * sy);
    int vw  = (int)(SCREEN_W * sx);
    int vh  = (int)(SCREEN_H * sy);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 80);
    SDL_Rect vr = {vx2, vy2, vw, vh};
    SDL_RenderDrawRect(r, &vr);
}

static void draw_hud(SDL_Renderer *r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 10, 15, 65, 195);
    SDL_Rect bar_bg = {14, 14, 214, 26};
    SDL_RenderFillRect(r, &bar_bg);

    float pct    = player.oxygen / OXYGEN_MAX;
    Uint8 ox_r   = (Uint8)(255 * (1.0f - pct));
    Uint8 ox_g   = (Uint8)(255 * pct);
    SDL_SetRenderDrawColor(r, ox_r, ox_g, 90, 230);
    SDL_Rect bar = {16, 16, (int)(210 * pct), 22};
    SDL_RenderFillRect(r, &bar);

    SDL_SetRenderDrawColor(r, 90, 190, 255, 200);
    draw_filled_circle(r, 8, 30, 7);

    static const int rc[3][3] = {{90, 140, 255}, {80, 210, 90}, {255, 195, 45}};
    for (int i = 0; i < 3; i++) {
        int bx = 16 + i * 75;
        SDL_SetRenderDrawColor(r, rc[i][0], rc[i][1], rc[i][2], 220);
        SDL_Rect icon = {bx, 50, 22, 22};
        SDL_RenderFillRect(r, &icon);
        for (int j = 0; j < player.res[i] && j < 15; j++) {
            SDL_Rect dot = {bx + j * 4, 76, 3, 9};
            SDL_RenderFillRect(r, &dot);
        }
    }

    if (player.oxygen < 25.0f) {
        flash_timer++;
        if ((flash_timer / 14) % 2 == 0) {
            SDL_SetRenderDrawColor(r, 200, 0, 0, 55);
            SDL_Rect fl = {0, 0, SCREEN_W, SCREEN_H};
            SDL_RenderFillRect(r, &fl);
        }
    }

    draw_minimap(r);
}

static void draw_gameover(SDL_Renderer *r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 20, 190);
    SDL_Rect ov = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(r, &ov);

    int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
    SDL_SetRenderDrawColor(r, 220, 45, 45, 250);
    draw_filled_circle(r, cx, cy - 50, 40);
    SDL_SetRenderDrawColor(r, 0, 0, 20, 240);
    draw_filled_circle(r, cx - 13, cy - 55, 9);
    draw_filled_circle(r, cx + 13, cy - 55, 9);
    SDL_SetRenderDrawColor(r, 220, 45, 45, 250);
    SDL_Rect jaw = {cx - 20, cy - 30, 40, 10};
    SDL_RenderFillRect(r, &jaw);

    SDL_SetRenderDrawColor(r, 200, 200, 255, 200);
    SDL_Rect btn = {cx - 60, cy + 30, 120, 36};
    SDL_RenderFillRect(r, &btn);
    SDL_SetRenderDrawColor(r, 60, 60, 180, 255);
    SDL_Rect btnb = {cx - 60, cy + 30, 120, 36};
    SDL_RenderDrawRect(r, &btnb);
    SDL_SetRenderDrawColor(r, 20, 20, 80, 255);
    SDL_Rect xmark1 = {cx - 12, cy + 38, 24, 20};
    SDL_Rect xmark2 = {cx - 12, cy + 38, 24, 20};
    (void)xmark1; (void)xmark2;
    SDL_RenderDrawLine(r, cx - 10, cy + 38, cx + 10, cy + 58);
    SDL_RenderDrawLine(r, cx + 10, cy + 38, cx - 10, cy + 58);
}

static void draw_win(SDL_Renderer *r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 25, 0, 160);
    SDL_Rect ov = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(r, &ov);

    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 50;
    SDL_SetRenderDrawColor(r, 255, 215, 40, 250);
    for (int i = 0; i < 8; i++) {
        float ang = i * 3.14159f * 2.0f / 8.0f;
        SDL_RenderDrawLine(r, cx, cy,
            cx + (int)(70 * cosf(ang)),
            cy + (int)(70 * sinf(ang)));
    }
    draw_filled_circle(r, cx, cy, 32);
    SDL_SetRenderDrawColor(r, 255, 255, 180, 255);
    draw_filled_circle(r, cx, cy, 18);

    SDL_SetRenderDrawColor(r, 200, 200, 255, 200);
    SDL_Rect btn = {cx - 60, cy + 80, 120, 36};
    SDL_RenderFillRect(r, &btn);
    SDL_SetRenderDrawColor(r, 20, 20, 80, 255);
    SDL_RenderDrawLine(r, cx - 10, cy + 88, cx + 10, cy + 108);
    SDL_RenderDrawLine(r, cx + 10, cy + 88, cx - 10, cy + 108);
}

static void update(float ax, float ay, float dt) {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        bubbles[i].y += bubbles[i].vy * dt;
        if (bubbles[i].y < 0) {
            bubbles[i].y = (float)WORLD_H;
            bubbles[i].x = (float)(rand() % WORLD_W);
        }
    }

    for (int i = 0; i < MAX_FISH; i++) {
        fish[i].phase += dt;
        fish[i].x += fish[i].vx * dt;
        fish[i].y += sinf(fish[i].phase * 1.6f + i) * 0.6f;
        if (fish[i].x < 100)         fish[i].vx =  fabsf(fish[i].vx);
        if (fish[i].x > WORLD_W-100) fish[i].vx = -fabsf(fish[i].vx);
        if (fish[i].y < 100)         fish[i].y = 100;
        if (fish[i].y > WORLD_H-100) fish[i].y = WORLD_H - 100;
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0) continue;
        particles[i].x    += particles[i].vx * dt * 60;
        particles[i].y    += particles[i].vy * dt * 60;
        particles[i].life -= dt;
    }

    player.x += ax * PLAYER_SPEED * dt;
    player.y += ay * PLAYER_SPEED * dt;
    if (player.x < 20)         player.x = 20;
    if (player.x > WORLD_W-20) player.x = WORLD_W - 20;
    if (player.y < 20)         player.y = 20;
    if (player.y > WORLD_H-20) player.y = WORLD_H - 20;

    player.oxygen -= OXYGEN_DRAIN * dt;
    if (player.oxygen < 0) { player.oxygen = 0; state = STATE_GAMEOVER; return; }

    int near_oxy = 0;
    for (int i = 0; i < MAX_OXY_PODS; i++) {
        if (!oxypods[i].alive) continue;
        float dx = player.x - oxypods[i].x;
        float dy = player.y - oxypods[i].y;
        if (sqrtf(dx*dx + dy*dy) < oxypods[i].radius) { near_oxy = 1; break; }
    }
    if (near_oxy) {
        player.oxygen += OXYGEN_REFILL * dt;
        if (player.oxygen > OXYGEN_MAX) player.oxygen = OXYGEN_MAX;
    }

    static const int rc[3][3] = {{90, 140, 255}, {80, 210, 90}, {255, 195, 45}};
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!resources[i].alive) continue;
        float dx = player.x - resources[i].x;
        float dy = player.y - resources[i].y;
        if (sqrtf(dx*dx + dy*dy) < 26.0f) {
            resources[i].alive = 0;
            player.res[resources[i].type]++;
            player.total++;
            int t = resources[i].type;
            for (int p = 0; p < 6; p++)
                spawn_particle(resources[i].x, resources[i].y, rc[t][0], rc[t][1], rc[t][2]);
        }
    }

    if (player.total >= MAX_RESOURCES) state = STATE_WIN;

    cam_x = player.x - SCREEN_W * 0.5f;
    cam_y = player.y - SCREEN_H * 0.5f;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > WORLD_W - SCREEN_W) cam_x = WORLD_W - SCREEN_W;
    if (cam_y > WORLD_H - SCREEN_H) cam_y = WORLD_H - SCREEN_H;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    SDL_Window   *win = SDL_CreateWindow("DeepDive Vita",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_GameController *ctrl = NULL;
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
        ctrl = SDL_GameControllerOpen(0);

    init_world();
    init_player();
    state = STATE_PLAYING;

    Uint32 last_tick = SDL_GetTicks();
    float  total_t   = 0.0f;
    int    running   = 1;

    while (running) {
        Uint32 now = SDL_GetTicks();
        float  dt  = (now - last_tick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;
        total_t  += dt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; break; }
            if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) { running = 0; break; }
                if ((state == STATE_GAMEOVER || state == STATE_WIN) &&
                    ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    init_world(); init_player(); state = STATE_PLAYING;
                }
            }
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                if ((state == STATE_GAMEOVER || state == STATE_WIN) &&
                    ev.key.keysym.sym == SDLK_RETURN) {
                    init_world(); init_player(); state = STATE_PLAYING;
                }
            }
        }

        float ax = 0, ay = 0;
        if (ctrl) {
            Sint16 lx = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 ly = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTY);
            ax = lx / 32767.0f;
            ay = ly / 32767.0f;
            if (fabsf(ax) < DEADZONE) ax = 0;
            if (fabsf(ay) < DEADZONE) ay = 0;
        }
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) ax = -1.0f;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) ax =  1.0f;
        if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) ay = -1.0f;
        if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) ay =  1.0f;

        if (state == STATE_PLAYING) update(ax, ay, dt);

        SDL_SetRenderDrawColor(ren, 4, 18, 55, 255);
        SDL_RenderClear(ren);
        draw_world(ren, total_t);
        draw_hud(ren);
        if (state == STATE_GAMEOVER) draw_gameover(ren);
        if (state == STATE_WIN)      draw_win(ren);
        SDL_RenderPresent(ren);
    }

    if (ctrl) SDL_GameControllerClose(ctrl);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
