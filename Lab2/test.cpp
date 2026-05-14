// Lab2 — образец для лексического анализа (разнообразие лексем).
// Запуск: lexer test.cpp   или   lexer test.cpp --raw

#include <cstdio>

static bool flag = false;

int sample_lex(int a, int b) {
  double x = 1.5;
  int h = 0x10;
  const char* msg = "tokens";
  if (a <= b && !flag) {
    x = x + 0.25e1;
  }
  (void)printf("%s %d\n", msg, h);
  return a + b * 2 - (h >> 1);
}
