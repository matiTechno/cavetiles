#include "Scene.hpp"
#include "imgui/imgui.h"
#include "GLFW/glfw3.h"
#include <math.h>

void Emitter::reserve()
{
    const int maxParticles = particleRanges.life.max * spawn.hz;
    particles.reserve(maxParticles);
    rects.reserve(maxParticles);
}

void Emitter::update(const float dt)
{
    for(int i = 0; i < numActive; ++i)
    {
        if(particles[i].life <= 0.f)
        {
            particles[i] = particles[numActive - 1];
            rects[i] = rects[numActive - 1];
            --numActive;
        }
        
        rects[i].pos.x += particles[i].vel.x * dt;
        rects[i].pos.y += particles[i].vel.y * dt;
        particles[i].life -= dt;
    }

    spawn.activeTime -= dt;
    if(spawn.activeTime <= 0.f)
        return;

    accumulator += dt;
    const float spawnTime = 1.f / spawn.hz;
    while(accumulator >= spawnTime)
    {
        accumulator -= spawnTime;

        if(numActive + 1 > particles.size())
        {
            particles.pushBack({});
            rects.pushBack({});
        }

        Particle& p = particles[numActive];
        p.life = getRandomFloat(particleRanges.life.min, particleRanges.life.max);
        p.vel.x = getRandomFloat(particleRanges.vel.min.x, particleRanges.vel.max.x);
        p.vel.y = getRandomFloat(particleRanges.vel.min.y, particleRanges.vel.max.y);

        Rect& rect = rects[numActive];

        rect.size.x = getRandomFloat(particleRanges.size.min, particleRanges.size.max);
        rect.size.y = rect.size.x;

        rect.color.x = getRandomFloat(particleRanges.color.min.x, particleRanges.color.max.x);
        rect.color.y = getRandomFloat(particleRanges.color.min.y, particleRanges.color.max.y);
        rect.color.z = getRandomFloat(particleRanges.color.min.z, particleRanges.color.max.z);
        rect.color.w = getRandomFloat(particleRanges.color.min.w, particleRanges.color.max.w);

        rect.pos.x = getRandomFloat(0.f, 1.f) * spawn.size.x + spawn.pos.x
                     - rect.size.x / 2.f;
        rect.pos.y = getRandomFloat(0.f, 1.f) * spawn.size.y + spawn.pos.y
                     - rect.size.y / 2.f;

        ++numActive;
    }
}

void Anim::update(const float dt)
{
    accumulator += dt;
    if(accumulator >= frameDt)
    {
        accumulator -= frameDt;
        ++idx;
        if(idx > numFrames - 1)
            idx = 0;
    }
}

ivec2 getPlayerTile(const vec2 playerPos, const float tileSize)
{
    return {int(playerPos.x / tileSize + 0.5f),
            int(playerPos.y / tileSize + 0.5f)};
}

bool isCollision(const vec2 playerPos, const ivec2 tile, const float tileSize)
{
    vec2 tilePos = {tile.x * tileSize, tile.y * tileSize};

    return playerPos.x < tilePos.x + tileSize &&
           playerPos.x + tileSize > tilePos.x &&
           playerPos.y < tilePos.y + tileSize &&
           playerPos.y + tileSize > tilePos.y;
}

float length(const vec2 v)
{
    return sqrt(v.x * v.x + v.y * v.y);
}

vec2 normalize(const vec2 v)
{
    const float len = length(v);
    return {v.x / len, v.y / len};
}

float dot(const vec2 v1, const vec2 v2)
{
    return v1.x * v2.x + v1.y * v2.y;
}

GameScene::GameScene()
{
    glBuffers_ = createGLBuffers();
    tileTexture_ = createTextureFromFile("res/tiles.png");
    player1Texture_ = createTextureFromFile("res/player1.png");
    player2Texture_ = createTextureFromFile("res/player2.png");

    emitter_.spawn.size = {5.f, 5.f};
    emitter_.spawn.pos = {tileSize_ * 10 + (tileSize_ - emitter_.spawn.size.x) / 2.f,
                          tileSize_ * 10 + (tileSize_ - emitter_.spawn.size.y) / 2.f};
    emitter_.spawn.hz = 100.f;
    emitter_.particleRanges.life = {3.f, 6.f};
    emitter_.particleRanges.size = {0.25f, 2.f};
    emitter_.particleRanges.vel = {{-3.5f, -30.f}, {3.5f, -2.f}};
    emitter_.particleRanges.color = {{0.1f, 0.f, 0.f, 0.f}, {0.5f, 0.25f, 0.f, 0.f}};
    emitter_.reserve();

    for(Player& player: players_)
    {
        player.vel = 80.f;
    }

    // specific configuration for each player
    assert(getSize(players_) >= 2);

    players_[0].pos = {tileSize_ * 1, tileSize_ * 1};
    players_[0].prevDir = Dir::Down;
    players_[0].texture = &player1Texture_;

    players_[1].pos = {tileSize_ * (MapSize - 2), tileSize_ * (MapSize - 2)};
    players_[1].prevDir = Dir::Left;
    players_[1].texture = &player2Texture_;

    // tightly coupled to the texture assets
    for(int i = Dir::Up; i < Dir::Count; ++i)
    {
        Anim anim;
        anim.frameDt = 0.06f;
        anim.numFrames = 4;
        const float frameSize = 48.f;

        int x;
        switch(i)
        {
            case Dir::Up:    x = 2; break;
            case Dir::Down:  x = 0; break;
            case Dir::Left:  x = 1; break;
            case Dir::Right: x = 3;
        }

        for(int i = 0; i < anim.numFrames; ++i)
        {
            anim.frames[i] = {frameSize * x, frameSize * i, frameSize, frameSize};
        }

        for(int j = 0; j < 2; ++j)
        {
            players_[j].anims[i] = anim;
        }
    }

    // tilemap

    // edges
    for(int i = 0; i < MapSize; ++i)
    {
        tiles_[0][i] = 2;
        tiles_[MapSize - 1][i] = 2;
        tiles_[i][0] = 2;
        tiles_[i][MapSize - 1] = 2;
    }

    // pillars
    for(int i = 2; i < MapSize - 1; i += 2)
    {
        for(int j = 2; j < MapSize - 1; j += 2)
        {
            tiles_[j][i] = 2;
        }
    }

    // crates
    int freeTiles[MapSize * MapSize];
    int numFreeTiles = 0;

    for(int i = 0; i < MapSize * MapSize; ++i)
    {
        if(tiles_[0][i] == 0)
        {
            // check if it is not adjacent to the players

            const ivec2 targetTile = {i % MapSize, i / MapSize};

            for(const Player& player: players_)
            {
                const ivec2 playerTile = getPlayerTile(player.pos, tileSize_);

                for(int k = -1; k < 2; ++k)
                {
                    for(int l = -1; l < 2; ++l)
                    {
                        const ivec2 adjacentTile = {playerTile.x + k, playerTile.y + l};

                        if(targetTile.x == adjacentTile.x && targetTile.y == adjacentTile.y)
                            goto end;
                    }
                }
            }
            freeTiles[numFreeTiles] = i;
            ++numFreeTiles;
        }
end:;
    }

    const int numCrates = numFreeTiles * 2 / 3;
    for(int i = 0; i < numCrates; ++i)
    {
        const int freeTileIdx = getRandomInt(0, numFreeTiles - 1);
        const int tileIdx = freeTiles[freeTileIdx];
        freeTiles[freeTileIdx] = freeTiles[numFreeTiles - 1];
        tiles_[0][tileIdx] = 1;
        numFreeTiles -= 1;
    }

    // rects

    for (int i = 0; i < MapSize; ++i)
    {
        for (int j = 0; j < MapSize; ++j)
        {
            Rect& rect = rects_[j + i * MapSize];
            rect.pos = {j * tileSize_, i * tileSize_};
            rect.size = {tileSize_, tileSize_};

            switch (tiles_[i][j])
            {
                case 0: rect.texRect = {0.f, 0.f, 64.f, 64.f};   break;
                case 1: rect.texRect = {64.f, 0.f, 64.f, 64.f};  break;
                case 2: rect.texRect = {128.f, 0.f, 32.f, 32.f};
                        rect.color = {0.25f, 0.25f, 0.25f, 1.f}; break;
            }

            rect.texRect.x /= tileTexture_.size.x;
            rect.texRect.y /= tileTexture_.size.y;
            rect.texRect.z /= tileTexture_.size.x;
            rect.texRect.w /= tileTexture_.size.y;
        }
    }
}

GameScene::~GameScene()
{
    deleteTexture(player1Texture_);
    deleteTexture(player2Texture_);
    deleteTexture(tileTexture_);
    deleteGLBuffers(glBuffers_);
}

void GameScene::processInput(const Array<WinEvent>& events)
{
    // @TODO(matiTechno): replace with 'gaffer on games' technique
    frame_.time = min(frame_.time, 0.033f);

    for (const WinEvent& e: events)
    {
        if(e.type == WinEvent::Key)
        {
            const bool on = e.key.action != GLFW_RELEASE;

            switch(e.key.key)
            {
                case GLFW_KEY_W:     keys_[0].up = on;    break;
                case GLFW_KEY_S:     keys_[0].down = on;  break;
                case GLFW_KEY_A:     keys_[0].left = on;  break;
                case GLFW_KEY_D:     keys_[0].right = on; break;

                case GLFW_KEY_UP:    keys_[1].up = on;    break;
                case GLFW_KEY_DOWN:  keys_[1].down = on;  break;
                case GLFW_KEY_LEFT:  keys_[1].left = on;  break;
                case GLFW_KEY_RIGHT: keys_[1].right = on;
            }
        }
    }

    assert(getSize(players_) >= getSize(keys_));

    for(int i = 0; i < getSize(keys_); ++i)
    {
        if(players_[i].dir)
            players_[i].prevDir = players_[i].dir;

        if     (keys_[i].left)  players_[i].dir = Dir::Left;
        else if(keys_[i].right) players_[i].dir = Dir::Right;
        else if(keys_[i].up)    players_[i].dir = Dir::Up;
        else if(keys_[i].down)  players_[i].dir = Dir::Down;
        else                    players_[i].dir = Dir::Nil;
    }
}

void GameScene::update()
{
    emitter_.update(frame_.time);

    for(Player& player: players_)
    {
        player.pos.x += player.vel * frame_.time * dirVecs_[player.dir].x;
        player.pos.y += player.vel * frame_.time * dirVecs_[player.dir].y;
        player.anims[player.dir].update(frame_.time);

        const ivec2 playerTile = getPlayerTile(player.pos, tileSize_);
        bool collision = false;

        for(int i = -1; i < 2; ++i)
        {
            for(int j = -1; j < 2; ++j)
            {
                const ivec2 tile = {playerTile.x + i, playerTile.y + j};

                if(tiles_[tile.y][tile.x] != 0 && isCollision(player.pos, tile, tileSize_))
                {
                    collision = true;
                    goto end;
                }
            }
        }
end:
        if(collision)
        {
            const vec2 playerTilePos = {playerTile.x * tileSize_, playerTile.y * tileSize_};

            if(player.dir == Dir::Left || player.dir == Dir::Right)
            {
                player.pos.x = playerTilePos.x;
            }
            else
            {
                player.pos.y = playerTilePos.y;
            }

            const ivec2 targetTile = {int(playerTile.x + dirVecs_[player.dir].x),
                                      int(playerTile.y + dirVecs_[player.dir].y)};

            // important: y first, x second
            if(tiles_[targetTile.y][targetTile.x] == 0)
            {

                const vec2 slideVec = {playerTilePos.x - player.pos.x,
                                       playerTilePos.y - player.pos.y};

                const vec2 slideDir = normalize(slideVec);

                player.pos.x += player.vel * frame_.time * slideDir.x;
                player.pos.y += player.vel * frame_.time * slideDir.y;

                const vec2 newSlideVec = {playerTilePos.x - player.pos.x,
                                          playerTilePos.y - player.pos.y};

                if(dot(slideVec, newSlideVec) < 0.f)
                {
                    player.pos = playerTilePos;
                }
            }
        }
    }
}

void GameScene::render(const GLuint program)
{
    bindProgram(program);

    Camera camera;
    camera.pos = {0.f, 0.f};
    camera.size = {MapSize * tileSize_, MapSize * tileSize_};
    camera = expandToMatchAspectRatio(camera, frame_.fbSize);
    uniform2f(program, "cameraPos", camera.pos);
    uniform2f(program, "cameraSize", camera.size);

    // 1) render the tilemap

    uniform1i(program, "mode", FragmentMode::Texture);
    bindTexture(tileTexture_);
    updateGLBuffers(glBuffers_, rects_, getSize(rects_));
    renderGLBuffers(glBuffers_, getSize(rects_));

    // 2) render the players
    
    for(Player& player: players_)
    {
        Rect rect;
        rect.size = {tileSize_, tileSize_};
        rect.pos = player.pos;

        const vec4 frame = player.dir ? player.anims[player.dir].getCurrentFrame() :
                                        player.anims[player.prevDir].frames[0];

        rect.texRect.x = frame.x / player.texture->size.x;
        rect.texRect.y = frame.y / player.texture->size.y;
        rect.texRect.z = frame.z / player.texture->size.x;
        rect.texRect.w = frame.w / player.texture->size.y;

        uniform1i(program, "mode", FragmentMode::Texture);
        bindTexture(*player.texture);
        updateGLBuffers(glBuffers_, &rect, 1);
        renderGLBuffers(glBuffers_, 1);
    }

    // 3) particles

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    uniform1i(program, "mode", FragmentMode::Color);
    updateGLBuffers(glBuffers_, emitter_.rects.data(), emitter_.numActive);
    renderGLBuffers(glBuffers_, emitter_.numActive);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 4) imgui

    ImGui::ShowDemoWindow();
    ImGui::Begin("options");
    ImGui::Text("test test test !!!");
    if(ImGui::Button("quit"))
        frame_.popMe = true;
    ImGui::End();
}
