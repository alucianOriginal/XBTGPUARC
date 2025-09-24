#pragma once

#include "mining_job.hpp"
#include <array>
#include <cstdint>
#include <functional>

void miner_loop(const MiningJob& job,
                const std::function<void(uint32_t, const std::array<uint8_t, 32>&, const MiningJob&)>& on_valid_share,
                int intensity); // ← intensity hier hinzufügen

