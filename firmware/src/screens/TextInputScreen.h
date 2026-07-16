#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"
#include <string>

namespace VOXA
{
    class TextInputScreen
    {
    public:
        ScreenId show(Touch& touch);
        
        static void prepare(const std::string& prompt, ScreenId backRoute, bool isPassword = false);
        static std::string getResult();

    private:
        static std::string s_prompt;
        static ScreenId    s_backRoute;
        static bool        s_isPassword;
        static std::string s_buffer;
        static bool        s_isShift;
        static bool        s_isNumMode;

        bool m_wasTouched { false };
    };
}
