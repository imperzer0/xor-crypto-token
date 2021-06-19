//
// Created by imper on 5/10/21.
//

#ifndef XOR_CRYPTO_TERMINAL_OUTPUT_HPP
#define XOR_CRYPTO_TERMINAL_OUTPUT_HPP

#include <iostream>
#include <string>
#include <termios.h>
#include <sys/ioctl.h>
#include <cmath>
#include <sys/stat.h>

#define RETURN_TO_BEGIN_OF_PREV_LINE "\033[F"
#define MOVE_UP "\033[A"

struct termios* stdin_defaults = nullptr;
struct termios* stdout_defaults = nullptr;

inline void setting1()
{
	if (stdin_defaults == nullptr)
	{
		stdin_defaults = new termios;
	}
	::tcgetattr(0, stdin_defaults);
	struct termios settings = *stdin_defaults;
	settings.c_lflag &= (~ICANON);
	settings.c_lflag &= (~ECHO);
	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 1;
	::tcsetattr(0, TCSANOW, &settings);
}

inline void setting2()
{
	if (stdout_defaults == nullptr)
	{
		stdout_defaults = new termios;
	}
	::tcgetattr(1, stdout_defaults);
	struct termios settings = *stdout_defaults;
	settings.c_lflag &= (~ICANON);
	settings.c_lflag &= (~ECHO);
	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 1;
	::tcsetattr(1, TCSANOW, &settings);
}

inline void default_()
{
	if (stdin_defaults != nullptr)
	{
		::tcsetattr(0, TCSANOW, stdin_defaults);
		stdin_defaults = nullptr;
	}
	
	if (stdout_defaults != nullptr)
	{
		::tcsetattr(1, TCSANOW, stdout_defaults);
		stdout_defaults = nullptr;
	}
}

void draw_progress_bar(const std::string& label, double progress)
{
	std::cout << std::fixed;
	std::cout.precision(2);
	std::cout << label << " ";
	if (std::round(progress * 100) / 100.0 < 10.0)
	{
		std::cout << " ";
	}
	if (std::round(progress * 100) / 100.0 < 100.0)
	{
		std::cout << " ";
	}
	std::cout << progress << "% "; // 3 + (4 -> 6) = (7 -> 9)
	struct winsize sz;
	ioctl(stdout->_fileno, TIOCGWINSZ, &sz);
	size_t size = sz.ws_col;
	size -= label.size() + 11; // 2 + 9 = 11
	std::cout.flush();
	std::cout << "[";
	size_t part = std::lround(progress * (double)size / 100.0);
	for (int i = 0; i < size; ++i)
	{
		if (i < part)
		{
			std::cout << '=';
		}
		else
		{
			std::cout << ' ';
		}
	}
	std::cout << "]\r";
	std::cout.flush();
}

typedef void (* cleanup_function)(void* args);

inline void promt(std::string file, cleanup_function cleanup_func, void* args = nullptr)
{
	struct stat st;
	if (!::stat(file.c_str(), &st))
	{
		std::cout << "File \"" << file << "\" will be overwritten. Do you agree?(y/N): ";
		char y_n = std::cin.get();
		if (y_n == 'Y' || y_n == 'y')
		{
			std::cout << "y\nremoving...\n";
		}
		else
		{
			std::cout << "N\nexiting...\n";
			cleanup_func(args);
		}
	}
}

#endif //XOR_CRYPTO_TERMINAL_OUTPUT_HPP
