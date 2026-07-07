#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../models/Question.h"

namespace VOXA
{
    class StorageService;

    class QuestionService
    {
    public:
        explicit QuestionService(StorageService* storage);

        [[nodiscard]] std::vector<Question> getAll();
        Question add(const std::string& text, const std::string& answer, const std::string& timestamp);
        bool remove(uint32_t id);

    private:
        StorageService* m_storage { nullptr };
    };
}
