#include "game.h"
#include "../offsets/offsets.h"
#include <cmath>
#include <cstddef>
#include <cstdio>

std::uintptr_t Game::controller() const {
    if (!game_instance_)
        return 0;
    const auto players = process_.read<std::uintptr_t>(game_instance_ + off::game_instance_local_players);
    const auto count = process_.read<std::int32_t>(game_instance_ + off::game_instance_local_players + 8);
    if (!players || count < 1)
        return 0;
    const auto local = process_.read<std::uintptr_t>(players);
    if (!local)
        return 0;
    return process_.read<std::uintptr_t>(local + off::local_player_controller);
}

bool Game::read_pov(std::uintptr_t manager) {
    float raw[7]{};
    if (!process_.read_bytes(manager + off::camera_pov, raw, sizeof(raw)))
        return false;
    if (raw[6] < 20.f || raw[6] > 140.f)
        return false;
    camera_.origin = { raw[0], raw[1], raw[2] };
    camera_.pitch = raw[3];
    camera_.yaw = raw[4];
    camera_.fov = raw[6];
    return std::isfinite(camera_.origin.x) && std::isfinite(camera_.yaw);
}

bool Game::refresh() {
    camera_ok_ = false;
    pawns_.clear();
    world_ = 0;
    level_ = 0;

    if (!process_.alive() || !process_.read<std::uint16_t>(process_.module_base())) {
        process_.attach(L"OAR-Win64-Shipping.exe");
        game_instance_ = 0;
    }
    if (!process_.alive())
        return false;

    const auto base = process_.module_base();
    const auto vt = base + off::game_instance;
    if (!game_instance_ || process_.read<std::uintptr_t>(game_instance_) != vt) {
        game_instance_ = process_.find_vtable(vt);
        if (game_instance_)
            std::printf("game instance %llx\n", static_cast<unsigned long long>(game_instance_));
    }
    if (!game_instance_)
        return false;

    const auto control = controller();
    const auto me = control ? process_.read<std::uintptr_t>(control + off::controller_pawn) : 0;
    const auto manager = control ? process_.read<std::uintptr_t>(control + off::controller_camera) : 0;
    if (me) {
        level_ = process_.read<std::uintptr_t>(me + off::actor_outer);
        world_ = level_ ? process_.read<std::uintptr_t>(level_ + off::actor_outer) : 0;
        if (!world_ || process_.read<std::uintptr_t>(world_ + off::world_level) != level_) {
            world_ = 0;
            level_ = 0;
        }
    }
    if (manager)
        camera_ok_ = read_pov(manager);
    if (!level_ || !camera_ok_)
        return false;

    const auto actors = process_.read<std::uintptr_t>(level_ + off::level_actors);
    const auto count = process_.read<std::int32_t>(level_ + off::level_actors + 8);
    if (!actors || count < 1 || count > off::actor_cap)
        return true;

    std::vector<std::uintptr_t> list(static_cast<size_t>(count));
    if (!process_.read_bytes(actors, list.data(), list.size() * sizeof(std::uintptr_t)))
        return true;

    const auto pawn_vt = base + off::pawn_vtable;
    const auto camera_vt = base + off::camera_vtable;
    const Vec3 cam = camera_.origin;
    pawns_.reserve(40);

    struct Head {
        std::uintptr_t vtable;
        std::uint8_t pad[off::actor_root - sizeof(std::uintptr_t)];
        std::uintptr_t root;
    };
    static_assert(offsetof(Head, root) == off::actor_root);

    struct RootBlock {
        float half_height;
        std::uint8_t pad[off::scene_location - off::capsule_half_height - sizeof(float)];
        Vec3 pos;
    };
    static_assert(offsetof(RootBlock, pos) == off::scene_location - off::capsule_half_height);

    for (auto actor : list) {
        if (!actor || actor == me)
            continue;
        Head head{};
        if (!process_.read_bytes(actor, &head, sizeof(head)))
            continue;
        if (head.vtable != pawn_vt && head.vtable != camera_vt)
            continue;
        if (!head.root)
            continue;
        const auto kind = head.vtable == camera_vt ? ActorKind::Camera : ActorKind::Guard;
        RootBlock block{};
        if (!process_.read_bytes(head.root + off::capsule_half_height, &block, sizeof(block)))
            continue;
        const auto& pos = block.pos;
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
            continue;
        const float dx = pos.x - cam.x;
        const float dy = pos.y - cam.y;
        const float dz = pos.z - cam.z;
        if (kind == ActorKind::Guard && dx * dx + dy * dy + dz * dz < off::self_radius * off::self_radius)
            continue;
        float half = block.half_height;
        if (kind == ActorKind::Camera)
            half = 18.f;
        else if (!std::isfinite(half) || half < 5.f || half > 300.f)
            half = 88.f;
        pawns_.push_back({ actor, pos, half, kind });
    }
    return true;
}
