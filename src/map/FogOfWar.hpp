
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class FogOfWar{
public:
    void init(int w,int h){ m_w=w; m_h=h; m_vis.assign(w*h, 0); }
    void clear(){ std::fill(m_vis.begin(), m_vis.end(), 0); }
    void revealCircle(sf::Vector2f world, float radius){
        int gx = (int)(world.x/64); int gy = (int)(world.y/64);
        int r = (int)(radius/64)+1;
        for(int y=gy-r;y<=gy+r;++y){
            for(int x=gx-r;x<=gx+r;++x){
                if(x>=0&&y>=0&&x<m_w&&y<m_h){
                    float cx=x*64+32, cy=y*64+32;
                    float dx=cx-world.x, dy=cy-world.y;
                    if(dx*dx+dy*dy <= radius*radius) m_vis[y*m_w+x]=1;
                }
            }
        }
    }
    void render(sf::RenderTarget& rt, float alpha=0.65f) const{
        sf::RectangleShape r({64,64});
        for(int y=0;y<m_h;++y){
            for(int x=0;x<m_w;++x){
                if(m_vis[y*m_w+x]==0){
                    r.setFillColor(sf::Color(0,0,0,(sf::Uint8)(alpha*255)));
                    r.setPosition(x*64.f,y*64.f);
                    rt.draw(r);
                }
            }
        }
    }
private:
    int m_w=0,m_h=0;
    std::vector<unsigned char> m_vis;
};
