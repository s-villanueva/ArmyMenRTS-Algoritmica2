#include "PlayState.hpp"
#include "../core/Game.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

// ==================== Helpers locales ====================
static float vlen(sf::Vector2f v){ return std::sqrt(v.x*v.x + v.y*v.y); }
static sf::Vector2f vnorm(sf::Vector2f v){ float L=vlen(v); return (L>0)? v*(1.f/L) : sf::Vector2f(0.f,0.f); }
static sf::Vector2f worldMouse(sf::RenderWindow& win, const sf::View& cam){
    auto p = sf::Mouse::getPosition(win);
    return win.mapPixelToCoords(p, cam);
}

// mapear mouse (respetando la vista cam)

// offsets para formación (cuadrícula compacta)
static std::vector<sf::Vector2f> formationOffsets(std::size_t n, float spacing=18.f){
    std::vector<sf::Vector2f> off;
    if(n==0) return off;
    std::size_t cols = std::ceil(std::sqrt((float)n));
    std::size_t rows = std::ceil((float)n / (float)cols);
    float w = (cols-1)*spacing, h = (rows-1)*spacing;
    off.reserve(n);
    std::size_t k=0;
    for(std::size_t r=0;r<rows && k<n;r++){
        for(std::size_t c=0;c<cols && k<n;c++,k++){
            float x = (c*spacing) - w*0.5f;
            float y = (r*spacing) - h*0.5f;
            off.push_back({x,y});
        }
    }
    return off;
}

// ==================== Input ====================
void PlayState::handleEvent(const sf::Event& e){
    auto& win = game.window();

    // Zoom con rueda
    if(e.type==sf::Event::MouseWheelScrolled){
        zoom += (e.mouseWheelScroll.delta>0 ? -0.1f : 0.1f);
        if(zoom<0.6f) zoom=0.6f; if(zoom>1.8f) zoom=1.8f;
        cam.setSize(1280*zoom,720*zoom);
        win.setView(cam);
    }

    // Selección/movimiento del "player" (objeto de pruebas que ya usabas)
    if(e.type==sf::Event::MouseButtonPressed){
        if(e.mouseButton.button==sf::Mouse::Left){
            sf::Vector2f w = screenToWorld(win,{e.mouseButton.x,e.mouseButton.y});
            player.selected = (std::hypot(w.x-player.pos.x, w.y-player.pos.y) < 28.f);
        }
        if(e.mouseButton.button==sf::Mouse::Right){
            if(player.selected){
                sf::Vector2f w = screenToWorld(win,{e.mouseButton.x,e.mouseButton.y});
                player.target = w; player.hasTarget=true;
            }
        }
    }

    if(e.type==sf::Event::MouseButtonPressed){
    if(e.mouseButton.button==sf::Mouse::Left){
        dragging_ = true;
        dragStart_ = worldMouse(win, cam);
    }
    if(e.mouseButton.button==sf::Mouse::Right){
        // movimiento grupal al punto
        sf::Vector2f tgt = worldMouse(win, cam);

        // junta seleccionados: aliados + bulldozer (si quieres excluir bulldozer, quita su bloque)
        std::vector<sf::Vector2f*> movers; movers.reserve(allies_.size()+1);
        for(auto& a : allies_) if(a.alive && a.selected) movers.push_back(&a.target);
        bool bulldozerSelected = false;
        if(bulldozer.alive && bulldozer.selected) { bulldozerSelected = true; movers.push_back(&bulldozer.target); }

        if(!movers.empty()){
            auto off = formationOffsets(movers.size(), 18.f);
            // asigna destino en formación
            for(std::size_t i=0;i<movers.size();++i){
                *(movers[i]) = tgt + off[i];
            }
            // activa hasTarget de los verdaderos objetos
            for(auto& a : allies_) if(a.alive && a.selected){ a.hasTarget = true; }
            if(bulldozerSelected) { bulldozer.hasTarget = true; }
        }else{
            // si nadie está seleccionado, mueve el "player" de pruebas como antes
            if(player.selected){ player.target = tgt; player.hasTarget = true; }
        }
    }
}
if(e.type==sf::Event::MouseButtonReleased && e.mouseButton.button==sf::Mouse::Left){
    dragging_ = false;
    sf::Vector2f end = worldMouse(win, cam);

    // ¿drag o click?
    sf::FloatRect sel(std::min(dragStart_.x,end.x), std::min(dragStart_.y,end.y),
                      std::abs(end.x-dragStart_.x), std::abs(end.y-dragStart_.y));

    bool isClick = (sel.width < 3.f && sel.height < 3.f);
    bool addMode = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);

    if(!addMode){
        // limpia selección previa si no hay Shift
        for(auto& a : allies_) a.selected = false;
        bulldozer.selected = false;
        player.selected = false; // opcional
    }

    if(isClick){
        // selección por click (elige la unidad más cercana dentro de un radio)
        sf::Vector2f w = end;
        float best = 26.f; // radio de selección
        Ally* bestAlly = nullptr;
        for(auto& a : allies_) if(a.alive){
            float d = vlen(a.pos - w);
            if(d < best){ best = d; bestAlly = &a; }
        }
        if(bestAlly) bestAlly->selected = true;
        else{
            // probar bulldozer y player
            if(vlen(bulldozer.pos - w) < 26.f) bulldozer.selected = true;
            else if(vlen(player.pos - w) < 26.f) player.selected = true;
        }
    }else{
        // selección por rectángulo (marquee)
        for(auto& a : allies_){
            if(a.alive) a.selected = sel.contains(a.pos);
        }
        if(bulldozer.alive) bulldozer.selected = sel.contains(bulldozer.pos);
        // player opcional:
        // player.selected = sel.contains(player.pos);
    }
}

    // Toggle Fog
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::N){
        showFog_ = !showFog_;
    }

    // Colas de producción: HQ y Garage
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::Q){
        for (auto& b : buildingsA_) if (b.type == Building::Type::HQ){ b.queue.push_back("Soldier"); break; }
    }
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::E){
        for (auto& b : buildingsA_) if (b.type == Building::Type::Garage){ b.queue.push_back("Tank"); break; }
    }
    // Extras (opcional): Harvester y Minesweeper
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::H){
        for (auto& b : buildingsA_) if (b.type == Building::Type::Garage){ b.queue.push_back("Harvester"); break; }
    }
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::X){
        for (auto& b : buildingsA_) if (b.type == Building::Type::HQ){ b.queue.push_back("Minesweeper"); break; }
    }

    // Construcción con Bulldozer (B)
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::B) {
        sf::Vector2f w = worldMouse(win, cam);
        bulldozerBuildAttempt(w);
    }

    // Colocar mina (M)
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::M){
        sf::Vector2f w = screenToWorld(win, sf::Mouse::getPosition(win));
        mines_.push_back({w, 18.f, true});
    }
}

// ==================== Update ====================
void PlayState::update(float dt){
    auto& win = game.window();

    // init once
    static bool init=false;
    if(!init){
        map.generate(32,20);
        fog.init(map.width(), map.height());
        cam = win.getDefaultView();

        // Player de pruebas (lo mantenemos por compatibilidad con tu base)
        player.pos = {4*64.f+32.f, 4*64.f+32.f};
        player.target = player.pos;

        font.loadFromFile("/usr/share/fonts/TTF/DejaVuSans.ttf"); // puede fallar silencioso en otros SO
        hud.setFont(font); hud.setCharacterSize(16); hud.setFillColor(sf::Color::White);

        // === Recursos (juguetes) ===
        resources_.push_back({ {600.f, 400.f}, 300.f });
        resources_.push_back({ {740.f, 520.f}, 300.f });

        // === Edificios base (A) ===
        buildingsA_.push_back({ Building::Type::HQ,     {200.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Garage, {320.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Depot,  {260.f,200.f}, {}, 0.f });

        // estilo del rectángulo de selección
        dragRect_.setFillColor(sf::Color(0,120,255,40));
        dragRect_.setOutlineColor(sf::Color(0,120,255,200));
        dragRect_.setOutlineThickness(1.f);

        // === Ejército inicial Equipo A ===
        // 5 soldados básicos
        for(int i=0;i<5;i++){
            Ally s;
            s.type  = UnitType::Soldier;
            s.pos   = {220.f + float(i*18), 260.f};
            s.speed = 110.f;
            s.color = sf::Color(60,150,70);
            allies_.push_back(s);
        }
        // 2 volquetas (harvesters)
        for(int i=0;i<2;i++){
            Ally h;
            h.type = UnitType::Harvester;
            h.pos  = {340.f + float(i*20), 260.f};
            h.speed = 90.f;
            h.cargo = 0.f; h.cargoCap = 100.f; h.resIdx = -1; h.waiting = false;
            h.color = sf::Color(220,220,0);
            allies_.push_back(h);
        }
        // 1 busca-minas
        {
            Ally m;
            m.type = UnitType::Minesweeper;
            m.pos  = {285.f, 260.f};
            m.speed= 100.f;
            m.color= sf::Color(120,200,120);
            allies_.push_back(m);
        }

        // === Bulldozer único ===
        bulldozer.pos   = {180.f, 260.f};
        bulldozer.type  = UnitType::Bulldozer;
        bulldozer.speed = 60.f;
        bulldozer.alive = true;

        // Plástico inicial Equipo A
        plastic_ = 300;

        init=true;
    }

    // WASD camera
    float pan=400.f*dt;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::W)) cam.move(0,-pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::S)) cam.move(0, pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::A)) cam.move(-pan,0);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) cam.move( pan,0);
    win.setView(cam);

    // Movimiento del player de pruebas
    if(player.hasTarget){
        sf::Vector2f dir = player.target - player.pos;
        float len = std::hypot(dir.x, dir.y);
        if(len>2.f){
            dir.x/=len; dir.y/=len;
            player.pos += dir * player.speed * dt;
        } else {
            player.hasTarget=false;
        }
    }

    // ===== Minas: afectan a todos (player, aliados, bulldozer) =====
    for (auto& m : mines_) if (m.active) {
        if (vlen(player.pos - m.pos) < m.radius){ m.active = false; player.selected=false; }
        for (auto& a : allies_) if (a.alive && vlen(a.pos - m.pos) < m.radius){ m.active=false; a.alive=false; break; }
        if (bulldozer.alive && vlen(bulldozer.pos - m.pos) < m.radius){ m.active=false; bulldozer.alive=false; }
    }

    if(dragging_){
        sf::Vector2f cur = worldMouse(win, cam);
        sf::FloatRect rect(
            std::min(dragStart_.x, cur.x),
            std::min(dragStart_.y, cur.y),
            std::abs(cur.x - dragStart_.x),
            std::abs(cur.y - dragStart_.y)
        );
        dragRect_.setPosition({rect.left, rect.top});
        dragRect_.setSize({rect.width, rect.height});
        win.draw(dragRect_);
    }
    if(dragging_){
        sf::Vector2f cur = worldMouse(win, cam);
        sf::FloatRect rect(
            std::min(dragStart_.x, cur.x),
            std::min(dragStart_.y, cur.y),
            std::abs(cur.x - dragStart_.x),
            std::abs(cur.y - dragStart_.y)
        );
        dragRect_.setPosition({rect.left, rect.top});
        dragRect_.setSize({rect.width, rect.height});
        win.draw(dragRect_);
    }

    for(auto& a : allies_){
        if(!a.alive) continue;

        // Movimiento por target
        if(a.hasTarget){
            sf::Vector2f d = a.target - a.pos;
            float L = vlen(d);
            if(L>2.f) a.pos += vnorm(d) * a.speed * dt;
            else a.hasTarget = false;
        }

        // ... (resto de lógicas: Harvester, Minesweeper, etc.)
    }
    if(bulldozer.alive){
        sf::CircleShape c(6.f);
        c.setOrigin(6,6);
        c.setPosition(bulldozer.pos);
        c.setFillColor(sf::Color(255,140,0));
        win.draw(c);

        if(bulldozer.selected){
            sf::CircleShape ring(12.f); ring.setOrigin(12,12);
            ring.setPosition(bulldozer.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color::White);
            ring.setOutlineThickness(1.f);
            win.draw(ring);
        }
    }

    // ===== Aliados: mover y comportamientos =====
    auto nearestResource = [&](sf::Vector2f p)->int{
        int idx=-1; float best=1e9f;
        for(int i=0;i<(int)resources_.size();++i){
            if(resources_[i].amount<=0) continue;
            float d = vlen(resources_[i].pos - p);
            if(d<best){ best=d; idx=i; }
        }
        return idx;
    };
    auto depotPos = [&]()->sf::Vector2f{
        for(auto& b: buildingsA_) if(b.type==Building::Type::Depot) return b.pos;
        return sf::Vector2f{260.f,200.f};
    };

    for(auto& a : allies_){
        if(!a.alive) continue;

        // Movimiento por target si lo hay
        if(a.hasTarget){
            sf::Vector2f d = a.target - a.pos;
            float L = vlen(d);
            if(L>2.f){ a.pos += vnorm(d) * a.speed * dt; }
            else { a.hasTarget = false; }
        }

        // Comportamientos por tipo
        if(a.type == UnitType::Harvester){
            // 1) asignación/validación de recurso
            if(a.resIdx==-1 || resources_[a.resIdx].amount<=0){
                a.resIdx = nearestResource(a.pos);
                if(a.resIdx==-1){
                    if(a.cargo<=0.f){ a.waiting = true; continue; } // sin recurso y vacío → idle
                    a.target = depotPos(); a.hasTarget = true; a.waiting=false; // lleva carga → vuelve
                    continue;
                }
            }
            // 2) lleno → ir a depósito
            if(a.cargo >= a.cargoCap - 1e-3f){
                a.target = depotPos(); a.hasTarget = true; a.waiting=false;
                if(vlen(a.pos - a.target) < 16.f){
                    plastic_ += (int)a.cargo;
                    a.cargo = 0.f;
                    a.hasTarget=false;
                }
                continue;
            }
            // 3) ir al recurso o recolectar
            sf::Vector2f rpos = resources_[a.resIdx].pos;
            if(vlen(a.pos - rpos) > 14.f){
                a.target = rpos; a.hasTarget = true; a.waiting=false;
            }else{
                float mineRate = 40.f; // plástico/seg
                float take = std::min({mineRate*dt, a.cargoCap - a.cargo, resources_[a.resIdx].amount});
                a.cargo += take;
                resources_[a.resIdx].amount -= take;
                if(resources_[a.resIdx].amount <= 0.f){ resources_[a.resIdx].amount = 0.f; a.resIdx = -1; }
            }
            if(nearestResource(a.pos)==-1 && a.cargo<=0.f){ a.waiting=true; }
        }
        else if(a.type == UnitType::Minesweeper){
            float detectR = 42.f; // radio de detección
            for(auto& m : mines_) if(m.active){
                if(vlen(a.pos - m.pos) < detectR){ m.active = false; break; }
            }
        }
        else if(a.type == UnitType::Soldier){
            // FUTURO: disparo en línea recta
        }
        else if(a.type == UnitType::Tank){
            // FUTURO: lógica tanque
        }
    }

    // ===== Colas de producción (HQ/Garage) → spawnear unidades =====
    for (auto& b : buildingsA_){
        if (b.queue.empty()){ b.buildTimer = 0.f; continue; }
        if (b.buildTimer <= 0.f) b.buildTimer = 2.0f; // tiempo por item (placeholder)
        else b.buildTimer -= dt;

        if (b.buildTimer <= 0.f){
            std::string item = b.queue.front(); b.queue.erase(b.queue.begin());
            auto spawnAt = b.pos + sf::Vector2f(0,40);

            if (item == "Soldier"){
                if (plastic_ >= costs_["Soldier"]){
                    plastic_ -= costs_["Soldier"];
                    Ally s; s.type=UnitType::Soldier; s.pos=spawnAt; s.speed=110.f; s.color=sf::Color(60,150,70);
                    allies_.push_back(s);
                }
            } else if (item == "Tank"){
                if (plastic_ >= costs_["Tank"]){
                    plastic_ -= costs_["Tank"];
                    Ally t; t.type=UnitType::Tank; t.pos=spawnAt; t.speed=80.f; t.hp=250.f; t.color=sf::Color(30,100,40);
                    allies_.push_back(t);
                }
            } else if (item == "Harvester"){
                if (plastic_ >= costs_["Harvester"]){
                    plastic_ -= costs_["Harvester"];
                    Ally h; h.type=UnitType::Harvester; h.pos=spawnAt; h.speed=90.f; h.cargo=0.f; h.cargoCap=100.f; h.color=sf::Color(220,220,0);
                    allies_.push_back(h);
                }
            } else if (item == "Minesweeper"){
                if (plastic_ >= costs_["Minesweeper"]){
                    plastic_ -= costs_["Minesweeper"];
                    Ally m; m.type=UnitType::Minesweeper; m.pos=spawnAt; m.speed=100.f; m.color=sf::Color(120,200,120);
                    allies_.push_back(m);
                }
            }
        }
    }

    // ===== Construcción (bulldozer) =====
    updateBuildJobs(dt);

    // ===== Fog of War =====
    fog.clear();
    if (showFog_) {
        // Revela player + aliados
        fog.revealCircle(player.pos, 160.f);
        for (auto& a : allies_) if(a.alive){
            fog.revealCircle(a.pos, 140.f);
        }
        if(bulldozer.alive) fog.revealCircle(bulldozer.pos, 140.f);
    } else {
        // Mostrar todo
        fog.revealCircle({map.width()*32.f, map.height()*32.f}, 10000.f);
    }

    // ===== HUD =====
    hud.setString(
        "Plastico: " + std::to_string(plastic_) +
        " | Aliados: " + std::to_string((int)allies_.size()) +
        "\nQ: Soldier  E: Tank  H: Harvester  X: Minesweeper  |  B: Construir HQ  |  M: Mina  |  N: Fog"
    );
    hud.setPosition(cam.getCenter().x - cam.getSize().x/2 + 10, cam.getCenter().y - cam.getSize().y/2 + 10);
}

// ==================== Proyección pantalla→mundo ====================
sf::Vector2f PlayState::screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const{
    return win.mapPixelToCoords(mouse);
}

// ==================== Render ====================
void PlayState::render(sf::RenderWindow& win){
    renderer.draw(win, map, fog);

    // Player de pruebas
    {
        sf::CircleShape body(12.f); body.setOrigin(12,12); body.setPosition(player.pos);
        body.setFillColor(sf::Color(60,150,70));
        win.draw(body);
        if(player.selected){
            sf::CircleShape ring(18.f); ring.setOrigin(18,18); ring.setPosition(player.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color::Green); ring.setOutlineThickness(2.f);
            win.draw(ring);
        }
    }

    // Minas activas
    for (auto& m : mines_) if (m.active) {
        sf::CircleShape c(m.radius);
        c.setOrigin(m.radius, m.radius);
        c.setFillColor(sf::Color(120,60,60));
        c.setPosition(m.pos);
        win.draw(c);
    }

    // Recursos (juguetes)
    for (auto& r : resources_){
        sf::CircleShape c(8.f); c.setOrigin(8,8);
        c.setFillColor(sf::Color(200,180,0));
        c.setPosition(r.pos);
        win.draw(c);
    }

    // Edificios existentes
    for (auto& b : buildingsA_){
        sf::RectangleShape s({40,40});
        s.setOrigin(20,20);
        s.setPosition(b.pos);
        s.setFillColor(
            b.type==Building::Type::HQ     ? sf::Color(120,160,120) :
            b.type==Building::Type::Depot  ? sf::Color(160,160, 80) :
            b.type==Building::Type::Garage ? sf::Color(120,120,180) :
                                             sf::Color(140,140,140)
        );
        win.draw(s);
    }

    // Trabajos de construcción (fantasma + barra)
    drawBuildJobs(win);

    // Bulldozer (si lo mantienes fuera de allies_)
    if(bulldozer.alive){
        sf::CircleShape c(6.f);
        c.setOrigin(6,6);
        c.setPosition(bulldozer.pos);
        c.setFillColor(sf::Color(255,140,0));
        win.draw(c);
    }

    // Aliados (Equipo A)
    for (auto& a : allies_) if(a.alive){
        sf::CircleShape c(8.f); c.setOrigin(8,8);
        c.setPosition(a.pos);
        c.setFillColor(a.color);
        win.draw(c);

        if(a.selected){
            sf::CircleShape ring(12.f); ring.setOrigin(12,12);
            ring.setPosition(a.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color::White);
            ring.setOutlineThickness(1.f);
            win.draw(ring);
        }
        // Indicador de carga de Harvester
        if(a.type==UnitType::Harvester){
            float r = (a.cargo/a.cargoCap);
            sf::RectangleShape back({18,3}); back.setOrigin(9,14); back.setPosition(a.pos);
            back.setFillColor(sf::Color(0,0,0,160)); win.draw(back);
            sf::RectangleShape fill({18*r,2}); fill.setOrigin(9,14); fill.setPosition(a.pos);
            fill.setFillColor(sf::Color(240,240,90)); win.draw(fill);
        }
    }

    // HUD
    win.draw(hud);
}

// ==================== Construcción con Bulldozer ====================
void PlayState::bulldozerBuildAttempt(const sf::Vector2f& pos) {
    // Debe existir un bulldozer vivo
    if (!(bulldozer.alive && bulldozer.type == UnitType::Bulldozer)) return;

    // Tipo por defecto: HQ (podremos elegir por UI en iteración siguiente)
    const int cost = costs_.count("HQ") ? costs_["HQ"] : 50;
    if (plastic_ < cost) return; // no hay recursos, no crear job

    // Reserva el costo al crear el job (evita doble gasto si se cancela)
    plastic_ -= cost;

    // Crea un trabajo de construcción
    BuildJob job;
    job.type      = Building::Type::HQ;
    job.target    = pos;
    job.buildTime = 2.0f;   // placeholder
    job.progress  = 0.f;
    job.active    = true;
    job.started   = false;
    buildJobs_.push_back(job);
}

void PlayState::updateBuildJobs(float dt){
    if (!(bulldozer.alive && bulldozer.type == UnitType::Bulldozer)) return;
    const float arriveRadius = 14.f;

    for (auto& job : buildJobs_){
        if (!job.active) continue;

        if (!job.started){
            // Mover el dozer hacia el punto objetivo
            sf::Vector2f dir = vnorm(job.target - bulldozer.pos);
            bulldozer.vel = dir * bulldozer.speed;
            bulldozer.pos += bulldozer.vel * dt;
            bulldozer.vel *= 0.9f;

            if (vlen(job.target - bulldozer.pos) < arriveRadius){
                job.started = true; // llegó, comienza la obra
            }
        } else {
            // Construyendo
            job.progress += dt;
            if (job.progress >= job.buildTime){
                // Spawn del edificio terminado
                buildingsA_.push_back({ job.type, job.target, {}, 0.f });
                job.active = false;
            }
        }
    }
}

void PlayState::drawBuildJobs(sf::RenderWindow& win){
    for (auto& job : buildJobs_){
        if (!job.active) continue;

        // Fantasma del edificio (cuadrado semi-transparente)
        sf::RectangleShape ghost({40,40});
        ghost.setOrigin(20,20);
        ghost.setPosition(job.target);
        ghost.setFillColor(sf::Color(120,160,120, 120)); // verde traslúcido
        win.draw(ghost);

        if (job.started){
            // Barra de progreso sobre el edificio
            float ratio = std::min(1.f, job.progress / job.buildTime);
            sf::RectangleShape back({42,6}); back.setOrigin(21,20+8);
            back.setPosition(job.target);
            back.setFillColor(sf::Color(0,0,0,180));
            win.draw(back);

            sf::RectangleShape fill({42.f*ratio,4}); fill.setOrigin(21,20+8);
            fill.setPosition(job.target);
            fill.setFillColor(sf::Color(80,220,80));
            win.draw(fill);
        }
    }
}
