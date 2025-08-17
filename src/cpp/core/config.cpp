#include "config.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <bits/std_thread.h>

namespace woved {
  
Config g_config;

bool loadConfig(const std::string& path) {
    try {
        YAML::Node yaml = YAML::LoadFile(path);
        
        // Server config
        if (yaml["server"]) {
            auto srv = yaml["server"];
            g_config.server.bind_address = srv["bind_address"].as<std::string>(g_config.server.bind_address);
            g_config.server.grpc_port = srv["grpc_port"].as<uint16_t>(g_config.server.grpc_port);
            g_config.server.http_port = srv["http_port"].as<uint16_t>(g_config.server.http_port);
            g_config.server.metrics_port = srv["metrics_port"].as<uint16_t>(g_config.server.metrics_port);
            g_config.server.worker_threads = srv["worker_threads"].as<uint32_t>(g_config.server.worker_threads);
        }
        
        // Storage config
        if (yaml["storage"]) {
            auto stor = yaml["storage"];
            g_config.storage.data_dir = stor["data_dir"].as<std::string>(g_config.storage.data_dir);
            g_config.storage.wal_dir = stor["wal_dir"].as<std::string>(g_config.storage.wal_dir);
            g_config.storage.segment_dir = stor["segment_dir"].as<std::string>(g_config.storage.segment_dir);
            
            // WAL config
            if (stor["wal"]) {
                auto wal = stor["wal"];
                g_config.storage.wal.group_commit_ms = wal["group_commit_ms"].as<uint32_t>(g_config.storage.wal.group_commit_ms);
                g_config.storage.wal.fence_every_ms = wal["fence_every_ms"].as<uint32_t>(g_config.storage.wal.fence_every_ms);
                g_config.storage.wal.rotate_bytes = wal["rotate_bytes"].as<uint64_t>(g_config.storage.wal.rotate_bytes);
            }
            
            // B-tree config
            if (stor["btree"]) {
                auto btree = stor["btree"];
                g_config.storage.btree.epsilon = btree["epsilon"].as<float>(g_config.storage.btree.epsilon);
                g_config.storage.btree.adaptive_epsilon = btree["adaptive_epsilon"].as<bool>(g_config.storage.btree.adaptive_epsilon);
                g_config.storage.btree.hot_partition_threshold = btree["hot_partition_threshold"].as<float>(g_config.storage.btree.hot_partition_threshold);
            }
        }
        
        // Apply defaults for any missing values
        applyDefaults(g_config);
        
        return validateConfig(g_config);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

bool validateConfig(const Config& config) {
    // Validate paths exist
    // Validate port ranges
    // Validate memory limits
    // etc.
    return true;
}

void applyDefaults(Config& config) {
    if (config.server.worker_threads == 0) {
        config.server.worker_threads = std::thread::hardware_concurrency();
    }
}


} // namespace woved