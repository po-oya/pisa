#pragma once

#include <vector>
#include <string>

#include "concepts.hpp"
#include "concepts/posting_cursor.hpp"
#include "topk_queue.hpp"
// #include "utils/measure_pars.hpp"

namespace pisa {

struct block_max_maxscore_query {
    /******/
    std::string qid;
    /******/

    explicit block_max_maxscore_query(topk_queue& topk) : m_topk(topk) {}
    explicit block_max_maxscore_query(topk_queue& topk, std::string id) : m_topk(topk), qid(id) {}

    template <typename CursorRange>
    PISA_REQUIRES((concepts::BlockMaxPostingCursor<pisa::val_t<CursorRange>>))
    void operator()(CursorRange&& cursors, uint64_t max_docid) {
        using Cursor = typename std::decay_t<CursorRange>::value_type;
        if (cursors.empty()) {
            return;
        }

        std::vector<Cursor*> ordered_cursors;
        ordered_cursors.reserve(cursors.size());
        for (auto& en: cursors) {
            ordered_cursors.push_back(&en);
        }
        auto start_alg_prepartions = std::chrono::steady_clock::now();
        // sort enumerators by increasing maxscore
        std::sort(ordered_cursors.begin(), ordered_cursors.end(), [](Cursor* lhs, Cursor* rhs) {
            return lhs->max_score() < rhs->max_score();
        });

        std::vector<float> upper_bounds(ordered_cursors.size());
        upper_bounds[0] = ordered_cursors[0]->max_score();
        for (size_t i = 1; i < ordered_cursors.size(); ++i) {
            upper_bounds[i] = upper_bounds[i - 1] + ordered_cursors[i]->max_score();
        }

        int non_essential_lists = 0;
        uint64_t cur_doc =
            std::min_element(cursors.begin(), cursors.end(), [](Cursor const& lhs, Cursor const& rhs) {
                return lhs.docid() < rhs.docid();
            })->docid();

        auto end_alg_prepartions = std::chrono::steady_clock::now();
        double alg_prepartions_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_alg_prepartions - start_alg_prepartions).count();
        spdlog::info("Time taken to prepare running alg on this query: {}ms", alg_prepartions_ms);

        auto start_alg_exec = std::chrono::steady_clock::now();
        while (non_essential_lists < ordered_cursors.size() && cur_doc < max_docid) {
            float score = 0;
            uint64_t next_doc = max_docid;
            for (size_t i = non_essential_lists; i < ordered_cursors.size(); ++i) {
                if (ordered_cursors[i]->docid() == cur_doc) {
                    // p1 if
                    score += ordered_cursors[i]->score();
                    ordered_cursors[i]->next();
                }
                if (ordered_cursors[i]->docid() < next_doc) {
                    next_doc = ordered_cursors[i]->docid();
                }
            }

            double block_upper_bound =
                non_essential_lists > 0 ? upper_bounds[non_essential_lists - 1] : 0;
            for (int i = non_essential_lists - 1; i + 1 > 0; --i) {
                if (ordered_cursors[i]->block_max_docid() < cur_doc) {
                    // p2 if
                    ordered_cursors[i]->block_max_next_geq(cur_doc);
                }
                block_upper_bound -=
                    ordered_cursors[i]->max_score() - ordered_cursors[i]->block_max_score();
                if (!m_topk.would_enter(score + block_upper_bound)) {
                    // brz_1
                    break;
                }
            }
            if (m_topk.would_enter(score + block_upper_bound)) {
                // try to complete evaluation with non-essential lists
                // p3
                for (size_t i = non_essential_lists - 1; i + 1 > 0; --i) {
                    ordered_cursors[i]->next_geq(cur_doc);
                    if (ordered_cursors[i]->docid() == cur_doc) {
                        // p4
                        auto s = ordered_cursors[i]->score();
                        block_upper_bound += s;
                    }
                    block_upper_bound -= ordered_cursors[i]->block_max_score();

                    if (!m_topk.would_enter(score + block_upper_bound)) {
                        // brz_2
                        break;
                    }
                }
                score += block_upper_bound;
            }
            if (m_topk.insert(score, cur_doc)) {
                // update non-essential lists
                // p5
                while (non_essential_lists < ordered_cursors.size()
                       && !m_topk.would_enter(upper_bounds[non_essential_lists])) {
                    non_essential_lists += 1;
                }
            }
            cur_doc = next_doc;
        }

        auto end_alg_exec = std::chrono::steady_clock::now();
        double alg_exec_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_alg_exec - start_alg_exec).count();
        spdlog::info("Time taken to for running the alg on this query: {}ms", alg_exec_ms);
    }

    std::vector<typename topk_queue::entry_type> const& topk() const { return m_topk.topk(); }

  private:
    topk_queue& m_topk;
};
}  // namespace pisa
