
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class TileMap {
public:
    void generate(int w, int h){
        m_w=w; m_h=h; m_tiles.assign(w*h, 0);
        // Simple path band
        for(int x=0;x<m_w;++x){
            int y = m_h/2 + (x%5==0?1:0);
            if(y>=0 && y<m_h) m_tiles[y*m_w+x]=1;
        }
    }
    void render(sf::RenderTarget& rt) const{
        sf::RectangleShape r({64,64});
        for(int y=0;y<m_h;++y){
            for(int x=0;x<m_w;++x){
                r.setPosition(x*64.f,y*64.f);
                if(m_tiles[y*m_w+x]==0) r.setFillColor(sf::Color(90,160,70));
                else                    r.setFillColor(sf::Color(170,135,95));
                rt.draw(r);
            }
        }
    }
    bool inBounds(int gx,int gy) const { return gx>=0&&gy>=0&&gx<m_w&&gy<m_h; }
    int width()  const { return m_w; }
    int height() const { return m_h; }
private:
    int m_w=0, m_h=0;
    std::vector<int> m_tiles; // 0 grass, 1 path
};
