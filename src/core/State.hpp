
#pragma once
#include <SFML/Graphics.hpp>
class Game;
class State{
public:
    explicit State(Game& g):game(g){} virtual ~State()=default;
    virtual void handleEvent(const sf::Event&)=0;
    virtual void update(float)=0;
    virtual void render(sf::RenderWindow&)=0;
protected: Game& game;
};
