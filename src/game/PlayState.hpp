
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

struct Mine {
    sf::Vector2f pos;
    float radius = 18.f;
    bool active = true;
};

inline std::vector<Mine> mines_;


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
    bool showFog_ = true;   // toggle

    Unit player;
    sf::View cam;
    float zoom=1.f;

    sf::Font font;
    sf::Text hud;

    bool dragging_ = false;
    sf::Vector2f dragStart_;
    sf::RectangleShape dragRect_;

    sf::Vector2f screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const;
};
