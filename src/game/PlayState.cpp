#include "PlayState.hpp"
#include "../core/Game.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <filesystem>

// ==================== Assets helper ====================
static std::string spritePath(const std::string& rel) {
    namespace fs = std::filesystem;

    // 1) Ejecutando desde build/
    fs::path p1 = fs::path("src/ArmyMenRTS_Sprites") / rel;
    if (fs::exists(p1)) return p1.string();

    // 2) Ejecutando desde raíz del repo
    fs::path p2 = fs::path("../src/ArmyMenRTS_Sprites") / rel;
    if (fs::exists(p2)) return p2.string();

    // 3) Ejecutando desde src/ (algunos IDEs)
    fs::path p3 = fs::path("../../src/ArmyMenRTS_Sprites") / rel;
    if (fs::exists(p3)) return p3.string();

    // 4) Último intento: tal cual
    return (fs::path("src/ArmyMenRTS_Sprites") / rel).string();
}

// ==================== Spritesheet helpers ====================
static void initGrid(Sprite2D& s, sf::Texture& tex, int cols, int rows, float fps = 10.f) {
    s.spr.setTexture(tex);
    s.cols = std::max(1, cols);
    s.rows = std::max(1, rows);
    s.fps  = fps;
    s.frame = 0;
    s.t = 0.f;
    s.row = 0;
    s.framesInRow = 0;  // 0 = sin restricción
    s.startCol = 0;

    auto sz = tex.getSize();
    s.frameSize = { int(sz.x / unsigned(s.cols)), int(sz.y / unsigned(s.rows)) };
    s.spr.setTextureRect(sf::IntRect(0, 0, s.frameSize.x, s.frameSize.y));
    s.spr.setOrigin(float(s.frameSize.x)/2.f, float(s.frameSize.y)/2.f);
}

static void initGridRow(Sprite2D& s, sf::Texture& tex,
                        int cols, int rows, int rowIndex,
                        int framesInRow, float fps = 10.f) {
    s.spr.setTexture(tex);
    s.cols = std::max(1, cols);
    s.rows = std::max(1, rows);
    s.row  = std::clamp(rowIndex, 0, s.rows - 1);
    s.framesInRow = std::clamp(framesInRow, 1, s.cols);
    s.startCol = 0;
    s.fps  = fps;
    s.frame = 0;
    s.t = 0.f;

    auto sz = tex.getSize();
    s.frameSize = { int(sz.x / unsigned(s.cols)), int(sz.y / unsigned(s.rows)) };
    s.spr.setTextureRect(sf::IntRect(0, s.row * s.frameSize.y, s.frameSize.x, s.frameSize.y));
    s.spr.setOrigin(float(s.frameSize.x)/2.f, float(s.frameSize.y)/2.f);
}

static void updateAnim(Sprite2D& s, float dt) {
    const bool lockRow = (s.framesInRow > 0);
    const int total = lockRow ? s.framesInRow : (s.cols * s.rows);
    if (total <= 1 || s.fps <= 0.f) return;

    s.t += dt;
    const float step = 1.f / s.fps;
    while (s.t >= step) {
        s.t -= step;
        s.frame = (s.frame + 1) % total;

        int fx, fy;
        if (lockRow) {
            fx = (s.startCol + s.frame) % s.cols;
            fy = s.row;
        } else {
            int linear = s.frame;
            fx = linear % s.cols;
            fy = linear / s.cols;
        }

        s.spr.setTextureRect(sf::IntRect(
            fx * s.frameSize.x,
            fy * s.frameSize.y,
            s.frameSize.x,
            s.frameSize.y
        ));
    }
}

static void setFlipX(Sprite2D& s, bool flip) {
    if (flip == s.flipX) return;
    s.flipX = flip;
    auto sc = s.spr.getScale();
    float sx = (flip ? -1.f : 1.f) * (sc.x == 0.f ? 1.f : std::abs(sc.x));
    s.spr.setScale(sx, sc.y == 0.f ? 1.f : sc.y);
}

// ==================== Math & draw helpers ====================
static float vlen(sf::Vector2f v){ return std::sqrt(v.x*v.x + v.y*v.y); }
static sf::Vector2f vnorm(sf::Vector2f v){ float L=vlen(v); return (L>0)? v*(1.f/L) : sf::Vector2f(0.f,0.f); }
static sf::Vector2f worldMouse(sf::RenderWindow& win, const sf::View& cam){
    auto p = sf::Mouse::getPosition(win);
    return win.mapPixelToCoords(p, cam);
}

static float rad2deg(float r){ return r*180.f/3.1415926535f; }
static float deg2rad(float d){ return d*3.1415926535f/180.f; }
static float angDeg(sf::Vector2f v){ return rad2deg(std::atan2(v.y, v.x)); }
static float angDiff(float a, float b){ // diferencia mínima en grados [-180,180]
    float d = std::fmod(a-b+540.f, 360.f) - 180.f;
    return d;
}

static void drawArc(sf::RenderWindow& win, sf::Vector2f center, float radius, float startDeg, float endDeg, sf::Color fill, int steps=24){
    sf::VertexArray fan(sf::TriangleFan);
    fan.append(sf::Vertex(center, fill));
    float span = endDeg - startDeg;
    for(int i=0;i<=steps;i++){
        float t = (float)i/(float)steps;
        float a = deg2rad(startDeg + span*t);
        sf::Vector2f p = center + sf::Vector2f(std::cos(a), std::sin(a))*radius;
        fan.append(sf::Vertex(p, fill));
    }
    win.draw(fan);
}

// Distancia al AABB más cercano (para círculo vs rectángulo)
static float distCircleAABB(sf::Vector2f c, float r, sf::Vector2f rectCenter, sf::Vector2f halfExtents){
    sf::Vector2f d = { std::abs(c.x - rectCenter.x), std::abs(c.y - rectCenter.y) };
    sf::Vector2f q = { std::max(d.x - halfExtents.x, 0.f), std::max(d.y - halfExtents.y, 0.f) };
    return std::sqrt(q.x*q.x + q.y*q.y) - r;
}

// Push mínimo para resolver penetración círculo vs círculo
static void resolveCircleCircle(sf::Vector2f& pA, float rA, sf::Vector2f& pB, float rB){
    sf::Vector2f d = pB - pA;
    float L2 = d.x*d.x + d.y*d.y;
    float r = rA + rB;
    if (L2 <= 0.0001f) { pA.x -= rA*0.5f; pB.x += rB*0.5f; return; }
    float L = std::sqrt(L2);
    if (L >= r) return;
    float pen = r - L;
    sf::Vector2f n = d * (1.f / L);
    pA -= n * (pen * 0.5f);
    pB += n * (pen * 0.5f);
}

// Push mínimo para resolver penetración círculo vs AABB centrado
static void resolveCircleAABB(sf::Vector2f& p, float r, sf::Vector2f rectCenter, sf::Vector2f halfExtents){
    sf::Vector2f nearest{
        std::max(rectCenter.x - halfExtents.x, std::min(p.x, rectCenter.x + halfExtents.x)),
        std::max(rectCenter.y - halfExtents.y, std::min(p.y, rectCenter.y + halfExtents.y))
    };
    sf::Vector2f d = p - nearest;
    float L2 = d.x*d.x + d.y*d.y;
    if (L2 <= 0.0001f){
        if (std::abs(p.x - rectCenter.x) > std::abs(p.y - rectCenter.y))
            p.x = (p.x < rectCenter.x) ? rectCenter.x - halfExtents.x - r : rectCenter.x + halfExtents.x + r;
        else
            p.y = (p.y < rectCenter.y) ? rectCenter.y - halfExtents.y - r : rectCenter.y + halfExtents.y + r;
        return;
    }
    float L = std::sqrt(L2);
    if (L >= r) return;
    float pen = r - L;
    sf::Vector2f n = d * (1.f / L);
    p += n * pen;
}

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

    // Selección drag / movimiento grupal
    if(e.type==sf::Event::MouseButtonPressed){
        if(e.mouseButton.button==sf::Mouse::Left){
            dragging_ = true;
            dragStart_ = worldMouse(win, cam);
        }
        if(e.mouseButton.button==sf::Mouse::Right){
            sf::Vector2f tgt = worldMouse(win, cam);

            std::vector<sf::Vector2f*> movers; movers.reserve(allies_.size()+1);
            for(auto& a : allies_) if(a.alive && a.selected) movers.push_back(&a.target);
            bool bulldozerSelected = false;
            if(bulldozer.alive && bulldozer.selected) { bulldozerSelected = true; movers.push_back(&bulldozer.target); }

            if(!movers.empty()){
                auto off = formationOffsets(movers.size(), 18.f);
                for(std::size_t i=0;i<movers.size();++i) *(movers[i]) = tgt + off[i];
                for(auto& a : allies_) if(a.alive && a.selected) a.hasTarget = true;
                if(bulldozerSelected) bulldozer.hasTarget = true;
            }
        }
    }

    if(e.type==sf::Event::MouseButtonReleased && e.mouseButton.button==sf::Mouse::Left){
        dragging_ = false;
        sf::Vector2f end = worldMouse(win, cam);

        sf::FloatRect sel(std::min(dragStart_.x,end.x), std::min(dragStart_.y,end.y),
                          std::abs(end.x-dragStart_.x), std::abs(end.y-dragStart_.y));

        bool isClick = (sel.width < 3.f && sel.height < 3.f);
        bool addMode = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);

        if(!addMode){
            for(auto& a : allies_) a.selected = false;
            bulldozer.selected = false;
        }

        if(isClick){
            sf::Vector2f w = end;
            float best = 26.f;
            Ally* bestAlly = nullptr;
            for(auto& a : allies_) if(a.alive){
                float d = vlen(a.pos - w);
                if(d < best){ best = d; bestAlly = &a; }
            }
            if(bestAlly) bestAlly->selected = true;
            else if(vlen(bulldozer.pos - w) < 26.f) bulldozer.selected = true;
        }else{
            for(auto& a : allies_) if(a.alive) a.selected = sel.contains(a.pos);
            if(bulldozer.alive) bulldozer.selected = sel.contains(bulldozer.pos);
        }
    }

    // Toggle Fog
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::N) showFog_ = !showFog_;

    // Colas de producción: HQ/Garage
    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::Q)
        for (auto& b : buildingsA_) if (b.type == Building::Type::HQ){ b.queue.push_back("Soldier"); break; }

    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::E)
        for (auto& b : buildingsA_) if (b.type == Building::Type::Garage){ b.queue.push_back("Tank"); break; }

    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::H)
        for (auto& b : buildingsA_) if (b.type == Building::Type::Garage){ b.queue.push_back("Harvester"); break; }

    if (e.type==sf::Event::KeyPressed && e.key.code==sf::Keyboard::X)
        for (auto& b : buildingsA_) if (b.type == Building::Type::HQ){ b.queue.push_back("Minesweeper"); break; }

    // Construcción con Bulldozer (B)
    // AHORA: construcción por teclas (B=HQ, D=Depot, G=Garage)
    if (e.type == sf::Event::KeyPressed) {
        if (e.key.code == sf::Keyboard::B) {
            sf::Vector2f w = worldMouse(win, cam);
            bulldozerBuildAttempt(w, Building::Type::HQ);
        }
        if (e.key.code == sf::Keyboard::D) {
            sf::Vector2f w = worldMouse(win, cam);
            bulldozerBuildAttempt(w, Building::Type::Depot);
        }
        if (e.key.code == sf::Keyboard::G) {
            sf::Vector2f w = worldMouse(win, cam);
            bulldozerBuildAttempt(w, Building::Type::Garage);
        }
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

        if (!font.loadFromFile(spritePath("ui/DejaVuSans.ttf")))
            std::cerr << "WARN: Coloca DejaVuSans.ttf en src/ArmyMenRTS_Sprites/ui/\n";

        hud.setFont(font); hud.setCharacterSize(16); hud.setFillColor(sf::Color::White);

        if (!texSoldierGreen_.loadFromFile(spritePath("units/soldier_green.png")))
            std::cerr << "WARN: falta src/ArmyMenRTS_Sprites/units/soldier_green.png\n";
        if (!texSoldierBrown_.loadFromFile(spritePath("units/soldier_brown.png")))
            std::cerr << "WARN: falta src/ArmyMenRTS_Sprites/units/soldier_brown.png\n";
        texSoldierGreen_.setSmooth(false);
        texSoldierBrown_.setSmooth(false);

        // Recursos (juguetes)
        resources_.push_back({ {600.f, 400.f}, 300.f });
        resources_.push_back({ {740.f, 520.f}, 300.f });

        // Edificios base
        buildingsA_.push_back({ Building::Type::HQ,     {200.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Garage, {320.f,200.f}, {}, 0.f });
        buildingsA_.push_back({ Building::Type::Depot,  {260.f,200.f}, {}, 0.f });

        // Fortín inicial
        forts_.push_back(Fort{ {240.f, 240.f}, 300.f, 300.f, 25.f, 220.f, 0.f, 180.f, 0.8f, 0.f });

        // estilo del rectángulo de selección
        dragRect_.setFillColor(sf::Color(0,120,255,40));
        dragRect_.setOutlineColor(sf::Color(0,120,255,200));
        dragRect_.setOutlineThickness(1.f);

        // Ejército inicial
        for(int i=0;i<5;i++){
            Ally s;
            initGridRow(s.vis, texSoldierGreen_, 8, 2, 0, 8, 10.f);
            s.vis.spr.setScale(0.10f, 0.10f);
            s.type  = UnitType::Soldier;
            s.pos   = {220.f + float(i*18), 260.f};
            s.speed = 110.f;
            s.color = sf::Color(60,150,70);
            allies_.push_back(s);
        }
        {
            Ally h;
            h.type = UnitType::Harvester;
            h.pos  = {340.f, 260.f};
            h.speed = 90.f;
            h.cargo = 0.f; h.cargoCap = 100.f; h.resIdx = -1; h.waiting = false;
            h.color = sf::Color(220,220,0);
            allies_.push_back(h);
        }
        {
            Ally m;
            m.type = UnitType::Minesweeper;
            initGridRow(m.vis, texSoldierGreen_, 8, 2, 1, 8, 10.f);
            m.vis.spr.setScale(0.10f, 0.10f);
            m.pos  = {285.f, 260.f};
            m.speed= 100.f;
            m.color= sf::Color(120,200,120);
            allies_.push_back(m);
        }

        // Bulldozer
        bulldozer.pos   = {180.f, 260.f};
        bulldozer.type  = UnitType::Bulldozer;
        bulldozer.speed = 60.f;
        bulldozer.alive = true;

        // Plástico inicial
        plastic_ = 300;

        // Enemigos estáticos de prueba
        enemies_.clear();
        for (int i = 0; i < 6; ++i) {
            Ally e;
            initGridRow(e.vis, texSoldierBrown_, 8, 2, 0, 8, 0.f);
            e.vis.spr.setScale(0.10f, 0.10f);
            e.type  = UnitType::Soldier;
            e.pos   = { 900.f + float(i*40), 420.f };
            e.speed = 0.f;
            e.hp    = 100.f;
            e.color = sf::Color(120,120,220);
            e.alive = true;
            enemies_.push_back(e);
        }

        init=true;
    }

    // WASD camera
    float pan=400.f*dt;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::W)) cam.move(0,-pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::S)) cam.move(0, pan);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::A)) cam.move(-pan,0);
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) cam.move( pan,0);
    win.setView(cam);

    // Minas
    for (auto& m : mines_) if (m.active) {
        for (auto& a : allies_) if (a.alive && vlen(a.pos - m.pos) < m.radius){ m.active=false; a.alive=false; break; }
        if (bulldozer.alive && vlen(bulldozer.pos - m.pos) < m.radius){ m.active=false; bulldozer.alive=false; }
    }

    // Fortines
    for(auto& f : forts_){
        if(f.hp <= 0.f) continue;
        if(f.fireCooldown > 0.f) f.fireCooldown -= dt;

        int best = -1; float bestD = 1e9f;
        for(int i=0;i<(int)enemies_.size();++i){
            auto& e = enemies_[i];
            if(!e.alive) continue;
            float d = vlen(e.pos - f.pos);
            if(d > f.range) continue;

            float dirTo = angDeg(e.pos - f.pos);
            float diff  = std::abs(angDiff(dirTo, f.facingDeg));
            if(diff > f.fovDeg*0.5f) continue;

            if(d < bestD){ bestD=d; best=i; }
        }
        if(best != -1 && f.fireCooldown <= 0.f){
            enemies_[best].hp -= f.dmg;
            if(enemies_[best].hp <= 0.f){ enemies_[best].hp = 0.f; enemies_[best].alive=false; }
            f.fireCooldown = f.fireInterval;
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
    // Depósito más cercano y su posición
    auto nearestDepot = [&] (sf::Vector2f p) -> std::pair<int,sf::Vector2f> {
        int idx = -1;
        float best = 1e9f;
        sf::Vector2f bestPos{260.f,200.f}; // fallback
        for (int i=0; i<(int)buildingsA_.size(); ++i){
            if (buildingsA_[i].type != Building::Type::Depot) continue;
            float d = vlen(buildingsA_[i].pos - p);
            if (d < best){ best = d; idx = i; bestPos = buildingsA_[i].pos; }
        }
        return {idx, bestPos};
    };
    const float DEPOT_RADIUS = 56.f;

    // Radios por tipo (para colisiones)
    auto typeRadius = [](UnitType t)->float{
        switch(t){
            case UnitType::Soldier:     return 8.f;
            case UnitType::Minesweeper: return 8.f;
            case UnitType::Harvester:   return 10.f;
            case UnitType::Tank:        return 12.f;
            case UnitType::Bulldozer:   return 10.f;
            default: return 8.f;
        }
    };
    for (auto& a : allies_) a.radius = typeRadius(a.type);

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
                    if(a.cargo<=0.f){ a.waiting = true; }
                    else {
                        auto [di, dpos] = nearestDepot(a.pos);
                        if (di != -1){ a.target = dpos; a.hasTarget = true; a.waiting=false; }
                    }
                    goto NEXT_ALLY;
                }
            }

            // 2) lleno (o sin recurso pero con carga) → ir al depósito
            if(a.cargo >= a.cargoCap - 1e-3f){
                auto [di, dpos] = nearestDepot(a.pos);
                if (di != -1){
                    float dist = vlen(a.pos - dpos);
                    if (dist > DEPOT_RADIUS){
                        a.target = dpos; a.hasTarget = true; a.waiting=false;
                    } else {
                        // dentro del radio → depositar
                        plastic_ += (int)a.cargo;
                        a.cargo = 0.f;
                        a.hasTarget = false;
                    }
                }
                goto NEXT_ALLY;
            }

            // 3) ir al recurso o recolectar
            {
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
            }

            if(nearestResource(a.pos)==-1 && a.cargo<=0.f){ a.waiting=true; }

            // Salida limpia de esta iteración de aliado
            NEXT_ALLY:;
        }
        else if(a.type == UnitType::Minesweeper){
            float detectR = 42.f;
            for(auto& m : mines_) if(m.active){
                if(vlen(a.pos - m.pos) < detectR){ m.active = false; break; }
            }
        }
        else if(a.type == UnitType::Soldier){
            // Combate hitscan simple
            const float dmg = 8.f;
            const float rng = 140.f;
            const float cd  = 0.5f;

            a.attackCd = std::max(0.f, a.attackCd - dt);

            Ally* tgt = nullptr; float best = 1e9f;
            for (auto& e : enemies_) if (e.alive) {
                float d = vlen(e.pos - a.pos);
                if (d < rng && d < best) { best = d; tgt = &e; }
            }
            if (tgt) setFlipX(a.vis, (tgt->pos.x - a.pos.x) < 0.f);

            if (tgt && a.attackCd <= 0.f) {
                tgt->hp -= dmg;
                a.attackCd = cd;
                if (tgt->hp <= 0.f) tgt->alive = false;
            }
        }
        else if(a.type == UnitType::Tank){
            // FUTURO: lógica tanque
        }

        // Actualizar animación y sprite (aliados)
        if (a.vis.spr.getTexture()){
            if (a.vis.fps > 0.f) updateAnim(a.vis, dt);
            a.vis.spr.setPosition(a.pos);
        }
    }

    // Compactar recursos agotados (desaparecen del mapa)
    {
        std::size_t before = resources_.size();
        resources_.erase(
            std::remove_if(resources_.begin(), resources_.end(),
                           [](const ResourceNode& r){ return r.amount <= 0.f; }),
            resources_.end()
        );
        if (resources_.size() != before) {
            for (auto& a : allies_) if (a.alive && a.type == UnitType::Harvester) {
                a.resIdx = -1; a.waiting = false;
            }
        }
    }

    // Enemigos (estáticos): solo animación/sprite
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        if (e.vis.fps > 0.f) updateAnim(e.vis, dt);
        e.vis.spr.setPosition(e.pos);
    }

    // Producción (HQ/Garage)
    for (auto& b : buildingsA_){
        if (b.queue.empty()){ b.buildTimer = 0.f; continue; }
        if (b.buildTimer <= 0.f) b.buildTimer = 2.0f;
        else b.buildTimer -= dt;

        if (b.buildTimer <= 0.f){
            std::string item = b.queue.front(); b.queue.erase(b.queue.begin());
            auto spawnAt = b.pos + sf::Vector2f(0,40);

            if (item == "Soldier"){
                if (plastic_ >= costs_["Soldier"]){
                    plastic_ -= costs_["Soldier"];
                    Ally s; s.type=UnitType::Soldier; s.pos=spawnAt; s.speed=110.f; s.color=sf::Color(60,150,70);
                    initGridRow(s.vis, texSoldierGreen_, 8, 2, 0, 8, 10.f);
                    s.vis.spr.setScale(0.10f, 0.10f);
                    s.vis.spr.setPosition(s.pos);
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
                    initGridRow(m.vis, texSoldierGreen_, 8, 2, 1, 8, 10.f);
                    m.vis.spr.setScale(0.10f, 0.10f);
                    m.vis.spr.setPosition(m.pos);
                    allies_.push_back(m);
                }
            }
        }
    }

    // Construcción
    updateBuildJobs(dt);

    // Fog of War
    fog.clear();
    if (showFog_) {
        for (auto& a : allies_) if(a.alive) fog.revealCircle(a.pos, 140.f);
        if(bulldozer.alive) fog.revealCircle(bulldozer.pos, 140.f);
    } else {
        fog.revealCircle({map.width()*32.f, map.height()*32.f}, 10000.f);
    }

    // Win/Lose mínimo
    if (gameOver_ == GameOver::None) {
        bool anyEnemyAlive = std::any_of(enemies_.begin(), enemies_.end(),
                                         [](const Ally& u){ return u.alive; });
        if (!anyEnemyAlive) gameOver_ = GameOver::Win;

        int aliveA_buildings = (int)buildingsA_.size();
        bool dozerDead = !bulldozer.alive;
        if (aliveA_buildings == 0 || dozerDead) gameOver_ = GameOver::Lose;
    }

    // Colisiones / separación
    const int relaxIters = 2;
    for (int it=0; it<relaxIters; ++it){
        // unidades entre sí
        for (std::size_t i=0; i<allies_.size(); ++i){
            if (!allies_[i].alive) continue;
            for (std::size_t j=i+1; j<allies_.size(); ++j){
                if (!allies_[j].alive) continue;
                resolveCircleCircle(allies_[i].pos, allies_[i].radius,
                                    allies_[j].pos, allies_[j].radius);
            }
        }
        // unidades vs edificios
        for (auto& a : allies_){
            if (!a.alive) continue;
            for (auto& b : buildingsA_){
                resolveCircleAABB(a.pos, a.radius, b.pos, {20.f, 20.f});
            }
        }
        // bulldozer vs todos
        float dozerR = 10.f;
        if (bulldozer.alive){
            for (auto& a : allies_){
                if (!a.alive) continue;
                resolveCircleCircle(bulldozer.pos, dozerR, a.pos, a.radius);
            }
            for (auto& b : buildingsA_){
                resolveCircleAABB(bulldozer.pos, dozerR, b.pos, {20.f, 20.f});
            }
        }
    }

    // HUD
    hud.setString(
        "Plastico: " + std::to_string(plastic_) +
        " | Aliados: " + std::to_string((int)allies_.size()) +
        "\nQ: Soldier  E: Tank  H: Harvester  X: Minesweeper"
        "  |  B: HQ  D: Depot  G: Garage"
        "  |  M: Mina  |  N: Fog"
    );

    hud.setPosition(cam.getCenter().x - cam.getSize().x/2 + 10, cam.getCenter().y - cam.getSize().y/2 + 10);
}

// ==================== Proyección pantalla→mundo ====================
sf::Vector2f PlayState::screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const{
    return win.mapPixelToCoords(mouse, cam);
}

// ==================== Render ====================
void PlayState::render(sf::RenderWindow& win){
    // Mapa + Fog (tu renderer original)
    renderer.draw(win, map, fog);

    // Minas activas
    for (auto& m : mines_) if (m.active) {
        sf::CircleShape c(m.radius);
        c.setOrigin(m.radius, m.radius);
        c.setFillColor(sf::Color(120,60,60));
        c.setPosition(m.pos);
        win.draw(c);
    }

    // Recursos (juguetes) — con efecto de encogimiento si se agotan
    for (auto& r : resources_){
        if (r.amount <= 0.f) continue;
        float t = std::clamp(r.amount / 300.f, 0.f, 1.f);
        float rad = 8.f * (0.5f + 0.5f * t);
        sf::CircleShape c(rad);
        c.setOrigin(rad, rad);
        c.setFillColor(sf::Color(200,180,0, 180));
        c.setPosition(r.pos);
        win.draw(c);
    }

    // Edificios + (opcional) radio de recepción del Depósito
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

        if (b.type == Building::Type::Depot){
            constexpr float DEPOT_RADIUS = 56.f;
            sf::CircleShape range(DEPOT_RADIUS);
            range.setOrigin(DEPOT_RADIUS, DEPOT_RADIUS);
            range.setPosition(b.pos);
            range.setFillColor(sf::Color(0,0,0,0));
            range.setOutlineColor(sf::Color(200,200,80,90));
            range.setOutlineThickness(1.f);
            win.draw(range);
        }
    }

    // Trabajos de construcción (fantasma + barra)
    drawBuildJobs(win);

    // Bulldozer
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

    // Aliados
    for (auto& a : allies_) if(a.alive){
        if (a.vis.spr.getTexture()) win.draw(a.vis.spr);
        else {
            sf::CircleShape c(8.f); c.setOrigin(8,8);
            c.setPosition(a.pos);
            c.setFillColor(a.color);
            win.draw(c);
        }
        if(a.selected){
            sf::CircleShape ring(12.f); ring.setOrigin(12,12);
            ring.setPosition(a.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color::White);
            ring.setOutlineThickness(1.f);
            win.draw(ring);
        }
        if(a.type==UnitType::Harvester){
            float r = (a.cargo/a.cargoCap);
            sf::RectangleShape back({18,3}); back.setOrigin(9,14); back.setPosition(a.pos);
            back.setFillColor(sf::Color(0,0,0,160)); win.draw(back);
            sf::RectangleShape fill({18*r,2}); fill.setOrigin(9,14); fill.setPosition(a.pos);
            fill.setFillColor(sf::Color(240,240,90)); win.draw(fill);
        }
    }

    // Fortines (hexágono + arco de visión)
    for(auto& f : forts_){
        sf::ConvexShape hex; hex.setPointCount(6);
        float R = 18.f;
        for(int k=0;k<6;k++){
            float a = deg2rad(60.f*k - 30.f);
            hex.setPoint(k, sf::Vector2f(std::cos(a)*R, std::sin(a)*R));
        }
        hex.setOrigin(0.f, 0.f);
        hex.setPosition(f.pos);
        hex.setFillColor(sf::Color(100,130,160));
        win.draw(hex);

        // HP bar
        float w = 36.f;
        float ratio = std::max(0.f, f.hp/f.maxHp);
        sf::RectangleShape back({w+2.f, 5.f}); back.setOrigin((w+2.f)*0.5f, R+12.f);
        back.setPosition(f.pos); back.setFillColor(sf::Color(0,0,0,180));
        win.draw(back);
        sf::RectangleShape fill({w*ratio, 3.f}); fill.setOrigin((w)*0.5f, R+12.f);
        fill.setPosition(f.pos); fill.setFillColor(sf::Color(50,220,60));
        win.draw(fill);

        // FOV arc
        float start = f.facingDeg - f.fovDeg*0.5f;
        float end   = f.facingDeg + f.fovDeg*0.5f;
        drawArc(win, f.pos, f.range, start, end, sf::Color(120, 180, 220, 28), 36);

        // Facing ray
        sf::Vertex ray[2];
        ray[0] = sf::Vertex(f.pos, sf::Color(160,200,230,150));
        ray[1] = sf::Vertex(f.pos + sf::Vector2f(std::cos(deg2rad(f.facingDeg)), std::sin(deg2rad(f.facingDeg)))* (R+14.f),
                            sf::Color(160,200,230,150));
        win.draw(ray, 2, sf::Lines);
    }

    // Enemigos
    for (auto& e : enemies_) if (e.alive) {
        if (e.vis.spr.getTexture()) win.draw(e.vis.spr);
        else {
            sf::CircleShape c(8.f); c.setOrigin(8,8);
            c.setPosition(e.pos);
            c.setFillColor(sf::Color(200,60,60));
            win.draw(c);
        }
    }

    // Rectángulo de selección (durante drag)
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

    if (gameOver_ != GameOver::None) {
        sf::Text t; t.setFont(font);
        t.setCharacterSize(48);
        t.setString(gameOver_==GameOver::Win ? "YOU WIN" : "YOU LOSE");
        t.setFillColor(sf::Color::White);
        t.setOutlineColor(sf::Color::Black);
        t.setOutlineThickness(2.f);
        t.setPosition(cam.getCenter().x - 140.f, cam.getCenter().y - 40.f);
        win.draw(t);
    }

    // HUD
    win.draw(hud);
}

void PlayState::bulldozerBuildAttempt(const sf::Vector2f& pos, Building::Type type) {
    if (!(bulldozer.alive && bulldozer.type == UnitType::Bulldozer)) return;

    // Coste según el tipo
    int cost = 0;
    switch (type) {
        case Building::Type::HQ:     cost = costs_.count("HQ")     ? costs_["HQ"]     : 50; break;
        case Building::Type::Depot:  cost = costs_.count("Depot")  ? costs_["Depot"]  : 40; break;
        case Building::Type::Garage: cost = costs_.count("Garage") ? costs_["Garage"] : 60; break;
    }
    if (plastic_ < cost) return;

    plastic_ -= cost;

    BuildJob job;
    job.type      = type;
    job.target    = pos;
    job.buildTime = 2.0f;
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
            sf::Vector2f dir = vnorm(job.target - bulldozer.pos);
            bulldozer.vel = dir * bulldozer.speed;
            bulldozer.pos += bulldozer.vel * dt;
            bulldozer.vel *= 0.9f;

            if (vlen(job.target - bulldozer.pos) < arriveRadius){
                job.started = true;
            }
        } else {
            job.progress += dt;
            if (job.progress >= job.buildTime){
                buildingsA_.push_back({ job.type, job.target, {}, 0.f });
                job.active = false;
            }
        }
    }
}

void PlayState::drawBuildJobs(sf::RenderWindow& win){
    for (auto& job : buildJobs_){
        if (!job.active) continue;

        sf::RectangleShape ghost({40,40});
        ghost.setOrigin(20,20);
        ghost.setPosition(job.target);
        ghost.setFillColor(sf::Color(120,160,120, 120));
        win.draw(ghost);

        if (job.started){
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
