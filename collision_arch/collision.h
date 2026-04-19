#pragma once
#include <atomic>
#include <vector>
#include <cstring>
#include "../common.h"


struct alignas(64) TreeNode {
    
    std::atomic<uint64_t> max_seq_seen;
    
    
    MarketUpdate payload;
    uint64_t arrival_tsc; 

    TreeNode() : max_seq_seen(0), arrival_tsc(0) {}

    
    TreeNode(TreeNode&& other) noexcept : max_seq_seen(other.max_seq_seen.load()) {
        payload = other.payload;
        arrival_tsc = other.arrival_tsc;
    }
    
    TreeNode(const TreeNode&) = delete; 
    TreeNode& operator=(const TreeNode&) = delete;
};

class CollisionTree {
public:
    CollisionTree() {
        leaf_nodes_.resize(16); 
    }

    
    bool push(const MarketUpdate& u, uint64_t arrival_time) {
        size_t idx = u.timestamp % leaf_nodes_.size();
        
        
        if (!fight_for_node(leaf_nodes_[idx], u, arrival_time)) {
            return false; 
        }

        
        if (!fight_for_node(root_node_, u, arrival_time)) {
            return false; 
        }

        return true; 
    }

    
    bool read_root(MarketUpdate& result, uint64_t& out_arrival_time) {
        uint64_t seq = root_node_.max_seq_seen.load(std::memory_order_acquire);
        if (seq == 0) return false; 

        
        result = root_node_.payload;
        out_arrival_time = root_node_.arrival_tsc; 
        
        
        if (root_node_.max_seq_seen.load(std::memory_order_relaxed) != seq) {
            return false; 
        }
        return true;
    }

private:
    std::vector<TreeNode> leaf_nodes_;
    TreeNode root_node_;

    bool fight_for_node(TreeNode& node, const MarketUpdate& u, uint64_t tsc) {
        uint64_t current_max = node.max_seq_seen.load(std::memory_order_relaxed);

        while (true) {
            if (current_max >= u.timestamp) {
                return false; 
            }

            if (node.max_seq_seen.compare_exchange_weak(
                current_max, 
                u.timestamp,
                std::memory_order_acquire, 
                std::memory_order_relaxed)) {
                
                
                node.payload = u;
                node.arrival_tsc = tsc; 
                
                std::atomic_thread_fence(std::memory_order_release);
                return true; 
            }
        }
    }
};