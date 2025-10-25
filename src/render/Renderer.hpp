
#pragma once
#include <SFML/Graphics.hpp>
#include "../map/TileMap.hpp"
#include "../map/FogOfWar.hpp"

class Renderer{
public:
    void draw(sf::RenderWindow& win, const TileMap& map, const FogOfWar& fog);
};
