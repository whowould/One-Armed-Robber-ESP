#pragma once
#include <cstdint>

namespace off {
    constexpr std::uintptr_t game_instance = 0x3884CD8;
    constexpr std::uintptr_t pawn_vtable = 0x3DF79E8;
    constexpr std::uintptr_t camera_vtable = 0x3EA7E78;

    constexpr std::uintptr_t game_instance_local_players = 0x38;
    constexpr std::uintptr_t local_player_controller = 0x30;
    constexpr std::uintptr_t controller_pawn = 0x250;
    constexpr std::uintptr_t controller_camera = 0x2B8;
    constexpr std::uintptr_t camera_pov = 0xEA0;

    constexpr std::uintptr_t actor_outer = 0x20;
    constexpr std::uintptr_t world_level = 0x30;
    constexpr std::uintptr_t level_actors = 0x98;
    constexpr std::uintptr_t actor_root = 0x130;
    constexpr std::uintptr_t scene_location = 0x11C;
    constexpr std::uintptr_t capsule_half_height = 0x114;

    constexpr int actor_cap = 8192;
    constexpr float self_radius = 150.f;
}
