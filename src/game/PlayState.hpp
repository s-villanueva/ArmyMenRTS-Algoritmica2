
#pragma once
#include <unordered_map>

#include "../core/State.hpp"
#include "../map/TileMap.hpp"
#include "../map/FogOfWar.hpp"
#include "../render/Renderer.hpp"

enum class UnitType { Soldier, Harvester, Bulldozer /* + los que vengan */ };

struct Unit{
    sf::Vector2f pos;
    sf::Vector2f target;
    bool hasTarget=false;
    bool selected=false;
    float speed=120.f;
    UnitType type = UnitType::Soldier; // NUEVO
    bool alive = true;
};

struct ResourceNode {
    sf::Vector2f pos;
    float amount = 300.f;
};
struct Building {
    enum class Type { HQ, Depot, Garage };
    Type type;
    sf::Vector2f pos;
    std::vector<std::string> queue;
    float buildTimer = 0.f;
};

inline std::unordered_map<std::string, int> costs_ {
        {"HQ",50},{"Depot",40},{"Garage",60},
        {"Soldier",10},{"Tank",50}
};

inline std::vector<ResourceNode> resources_;
inline std::vector<Building> buildingsA_;
inline int plastic_ = 200;

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
    void bulldozerBuildAttempt(const sf::Vector2f& pos);

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
