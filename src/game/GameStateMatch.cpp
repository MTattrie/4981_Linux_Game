#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#include "GameStateMatch.h"
#include "../client/NetworkManager.h"
#include "../game/GameStateMatch.h"
#include "../sprites/Renderer.h"
#include "../sprites/SpriteTypes.h"
#include "../view/Window.h"
#include "../log/log.h"
#include "../server/server.h"
#include "../server/servergamestate.h"
#include "../sprites/VisualEffect.h"
#include "../map/Map.h"
#include "Game.h"

GameStateMatch::GameStateMatch(Game& g, const int gameWidth, const int gameHeight) : GameState(g),
        base(), camera(gameWidth,gameHeight){}

bool GameStateMatch::load() {
#ifndef SERVER
    if (networked) {
        GameManager::instance()->getPlayer().setControl(
                GameManager::instance()->getMarine(NetworkManager::instance().getPlayerId()).first);
        GameManager::instance()->getPlayer().setId(NetworkManager::instance().getPlayerId());
    } else {
        GameManager::instance()->addObject(base);
        Point newPoint = base.getSpawnPoint();
        GameManager::instance()->getPlayer().setControl(
                GameManager::instance()->getMarine(GameManager::instance()->createMarine()).first);
        GameManager::instance()->getPlayer().getMarine()->setPosition(newPoint.first, newPoint.second);
        GameManager::instance()->getPlayer().getMarine()->setSrcRect(SPRITE_FRONT, SPRITE_FRONT, SPRITE_SIZE_X, SPRITE_SIZE_Y);

        // Create Dummy Entitys
        //GameManager::instance()->createMarine(100, 100);
        GameManager::instance()->createZombie(800, 800);
        GameManager::instance()->createTurret(1000, 500);

        //set the boundary on the map
        GameManager::instance()->setBoundary(0, 0, MAP_WIDTH, MAP_HEIGHT);

        //createStores
        GameManager::instance()->createWeaponStore(STORE_X, STORE_Y);

        //createDropPoint
        GameManager::instance()->createDropZone(DROPZONE_X , DROPZONE_Y, DROPZONE_SIZE);
    }
#endif
    //GameManager::instance()->createZombieWave(1);

    bool success = true;
    //set the boundary on the map
    // GameManager::instance()->setBoundary(0, 0, MAP_WIDTH, MAP_HEIGHT);
    // Load Map
    Map m("assets/maps/Map4.csv");
    if(m.loadFileData() == 0) {
        logv("file not found");
    }
    m.mapLoadToGame();
    return success;
}

void GameStateMatch::loop() {
    int startTick = 0;
    int frameTicks = 0;
    // State Loop
    while (play) {
#ifndef SERVER
        // Process frame
        handle(); // Handle user input
#endif
        update((SDL_GetTicks() - startTick) / TICK_SEC); // Update state values
        startTick = SDL_GetTicks();
#ifndef SERVER
        // Sync game to server
        sync();
        // Render game state to window
        render();
#else
        //Server side sync packet sending
        //This will send update packets every frame
        //Currently at 60, so this is fine
        //If frame rate changes, this may need limiting
        sendSyncPacket(sendSocketUDP);
        clearAttackActions();
#endif

        //If frame finished early
        if ((frameTicks = SDL_GetTicks() - startTick) < SCREEN_TICK_PER_FRAME) {
            //Wait remaining time
            SDL_Delay(SCREEN_TICK_PER_FRAME - frameTicks);
        }
    }
}

void GameStateMatch::updateServ() {

}

void GameStateMatch::sync() {

}

void GameStateMatch::handle() {
    const Uint8 *state = SDL_GetKeyboardState(nullptr); // Keyboard state
    // Handle movement input
    GameManager::instance()->getPlayer().handleKeyboardInput(state);
    GameManager::instance()->getPlayer().handleMouseUpdate(game.getWindow().getWidth(), game.getWindow().getHeight(), camera.getX(), camera.getY());
    GameManager::instance()->getPlayer().getMarine()->updateImageDirection(); //Update direction of player
    GameManager::instance()->getPlayer().getMarine()->updateImageWalk(state);  //Update walking animation

    //Handle events on queue
    while (SDL_PollEvent(&event)) {
        game.getWindow().handleEvent(event);
        switch(event.type) {
            case SDL_WINDOWEVENT:
                camera.setViewSize(game.getWindow().getWidth(), game.getWindow().getHeight());
                break;
            case SDL_MOUSEWHEEL:
                GameManager::instance()->getPlayer().handleMouseWheelInput(&(event));
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    GameManager::instance()->getPlayer().handlePlacementClick(Renderer::instance().getRenderer());
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    GameManager::instance()->getPlayer().fireWeapon();
                }
                break;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        play = false;
                        break;
                    case SDLK_b:
                        GameManager::instance()->getPlayer().handleTempBarricade(
                                Renderer::instance().getRenderer());
                        break;
                    default:
                        break;
                    }
                    break;
            case SDL_KEYUP:
                switch(event.key.keysym.sym) {
                    default:
                        break;
                }
                break;
            case SDL_QUIT:
                play = false;
                break;
            default:
                break;
        }
    }
}

void GameStateMatch::update(const float delta) {
    GameManager::instance()->updateCollider();
#ifndef SERVER
    // Move player
    if (networked) {
        if (GameManager::instance()->getPlayer().hasChangedCourse()
                || GameManager::instance()->getPlayer().hasChangedAngle()) {
            GameManager::instance()->getPlayer().sendServMoveAction();
        }
        GameManager::instance()->getPlayer().getMarine()->move(
                GameManager::instance()->getPlayer().getMarine()->getDX() * delta,
                GameManager::instance()->getPlayer().getMarine()->getDY() * delta,
                GameManager::instance()->getCollisionHandler());
    }
    // Move Camera
    camera.move(GameManager::instance()->getPlayer().getMarine()->getX(),
            GameManager::instance()->getPlayer().getMarine()->getY());
#endif
    if (!networked) {
        GameManager::instance()->updateMarines(delta);
    }
    GameManager::instance()->updateZombies(delta);
    GameManager::instance()->updateTurrets();
}

void GameStateMatch::render() {
    //Only draw when not minimized
    if (!game.getWindow().isMinimized()) {

        SDL_RenderClear(Renderer::instance().getRenderer());

        //Render textures
        for (int i = camera.getX() / TEXTURE_SIZE - 1; ; ++i) {

            if (i * TEXTURE_SIZE - camera.getX() >= camera.getW()) {
                break;
            }

            for (int j = camera.getY() / TEXTURE_SIZE - 1; ; ++j) {
                if (j * TEXTURE_SIZE - camera.getY() >= camera.getH()) {
                    break;
                }

                Renderer::instance().render(
                        {i * TEXTURE_SIZE - static_cast<int>(camera.getX()),
                        j * TEXTURE_SIZE - static_cast<int>(camera.getY()),
                        TEXTURE_SIZE, TEXTURE_SIZE}, TEXTURES::BARREN);
            }
        }

        //render the temps before the objects in the game
        VisualEffect::instance().renderPreEntity(camera.getViewport());
        //renders objects in game
        GameManager::instance()->renderObjects(camera.getViewport());
        //render the temps after the object in the game
        VisualEffect::instance().renderPostEntity(camera.getViewport());

        //Update screen
        SDL_RenderPresent(Renderer::instance().getRenderer());
    }
}
