#pragma once
#include <vector>
#include <string>
#include <unordered_map>

#include <SFML/Graphics.hpp>

#include "../core/State.hpp"
#include "../map/TileMap.hpp"
#include "../map/FogOfWar.hpp"
#include "../render/Renderer.hpp"
#include <filesystem>

struct Sprite2D {
    sf::Sprite spr;
    int cols{1};
    int rows{1};
    int frame{0};
    float fps{10.f};
    float t{0.f};
    bool flipX{false};
    sf::Vector2i frameSize{0,0};

    // NUEVO: para “recorrer” solo una fila concreta
    int row{0};            // fila bloqueada (0 = primera)
    int framesInRow{0};    // cuántos frames usar en esa fila (0 = usar toda la grilla normal)
    int startCol{0};       // por si quieres empezar en una columna distinta (normalmente 0)
};


// ==== Components básicos (añadir cerca de tus tipos de unidad) ====
struct Health {
    float hp = 100.f;
    float max = 100.f;
};

struct Attack {
    float dmg = 8.f;        // daño por disparo
    float range = 140.f;    // alcance de disparo en px
    float cooldown = 0.5f;  // tiempo entre disparos
    float tcd = 0.f;        // tiempo restante de cooldown
};

// === Tipos base de unidad ===
enum class UnitType { Soldier, Harvester, Bulldozer, Minesweeper, Tank };



// === Unidad mínima (se usa para "player" de pruebas) ===
struct Unit {
    sf::Vector2f pos{};
    sf::Vector2f target{};
    bool hasTarget{false};
    bool selected{false};
    float speed{120.f};
    sf::Vector2f vel{0.f, 0.f};
    UnitType type{UnitType::Soldier};
    bool alive{true};
    int   team = 0;             // 0=A (jugador), 1=B (enemigo)
    Health health{100.f, 100.f};
    Attack attack{8.f, 140.f, 0.5f, 0.f};
};

// === Unidad del ejército aliado (Equipo A) ===
struct Ally {
    UnitType type{UnitType::Soldier};
    sf::Vector2f pos{};
    sf::Vector2f vel{};
    sf::Vector2f target{};
    bool hasTarget{false};
    bool selected{false};
    bool alive{true};
    float speed{90.f};
    float hp{100.f};
    // Combate básico
    Sprite2D vis;
    float attackCd{0.f};   // cooldown de disparo (seg)
    sf::Color color{sf::Color(60,150,70)};

    // Solo para Harvester (volqueta)
    float cargo{0.f};
    float cargoCap{100.f};
    int   resIdx{-1};
    bool  waiting{false};
};

// === Recursos y Edificios ===
struct ResourceNode {
    sf::Vector2f pos{};
    float amount{300.f};
};

struct Building {
    enum class Type { HQ, Depot, Garage, Fort };
    Type type{Type::HQ};
    sf::Vector2f pos{};
    std::vector<std::string> queue;
    float buildTimer{0.f};
};

// === Minas ===
struct Mine {
    sf::Vector2f pos{};
    float radius{18.f};
    bool active{true};
};

// === Job de construcción (Bulldozer) ===
struct BuildJob {
    Building::Type type{Building::Type::HQ};
    sf::Vector2f   target{};
    float          progress{0.f};     // 0..buildTime
    float          buildTime{2.0f};   // segundos
    bool           active{false};
    bool           started{false};    // ya llegó el dozer al target
};

// --- Defensas fijas: Fortín ---
struct Fort {
    sf::Vector2f pos{};
    float hp{300.f};
    float maxHp{300.f};
    float dmg{25.f};         // daño por disparo
    float range{220.f};      // radio de defensa
    float facingDeg{0.f};    // 0° = hacia +X (derecha)
    float fovDeg{180.f};     // campo de visión en grados
    float fireInterval{0.8f};
    float fireCooldown{0.f};
};
inline std::vector<Fort> forts_;

// (Opcional, para futuras pruebas con enemigos reales):
struct Enemy {
    sf::Vector2f pos{};
    bool alive{true};
    float hp{100.f};
    sf::Color color{sf::Color(200,60,60)};
};
inline std::vector<Enemy> enemies_; // vacío por ahora, el Fort no dispara si no hay enemigos


// --- Decoración del campo ---


class PlayState : public State {
public:
    using State::State;

    void handleEvent(const sf::Event&) override;
    void update(float dt) override;
    void render(sf::RenderWindow&) override;

private:
    // --- Mundo y render ---
    TileMap  map;
    FogOfWar fog;
    Renderer renderer;

    // --- Cámara/HUD ---
    sf::View cam;
    float    zoom{1.f};
    sf::Font font;
    sf::Text hud;
    sf::Texture texSoldierGreen_;
    sf::Texture texSoldierBrown_;

    // --- Input selección (si quisieras selección múltiple más adelante) ---
    bool            dragging_{false};
    sf::Vector2f    dragStart_{};
    sf::RectangleShape dragRect_{};

    // --- Estado de juego (Equipo A, economía, etc.) ---
    std::vector<Ally>     allies_;      // ejército aliado
    std::vector<ResourceNode> resources_;
    std::vector<Building> buildingsA_;
    std::vector<Mine>     mines_;

    int plastic_{300}; // plástico inicial Equipo A (ajustable)

    // Costos (puedes cargarlo desde archivo luego)
    std::unordered_map<std::string, int> costs_{
        {"HQ",50},{"Depot",40},{"Garage",60},
        {"Soldier",10},{"Tank",50},{"Harvester",25},{"Minesweeper",15}
    };



    // Bulldozer dedicado (lo mantenemos fuera del vector para claridad de la demo)
    Unit bulldozer;

    // Construcción
    std::vector<BuildJob> buildJobs_;

    // Fog
    bool showFog_{true};

    // Player de pruebas (mantiene compatibilidad con tu base actual)
    //Unit player;

    // --- Funciones auxiliares (implementadas en PlayState.cpp) ---
    void bulldozerBuildAttempt(const sf::Vector2f& pos);
    void updateBuildJobs(float dt);
    void drawBuildJobs(sf::RenderWindow& win);

    // Proyección de coordenadas
    sf::Vector2f screenToWorld(sf::RenderWindow& win, sf::Vector2i mouse) const;

    // --- Enemigos (Equipo B) + Patrulla ---
    std::vector<Ally> enemies_;                 // reusamos Ally para enemigos
    std::vector<sf::Vector2f> patrolPtsB_;      // waypoints de patrulla
    size_t patrolIdxB_ = 0;                     // índice actual

    // --- Fin de partida ---
    enum class GameOver { None, Win, Lose };
    GameOver gameOver_ = GameOver::None;

};
