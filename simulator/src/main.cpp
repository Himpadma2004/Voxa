#include <SDL3/SDL.h>
#include <iostream>

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << "Failed to initialize SDL\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "VOXA Simulator",
        900,
        600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window)
    {
        std::cout << "Failed to create window\n";
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    bool running = true;

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        SDL_SetRenderDrawColor(renderer,18,18,18,255);

        SDL_RenderClear(renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}