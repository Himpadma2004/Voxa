#include "SearchService.h"
#include "StorageService.h"

#include <algorithm>
#include <cctype>

namespace VOXA
{
    SearchService::SearchService(StorageService* storage)
        : m_storage(storage)
    {
    }

    bool SearchService::containsIgnoreCase(const std::string& haystack,
                                           const std::string& needle)
    {
        if (needle.empty()) return true;

        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(),   needle.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
        return it != haystack.end();
    }

    namespace
    {
        std::vector<std::string> tokenize(const std::string& text)
        {
            std::vector<std::string> tokens;
            std::string current;
            for (char c : text)
            {
                if (std::isalnum(static_cast<unsigned char>(c)))
                {
                    current += std::tolower(static_cast<unsigned char>(c));
                }
                else if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
            }
            if (!current.empty())
            {
                tokens.push_back(current);
            }
            return tokens;
        }

        bool isStopWord(const std::string& word)
        {
            static const std::vector<std::string> stopWords = {
                "what", "is", "how", "do", "to", "the", "a", "an", "of", "in", "on", "for", "with", "about", "why", "who", "where", "when", "are", "you", "i", "can", "my", "your", "and", "or", "but", "if"
            };
            return std::find(stopWords.begin(), stopWords.end(), word) != stopWords.end();
        }

        std::vector<std::string> expandSynonyms(const std::string& word)
        {
            static const std::map<std::string, std::vector<std::string>> synonyms = {
                {"sync", {"cloud", "backup", "files", "server", "offline", "network"}},
                {"backup", {"sync", "cloud", "files", "server", "save", "network"}},
                {"call", {"phone", "sofia", "talk", "mobile", "contact", "ring", "speak"}},
                {"sofia", {"call", "phone", "talk", "contact"}},
                {"birthday", {"bday", "emily", "gift", "party", "anniversary", "date"}},
                {"bday", {"birthday", "emily", "gift", "party", "anniversary", "date"}},
                {"emily", {"birthday", "bday", "party"}},
                {"roadmap", {"product", "plan", "future", "schedule", "milestones", "ideas"}},
                {"plan", {"roadmap", "product", "future", "schedule", "ideas", "brainstorm"}},
                {"chromadb", {"vector", "database", "embeddings", "rag", "search", "questions"}},
                {"vector", {"chromadb", "database", "embeddings", "rag", "search"}},
                {"database", {"chromadb", "vector", "embeddings", "rag", "search"}},
                {"llm", {"gpt", "ai", "model", "language", "large", "intelligence", "questions"}},
                {"ai", {"llm", "gpt", "model", "language", "large", "intelligence", "questions"}},
                {"memory", {"mind", "recall", "brain", "store", "thoughts", "others"}},
                {"thoughts", {"memory", "mind", "recall", "brain", "store", "others"}}
            };
            
            auto it = synonyms.find(word);
            if (it != synonyms.end())
            {
                return it->second;
            }
            return {};
        }

        int calculateScore(const std::vector<std::string>& queryTokens, const std::string& title, const std::string& content, const std::string& category)
        {
            std::vector<std::string> titleTokens = tokenize(title);
            std::vector<std::string> contentTokens = tokenize(content);

            int score = 0;

            bool asksTime = false;
            bool asksQuestion = false;
            for (const auto& qt : queryTokens)
            {
                if (qt == "when" || qt == "time" || qt == "date" || qt == "due" || qt == "calendar" || qt == "schedule") asksTime = true;
                if (qt == "what" || qt == "how" || qt == "explain" || qt == "why" || qt == "who" || qt == "where" || qt == "question") asksQuestion = true;
            }

            if (asksTime && category == "reminder") score += 15;
            if (asksQuestion && category == "question") score += 15;

            for (const auto& qt : queryTokens)
            {
                if (isStopWord(qt)) continue;

                if (std::find(titleTokens.begin(), titleTokens.end(), qt) != titleTokens.end())
                {
                    score += 25;
                }
                else
                {
                    for (const auto& tt : titleTokens)
                    {
                        if (tt.find(qt) != std::string::npos || qt.find(tt) != std::string::npos)
                        {
                            score += 12;
                            break;
                        }
                    }
                }

                if (std::find(contentTokens.begin(), contentTokens.end(), qt) != contentTokens.end())
                {
                    score += 15;
                }
                else
                {
                    for (const auto& ct : contentTokens)
                    {
                        if (ct.find(qt) != std::string::npos || qt.find(ct) != std::string::npos)
                        {
                            score += 8;
                            break;
                        }
                    }
                }

                auto syns = expandSynonyms(qt);
                for (const auto& syn : syns)
                {
                    if (std::find(titleTokens.begin(), titleTokens.end(), syn) != titleTokens.end())
                    {
                        score += 18;
                    }
                    if (std::find(contentTokens.begin(), contentTokens.end(), syn) != contentTokens.end())
                    {
                        score += 10;
                    }
                }
            }

            return score;
        }
    }

    std::vector<SearchResult> SearchService::search(const std::string& query) const
    {
        if (query.empty()) return {};

        std::vector<std::string> queryTokens = tokenize(query);

        struct ScoredResult
        {
            SearchResult result;
            int score;
        };
        std::vector<ScoredResult> scoredResults;

        // 1. Reminders
        for (const auto& r : m_storage->loadAllReminders())
        {
            int score = calculateScore(queryTokens, r.title, r.dateTime, "reminder");
            if (score > 0)
            {
                scoredResults.push_back({ { r.title, "reminder", r.dateTime, r.id }, score });
            }
        }

        // 2. Memories
        for (const auto& m : m_storage->loadAllMemories())
        {
            int score = calculateScore(queryTokens, m.title, m.content, "memory");
            if (score > 0)
            {
                scoredResults.push_back({ { m.title, "memory", m.timestamp, m.id }, score });
            }
        }

        // 3. Ideas
        for (const auto& idea : m_storage->loadAllIdeas())
        {
            int score = calculateScore(queryTokens, idea.title, idea.content, "idea");
            if (score > 0)
            {
                scoredResults.push_back({ { idea.title, "idea", idea.timestamp, idea.id }, score });
            }
        }

        // 4. Questions
        for (const auto& q : m_storage->loadAllQuestions())
        {
            int score = calculateScore(queryTokens, q.text, q.answer, "question");
            if (score > 0)
            {
                scoredResults.push_back({ { q.text, "question", q.timestamp, q.id }, score });
            }
        }

        // Sort by score descending
        std::sort(scoredResults.begin(), scoredResults.end(),
                  [](const ScoredResult& a, const ScoredResult& b) { return a.score > b.score; });

        std::vector<SearchResult> results;
        results.reserve(scoredResults.size());
        for (const auto& sr : scoredResults)
        {
            results.push_back(sr.result);
        }

        return results;
    }

    std::vector<SearchResult> SearchService::getAll() const
    {
        std::vector<SearchResult> results;

        for (const auto& r : m_storage->loadAllReminders())
            results.push_back({ r.title, "reminder", r.dateTime, r.id });
        for (const auto& m : m_storage->loadAllMemories())
            results.push_back({ m.title, "memory", m.timestamp, m.id });
        for (const auto& i : m_storage->loadAllIdeas())
            results.push_back({ i.title, "idea", i.timestamp, i.id });
        for (const auto& q : m_storage->loadAllQuestions())
            results.push_back({ q.text, "question", q.timestamp, q.id });

        return results;
    }

    std::vector<SearchResult> SearchService::getRecent(int maxCount) const
    {
        auto all = getAll();
        if (maxCount >= 0 && static_cast<int>(all.size()) > maxCount)
            all.resize(static_cast<std::size_t>(maxCount));
        return all;
    }
}
