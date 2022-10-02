#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "termcolor.hpp"

#include <unistd.h>
#include <termios.h>
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

template <std::size_t NUM_LINES_IN_TEST, std::size_t NUM_WORDS_PER_LINE_IN_TEST>
auto generate_lines(std::vector<std::string>& words, std::uniform_int_distribution<std::size_t>& distr, std::mt19937& gen) {
  std::array<std::string, NUM_LINES_IN_TEST> array_of_lines{};

  for (std::size_t i = 0; i < NUM_LINES_IN_TEST; ++i) {
    std::string line{""};
    for (std::size_t j = 0; j < NUM_WORDS_PER_LINE_IN_TEST; ++j) {
      line += words[distr(gen)];

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

      if (i > 0 && i % 10 == 0) {
        line += "\n";
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

template <std::size_t NUM_LINES_IN_TEST, std::size_t NUM_WORDS_PER_LINE_IN_TEST, std::size_t N>
void loop_array_of_lines(std::array<std::string, N>& array_of_lines, 
                         std::vector<std::string>& words, 
                         std::uniform_int_distribution<std::size_t>& distr, 
                         std::mt19937& gen) {
  std::chrono::high_resolution_clock::time_point start;

  /// Print lines first
  /// Assume cursor is already in the right place
  for (std::size_t i = 0; i < N; ++i) {
    std::cout << termcolor::white << termcolor::bold << array_of_lines[i] << termcolor::reset << std::endl;
  }

  /// Move up N lines to reset cursor
  move_up(N);

  /// Go to start of first line
  std::cout << "\r" << std::flush;

  /// Run test loop
  std::size_t i = 0;
  std::size_t n = 0; // current line
  auto line = array_of_lines[n];

  int key  = KEY_LEFTCTRL;

  FILE *kbd = fopen("/dev/input/by-path/platform-i8042-serio-0-event-kbd", "r");
  char key_map[KEY_MAX/8 + 1];    //  Create a byte array the size of the number of keys
  ioctl(fileno(kbd), EVIOCGKEY(sizeof(key_map)), key_map);    //  Fill the keymap with the current keyboard state

  int keyb = key_map[key/8];  //  The key we want (and the seven others arround it)
  int mask = 1 << (key % 8);  //  Put a one in the same column as out key state will be in;

  auto ctrl = [&]() {
    return !(keyb & mask);  //  Returns true if pressed otherwise false
  };

  while(true) {
    if (n >= N) {
      std::cout << "\r\n";
      // Report stats here
      auto end = std::chrono::high_resolution_clock::now();
      auto seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
      auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      auto num_words = NUM_LINES_IN_TEST * NUM_WORDS_PER_LINE_IN_TEST;
      std::cout << "Words : " << num_words << "\n";
      std::cout << "Time  : " << seconds << "s\n";
      std::cout << "WPM   : " << (float(num_words) / milliseconds) * 60 * 1000.0 << "\n";
      break;
    }
    char current = getch();

    if (current == 127 || current == 8) {
      /// Delete or Backspace
      if (ctrl()) {
        /// CTRL pressed
        /// Go back one word
      }
      else {
        /// Go back one char
      }
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

  constexpr std::size_t num_lines_in_test = 5;
  constexpr std::size_t num_words_per_line_in_test = 10;

  /// Generate list of lines
  auto array_of_lines = generate_lines<num_lines_in_test, num_words_per_line_in_test>(words, distr, gen);

  /// Start test
  loop_array_of_lines<num_lines_in_test, num_words_per_line_in_test>(array_of_lines, words, distr, gen);
}
