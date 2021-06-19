//
// Created by imper on 5/10/21.
//

#ifndef XOR_CRYPTO_COMPLETIONS_HPP
#define XOR_CRYPTO_COMPLETIONS_HPP

#include <string>
#include <pwd.h>
#include <execution>
#include <iostream>
#include <sstream>

FILE* completions = nullptr;

std::string& exec(const std::string& cmd)
{
	char buffer[128];
	auto* result = new std::string;
	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe) throw std::runtime_error("popen() failed!");
	try
	{
		while (fgets(buffer, sizeof buffer, pipe) != nullptr)
		{
			*result += buffer;
			bzero(buffer, sizeof buffer);
		}
	} catch (...)
	{
		pclose(pipe);
		throw;
	}
	pclose(pipe);
	return *result;
}

void set_completion(const char* appname, const char* arg, const char** parameters, size_t size, const char* description, const char* display_after = ""/*, const char* dont_mix_with = ""*/)
{
	std::string cmd("complete -c ");
	cmd += appname;
	if (size)
	{
		cmd += " -n 'not __fish_seen_subcommand_from ";
		for (int i = 0; i < size; ++i)
		{
			std::stringstream ss(exec(std::string("/bin/fish -c \"echo ") + parameters[i] + "\""));
			std::string s;
			while (ss >> s)
			{
				cmd += "\"--";
				cmd += arg;
				cmd += "=";
				cmd += s;
				cmd += "\" ";
			}
		}
		if (!std::string(display_after).empty())
		{
			cmd += "&& __fish_seen_subcommand_from ";
			cmd += display_after;
		}
//		if (!std::string(dont_mix_with).empty())
//		{
//			cmd += " && not __fish_seen_subcommand_from ";
//			cmd += dont_mix_with;
//		}
		cmd += "'";
	}
	cmd += " -f -l ";
	cmd += arg;
	if (size)
	{
		cmd += " -a '";
		for (int i = 0; i < size; ++i)
		{
			cmd += parameters[i];
			if (i != size - 1)
			{
				cmd += " ";
			}
		}
		cmd += "'";
	}
	cmd += " -d '";
	cmd += description;
	cmd += "'\n";
	
	if (completions == nullptr)
	{
		std::cerr << ::strerror(errno) << "\n";
		exit(1);
	}
	fwrite(cmd.c_str(), 1, cmd.size(), completions);
}

void completion_init(const char* appname)
{
	std::string cmd("complete -c ");
	cmd += appname;
	cmd += " -e\n";
	
	cmd += "complete -c ";
	cmd += appname;
	cmd += " -f\n";

//	struct stat st{ };
//	if (!::stat((std::string("/etc/fish/completions/") + appname + ".fish").c_str(), &st))
//	{
//		::remove((std::string("/etc/fish/completions/") + appname + ".fish").c_str());
//	}
	
	std::string s;
	if (!getuid())
	{
		s = "/etc/fish/config.fish";
	}
	else
	{
		struct passwd* pw = ::getpwuid(getuid());
		s = pw->pw_dir;
		s += "/.config/fish/config.fish";
	}
	completions = ::fopen(s.c_str(), "ab");
	if (completions == nullptr)
	{
		std::cerr << ::strerror(errno) << "\n";
		exit(1);
	}
	fwrite(cmd.c_str(), 1, cmd.size(), completions);
}

class linefstream
{
public:
	explicit linefstream(FILE* fd) : fd(fd)
	{ }
	
	std::string& getline()
	{
		auto* res = new std::string;
		while (true)
		{
			char* tmp[1024]{ };
			if (!::feof(fd))
			{
				size_t bytes_read = 0;
				if (bytes_read = ::fread(tmp, 1, 1024, fd); bytes_read < 0)
				{
					std::cerr << ::strerror(errno) << "\n";
					return *new std::string;
				}
				buffer.append(reinterpret_cast<const char*>(tmp), bytes_read);
			}
			
			int pos = -1;
			for (int i = 0; i < buffer.size(); ++i)
			{
				if (buffer[i] == '\n')
				{
					pos = i;
					break;
				}
			}
			
			if (pos >= 0)
			{
				*res = buffer.substr(0, pos);
				buffer.erase(buffer.begin(), buffer.begin() + pos + 1);
				return *res;
			}
			
			if (pos < 0 && !buffer.empty())
			{
				*res = buffer;
				buffer.clear();
				return *res;
			}
		}
	}
	
	void getline(std::string& res)
	{
		res.clear();
		while (true)
		{
			char* tmp[1024]{ };
			if (!::feof(fd))
			{
				size_t bytes_read = 0;
				if (bytes_read = ::fread(tmp, 1, 1024, fd); bytes_read < 0)
				{
					std::cerr << ::strerror(errno) << "\n";
					return;
				}
				buffer.append(reinterpret_cast<const char*>(tmp), bytes_read);
			}
			
			int pos = -1;
			for (int i = 0; i < buffer.size(); ++i)
			{
				if (buffer[i] == '\n')
				{
					pos = i;
					break;
				}
			}
			
			if (pos >= 0)
			{
				res = buffer.substr(0, pos);
				buffer.erase(buffer.begin(), buffer.begin() + pos + 1);
			}
			
			if (pos < 0 && !buffer.empty())
			{
				res = buffer;
				buffer.clear();
			}
		}
	}
	
	explicit operator bool() const
	{
		return !::feof(fd) || !buffer.empty();
	}

private:
	FILE* fd;
	std::string buffer;
};

void completion_remove_all_lines_with(const std::string& str)
{
	std::string s;
	if (!getuid())
	{
		s = "/etc/fish/config.fish";
	}
	else
	{
		struct passwd* pw = ::getpwuid(getuid());
		s = pw->pw_dir;
		s += "/.config/fish/config.fish";
	}
	
	auto* read_completions = ::fopen(s.c_str(), "rb");
	if (read_completions == nullptr)
	{
		std::cerr << ::strerror(errno) << "\n";
		exit(1);
	}
	
	std::string file_content;
	linefstream file(read_completions);
	while (file)
	{
		std::string& line = file.getline();
		if (line.find(str) == std::string::npos)
		{
			file_content += line;
			file_content += "\n";
		}
	}
	
	if (completions != nullptr)
	{
		::fclose(completions);
		::remove(s.c_str());
	}
	
	completions = ::fopen(s.c_str(), "wb");
	if (completions == nullptr)
	{
		std::cerr << ::strerror(errno) << "\n";
		exit(1);
	}
	::fwrite(file_content.c_str(), 1, file_content.size(), completions);
	::fclose(completions);
	completions = nullptr;
}

#endif //XOR_CRYPTO_COMPLETIONS_HPP
