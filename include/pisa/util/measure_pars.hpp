#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <map>

#include "query.hpp"


struct block_max_score_query_stat_logging{
    std::string qid;
    size_t term_cnt;
    size_t while_cnt;
    std::map<size_t, size_t> p1_cnt;
    std::map<size_t, size_t> p2_cnt;
    std::map<size_t, size_t> p3_cnt;
    std::map<size_t, size_t> p4_cnt;
    std::map<size_t, size_t> p5_cnt;
    std::map<size_t, size_t> br1_cnt;
    std::map<size_t, size_t> br2_cnt;

    block_max_score_query_stat_logging(std::string id, size_t tc, size_t wc)
    : qid(id), term_cnt(tc), while_cnt(wc) {

    }

    block_max_score_query_stat_logging() {

    }
};

extern std::map<std::string, block_max_score_query_stat_logging> query_stat_logging;
