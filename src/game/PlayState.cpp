
#include "PlayState.hpp"
#include "../core/Game.hpp"
#include <cmath>

void PlayState::handleEvent(const sf::Event& e){
    auto& win = game.window();
    if(e.type==sf::Event::MouseWheelScrolled){
        zoom += (e.mouseWheelScroll.delta>0 ? -0.1f : 0.1f);
        if(zoom<0.6f) zoom=0.6f; if(zoom>1.8f) zoom=1.8f;
        cam.setSize(1280*zoom,720*zoom);
        win.setView(cam);
    }
    if(e.type==sf::Event::MouseButtonPressed){
        if(e.mouseButton.button==sf::Mouse::Left){
            sf::Vector2f w = screenToWorld(win,{e.mouseButton.x,e.mouseButton.y});
            // select if near
            player.selected = (std::hypot(w.x-player.pos.x, w.y-player.pos.y) < 28.f);
        }
        if(e.mouseButton.button==sf::Mouse::Right){
            if(player.selected){
                sf::Vector2f w = screenToWorld(win,{e.mouseButton.x,e.mouseButton.y});
                player.target = w; player.hasTarget=true;
            }
        }
    }
}
void PlayState::update(float dt){
    auto& win = game.window();
    // init once
    static bool init=false;
    if(!init){
        map.generate(32,20);
        fog.init(map.width(), map.height());
        cam = win.getDefaultView();
        player.pos = {4*64.f+32.f, 4*64.f+32.f};
        player.target = player.pos;
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"); // may fail silently on some OS
        hud.setFont(font); hud.setCharacterSize(16); hud.setFillColor(sf::Color::White);
        init=true;
    }

    // WASD camera
    float pan=400.f*dt;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::W)) cam.move(0,-pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::S)) cam.move(0, pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::A)) cam.move(-pan,0);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) cam.move( pan,0);
    win.setView(cam);

    // Move unit
    if(player.hasTarget){
        sf::Vector2f dir = player.target - player.pos;
        float len = std::hypot(dir.x, dir.y);
        if(len>2.f){
            dir.x/=len; dir.y/=len;
            player.pos += dir * player.speed * dt;
        } else {
            player.hasTarget=false;
        }
    }

    // Reveal fog around unit
    fog.revealCircle(player.pos, 5*64.f);

    // HUD text
    hud.setString("LClick: select | RClick: move | Wheel: zoom | WASD: pan");
    hud.setPosition(cam.getCenter().x - cam.getSize().x/2 + 10, cam.getCenter().y - cam.getSize().y/2 + 10);
}
sf::Vector2f PlayState::screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const{
    return win.mapPixelToCoords(mouse);
}
void PlayState::render(sf::RenderWindow& win){
    renderer.draw(win, map, fog);
    // draw unit
    sf::CircleShape body(12.f); body.setOrigin(12,12); body.setPosition(player.pos);
    body.setFillColor(sf::Color(60,150,70));
    win.draw(body);
    // selection ring
    if(player.selected){
        sf::CircleShape ring(18.f); ring.setOrigin(18,18); ring.setPosition(player.pos);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color::Green); ring.setOutlineThickness(2.f);
        win.draw(ring);
    }
    win.draw(hud);
}
