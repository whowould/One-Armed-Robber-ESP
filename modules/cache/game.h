#pragma once
#include "../mem/process.h"
#include <vector>

struct Vec3 {
    float x, y, z;
};

struct Camera {
    Vec3 origin;
    float pitch;
    float yaw;
    float fov;
};

enum class ActorKind { Guard, Camera };

struct Pawn {
    std::uintptr_t address;
    Vec3 pos;
    float half_height;
    ActorKind kind;
};

class Game {
public:
    bool refresh();
    const Camera* camera() const { return camera_ok_ ? &camera_ : nullptr; }
    const std::vector<Pawn>& pawns() const { return pawns_; }
    std::uintptr_t world() const { return world_; }
    std::uintptr_t level() const { return level_; }

private:
    Process process_;
    std::uintptr_t game_instance_ = 0;
    std::uintptr_t world_ = 0;
    std::uintptr_t level_ = 0;
    Camera camera_{};
    bool camera_ok_ = false;
    std::vector<Pawn> pawns_;

    std::uintptr_t controller() const;
    bool read_pov(std::uintptr_t manager);
};
