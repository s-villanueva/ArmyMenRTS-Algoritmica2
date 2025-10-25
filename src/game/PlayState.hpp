
#pragma once
#include "../core/State.hpp"
#include "../map/TileMap.hpp"
#include "../map/FogOfWar.hpp"
#include "../render/Renderer.hpp"

struct Unit{
    sf::Vector2f pos;
    sf::Vector2f target;
    bool hasTarget=false;
    bool selected=false;
    float speed=120.f;
};

class PlayState : public State {
public:
    using State::State;
    void handleEvent(const sf::Event&) override;
    void update(float dt) override;
    void render(sf::RenderWindow&) override;
private:
    TileMap map;
    FogOfWar fog;
    Renderer renderer;

    Unit player;
    sf::View cam;
    float zoom=1.f;

    sf::Font font;
    sf::Text hud;

    sf::Vector2f screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const;
};
