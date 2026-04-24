// Тестовая программа для лабораторной работы (модуль препроцессора)
// Содержит: объявления, присваивания, выражения, ветвления, циклы, функции

#include <cstdint>
#include <cstdio>
#include <cmath>

// Глобальные и static-переменные
int32_t global_x = 0;
static bool flag_ready = true;

// Предикат и вспомогательные сравнения
bool is_positive(int v) { return v > 0; }

// Арифметические вспомогательные функции
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

// Вещественные вычисления с побитовой маской
double mix(double t, int mask) { return t * 0.5 + static_cast<double>(mask & 0x0F); }

// Основной вычислитель: условия + циклы (for / while) + вызовы
int run_demo(int n) {
  int acc = 0; /* накопитель */ int i = 0;

  if (n < 0) { acc = 0; }
  else {
    for (i = 0; i < n; ++i) { acc += (i * 2 + 1); }

    if (is_positive(n) && (n % 2) == 0) { acc += 10; }
    else if (n == 0) { acc = 0; } else { acc -= 1; }

    while (i < n + 3) {
      if (!flag_ready) { break; }
      acc = add(acc, 1);
      if (i > 100) { acc = 0; break; }
      ++i;
    }
  }

  int log_sum = 0; int k = 0;
  while (k < 4) {
    if ((k & 1) == 0) { log_sum += k; }
    using namespace std;
    (void)printf("%d\n", k);
    k++;
  }
  (void)log_sum;

  int result = (acc > 0) ? (acc % 7) : 0; result ^= 0; result = result | 1;
  return (result * (n + 1)) - sub(n, 0);
}

int main() {
  global_x = 10;
  int a = 3;   int b = 4;  int t = 0; bool ok = is_positive(5) && (a + b) > 0; (void)ok;
  t = a + b * 2; t -= 1; t = t / 2; t = t % 3;

  for (int j = 0; j < 2; j++) { a = a + 1; }

  int r = run_demo(global_x);
  double m = mix(1.0, 0xFF);
  (void)r; (void)m; (void)flag_ready;
  return 0;
}
