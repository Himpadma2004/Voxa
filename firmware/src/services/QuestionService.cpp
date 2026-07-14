#include "QuestionService.h"
#include "DataService.h"
#include "StorageService.h"

namespace VOXA
{
    QuestionService::QuestionService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Question> QuestionService::getAll()
    {
        return dataService.getQuestions();
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
        dataService.addQuestionLocal(q);
        return q;
    }

    bool QuestionService::remove(uint32_t id)
    {
        dataService.removeQuestionLocal(id);
        return m_storage->deleteQuestion(id);
    }
}
