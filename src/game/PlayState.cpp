
#include "PlayState.hpp"
#include "../core/Game.hpp"
#include <cmath>
#include <iostream>

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
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::N){
        showFog_ = !showFog_;
    }

    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::Q){
        for (auto& b : buildingsA_) if (b.type == Building::Type::HQ){
            b.queue.push_back("Soldier");
            break;
        }
    }
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::E){
        for (auto& b : buildingsA_) if (b.type == Building::Type::Garage){
            b.queue.push_back("Tank");
            break;
        }
    }


    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::M){
        auto& win = game.window();
        sf::Vector2f w = screenToWorld(win, sf::Mouse::getPosition(win));
        mines_.push_back({w, 18.f, true});
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
        font.loadFromFile("/usr/share/fonts/TTF/DejaVuSans.ttf"); // may fail silently on some OS
        hud.setFont(font); hud.setCharacterSize(16); hud.setFillColor(sf::Color::White);
        // nodos de recurso
        resources_.push_back({ {600.f, 400.f}, 300.f });
        resources_.push_back({ {740.f, 520.f}, 300.f });
        // depósito aliado
        buildingsA_.push_back({ Building::Type::HQ,    {200.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Garage,{320.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Depot, {260.f, 200.f} });
        // volqueta: reutiliza tu player o crea otra unidad
        // si quieres que player sea harvester:
        player.type = UnitType::Soldier;
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

    auto len = [](sf::Vector2f v){ return std::sqrt(v.x*v.x + v.y*v.y); };

    for (auto& m : mines_) if (m.active) {
        // ejemplo con el player; si tienes un vector<Unidad> itera sobre todas
        if (len(player.pos - m.pos) < m.radius) {
            m.active = false;
            // aplica daño/efecto; por ahora placeholder: deseleccionar jugador
            player.selected = false;
        }
    }

    auto length = [](sf::Vector2f v){ return std::sqrt(v.x*v.x + v.y*v.y); };
    auto norm   = [&](sf::Vector2f v){ float L = length(v); return (L>0)? v*(1.f/L) : sf::Vector2f(0,0); };

    if (player.type == UnitType::Harvester) {
        // Busca nodo más cercano con amount>0
        int idx=-1; float best=1e9f;
        for (int i=0; i<(int)resources_.size(); ++i){
            if (resources_[i].amount <= 0) continue;
            float d = length(resources_[i].pos - player.pos);
            if (d < best){ best = d; idx = i; }
        }

        if (idx != -1) {
            if (best < 14.f) {
                // “mina” el recurso
                resources_[idx].amount -= 20.f * dt;
                if (resources_[idx].amount < 0) resources_[idx].amount = 0;
            } else {
                player.pos += norm(resources_[idx].pos - player.pos) * (player.speed * 0.6f) * dt;
            }
        } else {
            // ya no quedan recursos -> ve al depósito y suma plástico
            for (auto& b : buildingsA_) if (b.type == Building::Type::Depot) {
                float d = length(b.pos - player.pos);
                if (d < 16.f){ plastic_ += 60; } // depósito
                else { player.pos += norm(b.pos - player.pos) * (player.speed * 0.6f) * dt; }
                break;
            }
        }
    }

    for (auto& b : buildingsA_){
        if (b.queue.empty()){ b.buildTimer = 0.f; continue; }
        if (b.buildTimer <= 0.f) b.buildTimer = 2.0f; // tiempo placeholder
        else b.buildTimer -= dt;

        if (b.buildTimer <= 0.f){
            std::string item = b.queue.front(); b.queue.erase(b.queue.begin());
            if (item == "Soldier"){
                if (plastic_ >= costs_["Soldier"]){
                    plastic_ -= costs_["Soldier"];
                    // crea una unidad nueva junto al edificio (aquí reusarías tu contenedor real de unidades)
                    // por ahora, si solo tienes `player`, omite o prepara un vector< Unit > teamA_
                    // teamA_.push_back(Unit{ .pos = b.pos + sf::Vector2f(0,40), .type = UnitType::Soldier });
                }
            } else if (item == "Tank"){
                if (plastic_ >= costs_["Tank"]){
                    plastic_ -= costs_["Tank"];
                    // teamA_.push_back(Unit{ .pos = b.pos + sf::Vector2f(0,40), .type = UnitType::Soldier /* placeholder */ });
                }
            }
        }
    }


    fog.clear();
    if (showFog_) {
        // Revela alrededor de cada unidad aliada que quieras visible en el mapa
        // player ya existe; si tienes más, repite.
        fog.revealCircle(player.pos, 160.f); // radio placeholder
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

    for (auto& m : mines_) if (m.active) {
        sf::CircleShape c(m.radius);
        c.setOrigin(m.radius, m.radius);
        c.setFillColor(sf::Color(120,60,60));
        c.setPosition(m.pos);
        win.draw(c);
    }

    // recursos
    for (auto& r : resources_){
        sf::CircleShape c(8.f); c.setOrigin(8,8);
        c.setFillColor(sf::Color(200,180,0));
        c.setPosition(r.pos);
        win.draw(c);
    }

    // HUD plástico (si ya usas sf::Text hud, añade la línea)
    hud.setString("Plastico: " + std::to_string(plastic_));

    for (auto& b : buildingsA_){
        sf::RectangleShape s({40,40}); s.setOrigin(20,20);
        s.setPosition(b.pos);
        s.setFillColor(b.type==Building::Type::HQ ? sf::Color(120,160,120) :
                       b.type==Building::Type::Depot ? sf::Color(160,160,80) :
                                                       sf::Color(120,120,180));
        win.draw(s);
    }

    if(player.selected){
        sf::CircleShape ring(18.f); ring.setOrigin(18,18); ring.setPosition(player.pos);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color::Green); ring.setOutlineThickness(2.f);
        win.draw(ring);
    }
    win.draw(hud);


}
