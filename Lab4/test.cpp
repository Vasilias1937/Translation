// ЛР4 — тест полного конвейера (дубликат Lab3/test.cpp по смыслу).
// Запуск: из каталога Lab4 выполнить .\compiler.exe .\test.cpp

#include <cstdint>

static int32_t counter = 0;

int bump(int32_t v) {
  return v + 1;
}

int main() {
  int x = 1;
  int sum = 0;
  for (int i = 0; i < 3; ++i) {
    if ((i % 2) == 0) {
      sum += bump(x);
    } else {
      sum -= 1;
    }
  }
  while (counter < 2) {
    counter++;
    if (counter > 10) {
      break;
    }
  }
  int r = (sum > 0) ? sum % 5 : 0;
  (void)r;
  return 0;
}
