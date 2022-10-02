#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "termcolor.hpp"

#include <unistd.h>
#include <termios.h>

char getch() {
  char buf = 0;
  struct termios old = {0};
  if (tcgetattr(0, &old) < 0)
    perror("tcsetattr()");
  old.c_lflag &= ~ICANON;
  old.c_lflag &= ~ECHO;
  old.c_cc[VMIN] = 1;
  old.c_cc[VTIME] = 0;
  if (tcsetattr(0, TCSANOW, &old) < 0)
    perror("tcsetattr ICANON");
  if (read(0, &buf, 1) < 0)
    perror ("read()");
  old.c_lflag |= ICANON;
  old.c_lflag |= ECHO;
  if (tcsetattr(0, TCSADRAIN, &old) < 0)
    perror ("tcsetattr ~ICANON");
  return (buf);
}

void move_up(int N) {
  printf("\033[%dA", N);
}

int main() {

  std::ifstream file("popular.txt");

  std::size_t num_lines{0};
  std::vector<std::string> words{};

  {
    std::string line;

    while(getline(file, line)) {
      num_lines++; 
      words.push_back(line);  
    }
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> distr(0, num_lines);

  constexpr std::size_t num_words = 20;

  std::size_t num_line_breaks{0};
  std::string line{""};

  /// Construct words for typing
  for (std::size_t i = 0; i < num_words; ++i) {
    line += words[distr(gen)] + " ";
    if (i > 0 && i % 10 == 0) {
      line += "\n";
      num_line_breaks += 1;
    }
  }

  /// Add final newline if needed
  if (line[line.size() - 1] != '\n') {
    line += "\n";
    num_line_breaks += 1;
  }

  // Reset cursor to the start
  std::cout << termcolor::white << termcolor::bold << line << termcolor::reset << std::flush;
  move_up(num_line_breaks);
  std::cout << "\r" << std::flush;

  /// Start measurement
  auto start = std::chrono::high_resolution_clock::now();

  /// Run test loop
  std::size_t i = 0;
  while(true) {
    if (i >= line.size() - 2) {
      std::cout << "\r\n";
      // Report stats here
      auto end = std::chrono::high_resolution_clock::now();
      auto seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
      auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      std::cout << "Words : " << num_words << "\n";
      std::cout << "Time  : " << seconds << "s\n";
      std::cout << "WPM   : " << (float(num_words) / milliseconds) * 60 * 1000.0 << "\n";
      break;
    }
    if (line[i] == ' ') {
      std::cout << ' ' << std::flush;
      i += 1;
    }
    if (line[i] == '\n') {
      std::cout << "\n\r" << std::flush;
      i += 1;
    }
    char current = getch();
    char expected = line[i++];
    if (expected == current) {
      std::cout << termcolor::yellow << termcolor::bold << current << termcolor::reset << std::flush;
    }
    else {
      std::cout << termcolor::red << expected << termcolor::reset << std::flush;
    }
  }
  
}
