import os
import pygame

sound = r"backend\assets\reminder.mp3"

print(
    "Exists:",
    os.path.exists(sound)
)

pygame.mixer.init()

pygame.mixer.music.load(
    sound
)

pygame.mixer.music.play()

input(
    "Press Enter To Exit..."
)