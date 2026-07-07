#include "QuestionService.h"
#include "StorageService.h"

#include <algorithm>

namespace VOXA
{
    QuestionService::QuestionService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Question> QuestionService::getAll()
    {
        auto questions = m_storage->loadAllQuestions();
        std::sort(questions.begin(), questions.end(),
                  [](const Question& a, const Question& b) { return a.id < b.id; });
        return questions;
    }

    Question QuestionService::add(const std::string& text, const std::string& answer, const std::string& timestamp)
    {
        Question q;
        q.id = 0;
        q.text = text;
        q.answer = answer;
        q.timestamp = timestamp;
        q.answered = !answer.empty();
        m_storage->saveQuestion(q);

        auto all = m_storage->loadAllQuestions();
        for (auto it = all.rbegin(); it != all.rend(); ++it)
            if (it->text == text && it->timestamp == timestamp)
                return *it;
        return q;
    }

    bool QuestionService::remove(uint32_t id)
    {
        return m_storage->deleteQuestion(id);
    }
}
