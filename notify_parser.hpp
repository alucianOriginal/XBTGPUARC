#pragma once
#include "mining_job.hpp"
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <iostream>

inline std::optional<MiningJob> parse_notify(const std::string& json_line) {
    auto get_string_safe = [](const boost::json::value& v) -> std::string {
        if (v.is_string()) return v.as_string().c_str();
        else return {};
    };

    try {
        auto val = boost::json::parse(json_line);
        if (!val.is_object()) return std::nullopt;
        const auto& obj = val.as_object();

        if (!obj.contains("method") || obj.at("method").as_string() != "mining.notify")
            return std::nullopt;

        if (!obj.contains("params")) return std::nullopt;
        const auto& params = obj.at("params").as_array();

        if (params.size() < 9) return std::nullopt;

        MiningJob job;
        job.job_id    = get_string_safe(params[0]);
        job.prevhash  = get_string_safe(params[1]);
        job.coinb1    = get_string_safe(params[2]);
        job.coinb2    = get_string_safe(params[3]);

        job.merkle_branch.clear();
        if (params[4].is_array()) {
            const auto& branch = params[4].as_array();
            for (const auto& el : branch) {
                job.merkle_branch.push_back(get_string_safe(el));
            }
        } else {
            std::cerr << "⚠️ Merkle Branch ist kein Array, wird leer angenommen.\n";
        }

        job.version     = get_string_safe(params[5]);
        job.bits        = get_string_safe(params[6]);
        job.ntime       = get_string_safe(params[7]);
        job.extranonce1 = "";           // wird vom Pool gesetzt
        job.extranonce2 = "00000000";   // Dummy

        return job;

    } catch (const std::exception& e) {
        std::cerr << "❌ parse_notify Fehler: " << e.what() << "\n";
        return std::nullopt;
    }
}
