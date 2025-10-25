
#pragma once
#include <memory>
#include <SFML/Graphics.hpp>
#include "State.hpp"

class Game {
public:
    Game();
    void run();
    void changeState(std::unique_ptr<State> st);
    sf::RenderWindow& window(){ return m_window; }
private:
    sf::RenderWindow m_window;
    std::unique_ptr<State> m_state;
};
