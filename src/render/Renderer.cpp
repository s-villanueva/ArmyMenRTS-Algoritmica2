
#include "Renderer.hpp"
void Renderer::draw(sf::RenderWindow& win, const TileMap& map, const FogOfWar& fog){
    map.render(win);
    fog.render(win);
}
