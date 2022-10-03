#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "termcolor.hpp"

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/input.h>

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

void move_down(int N) {
  printf("\033[%dB", N);
}

void move_right(int N) {
  printf("\033[%dC", N);
}

void move_left(int N) {
  printf("\033[%dD", N);
}

void clear_line() {
  printf("\33[2K\r");
}

template <std::size_t NUM_LINES_IN_TEST, std::size_t NUM_WORDS_PER_LINE_IN_TEST>
auto generate_lines(std::vector<std::string>& words, std::uniform_int_distribution<std::size_t>& distr, std::mt19937& gen,
  unsigned short rows, unsigned short cols) {
  std::array<std::string, NUM_LINES_IN_TEST> array_of_lines{};

  for (std::size_t i = 0; i < NUM_LINES_IN_TEST; ++i) {
    std::string line{""};
    for (std::size_t j = 0; j < NUM_WORDS_PER_LINE_IN_TEST; ++j) {
      auto word = words[distr(gen)];

      /// Check terminal size (cols)
      /// and break early if overflowing
      if (line.size() + word.size() >= cols) {
        break;
      }

      line += word;

      if (j + 1 < NUM_WORDS_PER_LINE_IN_TEST) {
        /// Not the last line
        line += " ";
      }
      else if (j + 1 == NUM_WORDS_PER_LINE_IN_TEST) {
        /// Last word in line
        if (i + 1 < NUM_LINES_IN_TEST) {
          line += " ";
        }
      }
    }

    array_of_lines[i] = line;
  }

  return array_of_lines;
}

template <std::size_t NUM_LINES_IN_TEST, std::size_t NUM_WORDS_PER_LINE_IN_TEST, std::size_t N>
auto update_lines(std::array<std::string, N>& array_of_lines, std::vector<std::string>& words, std::uniform_int_distribution<std::size_t>& distr, std::mt19937& gen) {
  std::array<std::string, N> result{};

  for (std::size_t i = 0; i < NUM_LINES_IN_TEST - 1; ++i) {
    result[i] = array_of_lines[i + 1];
  }

  /// Generate one new line
  auto new_line = generate_lines<1, NUM_WORDS_PER_LINE_IN_TEST>(words, distr, gen);

  result[N - 1] = new_line[0];

  return result;
}

std::size_t count_words(const std::string& str) {
  std::size_t result{0};

  char prev = ' ';

  auto size = str.size();

  for(std::size_t i = 0; i < size; ++i) {
    if(i + 1 < size && str[i] != ' ' && prev == ' ') {
      result++;
    }
    prev = str[i];
  }

  return result;
}

template <std::size_t NUM_LINES_IN_TEST, std::size_t NUM_WORDS_PER_LINE_IN_TEST, std::size_t N>
void loop_array_of_lines(std::array<std::string, N>& array_of_lines, 
                         std::vector<std::string>& words, 
                         std::uniform_int_distribution<std::size_t>& distr, 
                         std::mt19937& gen) {
  std::chrono::high_resolution_clock::time_point start;

  /// Print lines first
  /// Assume cursor is already in the right place
  for (std::size_t i = 0; i < N; ++i) {
    std::cout << termcolor::grey << termcolor::bold << array_of_lines[i] << termcolor::reset << std::endl;
  }

  /// Move up N lines to reset cursor
  move_up(N);

  /// Go to start of first line
  std::cout << "\r" << std::flush;

  /// Run test loop
  std::size_t i = 0;
  std::size_t n = 0; // current line
  auto line = array_of_lines[n];

  std::size_t total_num_mistakes{0};

  /// each index in the set in each line has a mistake
  std::array<std::set<std::size_t>, N> error_indices{}; 

  while(true) {
    if (n >= N) {
      std::cout << "\r\n";
      // Report stats here
      auto end = std::chrono::high_resolution_clock::now();
      auto seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
      auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

      std::size_t num_chars{0};
      std::size_t num_words{0};
      for (std::size_t i = 0; i < N; ++i) {
        num_words += count_words(array_of_lines[i]);
        num_chars += array_of_lines[i].size();
      }

      auto accuracy = 100.0 - (float(total_num_mistakes) / num_chars * 100.0);
      auto wpm = (float(num_words) / milliseconds) * 60 * 1000.0;

      std::cout << int(wpm) 
                << " wpm with "
                << std::setprecision(2) 
                << std::fixed
                << accuracy << "%"
                << " accuracy" 
                << std::endl;
      break;
    }

    char current = getch();

    if (current == 127) {
      /// Go to start of line
      std::cout << "\r"; 

      auto current_pos = i - 1;

      auto it = error_indices[n].find(current_pos);
      if (it != error_indices[n].end()) {
        error_indices[n].erase(it);
      }

      for (std::size_t x = 0; x < line.size(); ++x) {

        auto error_char = error_indices[n].find(x) != error_indices[n].end();

        if (error_char) {
          std::cout << termcolor::red << termcolor::bold << line[x] << termcolor::reset << std::flush;
        }
        else {
          if (x > current_pos) {
            std::cout << termcolor::grey << termcolor::bold << line[x] << termcolor::reset << std::flush;
          }
          else {
            std::cout << termcolor::yellow << termcolor::bold << line[x] << termcolor::reset << std::flush;
          }
        }
      }

      move_left(line.size() - current_pos + 1);

      if (i > 1) {
        i -= 1;
      } else {
        i = 0;
      }

      if (error_indices[n].find(i - 1) != error_indices[n].end()) {
        std::cout << termcolor::red << termcolor::bold << line[i - 1] << termcolor::reset << std::flush;
      } else {
        std::cout << termcolor::yellow << termcolor::bold << line[i - 1] << termcolor::reset << std::flush;
      }
      continue;
    }

    if (i > 0 && line[i - 1] == ' ') {
      /// Previous character was a space
      if (current == ' ') {
        /// User types additional spaces

        /// Ignore it
        continue;
      }
    }

    if (n == 0 && i == 0) {
      /// First characted typed by user
      /// Start time measurement here
      start = std::chrono::high_resolution_clock::now();
    }
    char expected = line[i++];
    if (expected == current) {
      std::cout << termcolor::yellow << termcolor::bold << current << termcolor::reset << std::flush;
    }
    else {
      std::cout << termcolor::red << expected << termcolor::reset << std::flush;
      total_num_mistakes += 1;
      error_indices[n].insert(i - 1);
    }

    if (i >= line.size()) {
      /// Last character in line has been printed

      /// Go to start of next line
      std::cout << "\n\r" << std::flush;
      n += 1;
      i = 0;

      /// Update line string
      if (n == N) {
        /// No more lines
      }
      else {
        line = array_of_lines[n];
      }
    }
  }
}

int main() {

  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  const auto rows = w.ws_col;
  const auto cols = w.ws_col;

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

  constexpr std::size_t num_lines_in_test = 3;
  constexpr std::size_t num_words_per_line_in_test = 5;

  /// Generate list of lines
  auto array_of_lines = generate_lines<num_lines_in_test, num_words_per_line_in_test>(words, distr, gen, rows, cols);

  /// Start test
  loop_array_of_lines<num_lines_in_test, num_words_per_line_in_test>(array_of_lines, words, distr, gen);
}
