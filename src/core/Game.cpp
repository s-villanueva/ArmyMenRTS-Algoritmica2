
#include "Game.hpp"
#include "State.hpp"
#include "../game/PlayState.hpp"

Game::Game() : m_window(sf::VideoMode(1280,720), "ArmyMen RTS — Playable"){
    m_window.setFramerateLimit(60);
    changeState(std::make_unique<PlayState>(*this));
}
void Game::run(){
    sf::Clock clk;
    while(m_window.isOpen()){
        sf::Event e;
        while(m_window.pollEvent(e)){
            if(e.type==sf::Event::Closed) m_window.close();
            if(m_state) m_state->handleEvent(e);
        }
        float dt = clk.restart().asSeconds();
        if(m_state){
            m_state->update(dt);
            m_window.clear(sf::Color(20,40,20));
            m_state->render(m_window);
            m_window.display();
        }
    }
}
void Game::changeState(std::unique_ptr<State> st){ m_state = std::move(st); }
