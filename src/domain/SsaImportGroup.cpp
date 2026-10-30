#include "domain/SsaImportGroup.h"

#include "domain/WhitespaceTrim.h"

#include <array>
#include <string_view>
#include <tuple>

namespace ssa::domain {

    namespace {

        // The fields that date a row and name it in a warning.
        constexpr std::array<std::string_view, 7> kDatingFields{
            "numero_ssa",          "situacao",
            "data_planilha",       "arquivo_origem",
            "data_arquivo_origem", "data_criacao_arquivo",
            "data_cadastro"};

        SsaImportPolicy::Values datingFields(const SsaImportPolicy::Values& row) {
            SsaImportPolicy::Values dating;
            for (const auto key : kDatingFields) {
                if (const auto found = row.find(std::string{key}); found != row.end()) {
                    dating.insert(*found);
                }
            }
            return dating;
        }

        std::string datingText(const SsaImportPolicy::Values& values) {
            std::string text;
            for (const auto key : kDatingFields) {
                const auto found = values.find(std::string{key});
                text += (found == values.end() ? std::string{} : found->second) + '';
            }
            return text;
        }

        bool isTerminalRow(const SsaImportPolicy::Values& row) {
            const auto status = row.find("situacao");
            return status != row.end() && SsaImportPolicy::isTerminalStatus(status->second);
        }

    } // namespace

    bool SsaImportGroup::Rank::operator<(const Rank& other) const {
        return std::tie(precedence, content, sourceName) <
               std::tie(other.precedence, other.content, other.sourceName);
    }

    void SsaImportGroup::Fold::add(const Values& row, const std::shared_ptr<const Rank>& rank) {
        // Tracks only the leading precedence: a newer one starts over.
        if (top && top->precedence == rank->precedence && top->content != rank->content) {
            resolvedByContent = true;
        } else if (top && top->precedence < rank->precedence) {
            resolvedByContent = false;
        }
        if (!top || *top < *rank) {
            top = rank;
        }
        for (const auto& [key, raw] : row) {
            auto value = trimWhitespace(raw);
            if (value.empty()) {
                continue;
            }
            auto& field = fields[key];
            // An equal rank is the same row read twice: the larger spelling keeps the result
            // independent of the read order.
            if (!field.rank || *field.rank < *rank ||
                (!(*rank < *field.rank) && field.value < value)) {
                field = {rank, std::move(value)};
            }
        }
    }

    void SsaImportGroup::keepExtreme(std::shared_ptr<const Rank>& kept, Values& dating,
                                     const std::shared_ptr<const Rank>& rank, const Values& row,
                                     const bool newest) {
        const auto candidate = datingFields(row);
        const bool beats = !kept || (newest ? *kept < *rank : *rank < *kept);
        // An equal rank (the same row spelled twice): the larger dating fields, in any order.
        const bool tie = kept && !(*kept < *rank) && !(*rank < *kept);
        if (beats || (tie && datingText(dating) < datingText(candidate))) {
            kept = rank;
            dating = candidate;
        }
    }

    void SsaImportGroup::add(const Values& row, const std::size_t source,
                             const std::string& sourceName) {
        ++rows_;
        const auto rank = std::make_shared<const Rank>(Rank{SsaImportPolicy::precedenceKey(row),
                                                            SsaImportPolicy::contentKey(row),
                                                            sourceName, source});
        if (!isTerminalRow(row)) {
            if (!terminal_.top) {
                open_.add(row, rank); // open rows no longer count once a terminal row exists
            }
            keepExtreme(newestOpenRank_, newestOpen_, rank, row, true);
            return;
        }
        keepExtreme(oldestTerminalRank_, oldestTerminal_, rank, row, false);
        open_ = {};
        if (terminal_.top && rank->precedence < terminal_.top->precedence) {
            return; // an older terminal row never counts
        }
        if (terminal_.top && terminal_.top->precedence < rank->precedence) {
            terminal_ = {};
        }
        terminal_.add(row, rank);
    }

    SsaImportGroup::Values SsaImportGroup::values() const {
        const auto& fold = terminal_.top ? terminal_ : open_;
        Values result;
        for (const auto& [key, field] : fold.fields) {
            result.emplace(key, field.value);
        }
        return result;
    }

    std::size_t SsaImportGroup::winnerSource() const {
        const auto& top = terminal_.top ? terminal_.top : open_.top;
        return top ? top->source : 0;
    }

    std::size_t SsaImportGroup::rows() const noexcept {
        return rows_;
    }

    bool SsaImportGroup::resolvedByContent() const noexcept {
        return terminal_.top ? terminal_.resolvedByContent : open_.resolvedByContent;
    }

    const SsaImportGroup::Values& SsaImportGroup::newestOpen() const noexcept {
        return newestOpen_;
    }

    const SsaImportGroup::Values& SsaImportGroup::oldestTerminal() const noexcept {
        return oldestTerminal_;
    }

} // namespace ssa::domain
