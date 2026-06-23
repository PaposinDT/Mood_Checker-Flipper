#include "quotes.h"

static const char* const mood_quotes[MOOD_QUOTES_COUNT] = {
    "Small steps every day.",
    "Progress, not perfection.",
    "You are enough.",
    "Rest is productive.",
    "One day at a time.",
    "Feel it. Heal it.",
    "Your story matters.",
    "Be kind to yourself.",
    "Growth takes time.",
    "You got this.",
    "Energy flows where focus goes.",
    "Breathe. You are here.",
    "Celebrate tiny wins.",
    "Emotions are data.",
    "Consistency beats intensity.",
    "Check in. Show up.",
    "Balance is a practice.",
    "Mood tracking = self knowledge.",
    "Notice. Don't judge.",
    "Every entry counts.",
    "Strong people ask for help.",
    "This moment will pass.",
    "Your baseline is rising.",
    "Awareness is power.",
    "Log it. Learn it.",
    "You are not your stress.",
    "Patterns reveal paths.",
    "Steady wins the race.",
    "Trust the process.",
    "Tomorrow is a new log.",
};

const char* mood_quote_get(uint8_t index) {
    if(index >= MOOD_QUOTES_COUNT) index = 0;
    return mood_quotes[index];
}
