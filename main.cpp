#include <map>
#include <thread>
#include <dirent.h>
#include <cstdarg>
#include <fcntl.h>
#include <sys/sendfile.h>
#include "terminal_output.hpp"
#include "completions.hpp"

inline static void error(const std::string& message)
{
	std::cerr << "\033[31m\033[1merror\033[0m : \033[31m\033[3m" << message << "\n";
	throw std::runtime_error(message);
}

std::string& trim_str_and_convert(const std::string& str, size_t start, size_t count = std::string::npos)
{
	auto* result = new std::string;
	for (int i = 0; i < count && i < str.size(); ++i)
	{
		if (str[i + start] == '%' == str[i + start + 1])
		{
			++i;
			++count;
		}
		*result += str[i + start];
	}
	return *result;
}

bool get_s_in_fmt(const std::string& str, const std::string& format, ...)
{
	va_list args;
	va_start(args, format);
	
	auto* result = new std::string;
	int pos_prefix_end = -1;
	for (int i = 0; i < format.size() && i < str.size(); ++i)
	{
		if (format[i] == '%' && format[i + 1] == 's')
		{
			pos_prefix_end = i;
			break;
		}
		else if (str[i] != format[i])
		{
			break;
		}
	}
	
	if (pos_prefix_end < 0)
	{
		return false;
	}
	
	int j = pos_prefix_end;
	int i = pos_prefix_end + 2, pos_s = j;
	int delta = 0, delta2 = 0;
	for (; j < str.size() && i < format.size();)
	{
		bool is_eq = true;
		for (int k = i, l = j;; ++k, ++l)
		{
			if ((format.size() - k >= 2 && format[k] == '%' && format[k + 1] == 's') || k >= format.size())
			{
				delta2 += k - i;
				j = l;
				break;
			}
			else if (str[l] != format[k])
			{
				is_eq = false;
				break;
			}
		}
		if (is_eq)
		{
			*va_arg(args, std::string*) = trim_str_and_convert(str, pos_s, delta);
			i += delta2 + 2;
			delta = 0;
			delta2 = 0;
			pos_s = j;
		}
		else
		{
			++delta;
			++j;
		}
	}
	if (format[i - 2] == '%' && format[i - 1] == 's')
	{
		*va_arg(args, std::string*) = trim_str_and_convert(str, j);
	}
	
	if (i < format.size())
	{
		return false;
	}
	
	va_end(args);
	
	return true;
}

inline static bool is_dir(const std::string& path)
{
	struct stat st;
	::stat(path.c_str(), &st);
	return S_ISDIR(st.st_mode);
}

inline static bool is_file_or_block(const std::string& path)
{
	struct stat st;
	::stat(path.c_str(), &st);
	return S_ISREG(st.st_mode) || S_ISBLK(st.st_mode);
}

inline static void help(FILE* output_stream, const char* appname)
{
	size_t len = strlen(appname);
	
	char* spacing = new char[len + 1];
	
	for (int i = 0; i < len; ++i)
	{
		spacing[i] = ' ';
	}
	spacing[len] = 0;
	
	fprintf(
			output_stream,
			"Usage:\n"
			"      %s --action=create-token --token=/dev/sdX              --- Setup /dev/sdX device as token (data will be lost).\n" // 1
			"      %s--label=<label> (--randompasswd/--passwd=<password>/ --- Password might be of size k*512 characters.\n" // s
			"      %s--passwd-file=<password-file>)                       --- Program will ask to input password if no argument provided.\n" // s
			"      or\n"
			"      %s --action=check-token --token=/dev/sdX               --- Checks if /dev/sdX device is token.\n" // 2
			"      or\n"
			"      %s --action=list-tokens                                --- List all tokens connected to the system\n" // 3
			"      or\n"
			"      %s --action=install-completions <program_name>         --- Installs completions (if run from sudo -\n" // 4
			"      %s                                                     --- for all users, otherwise - only for\n" // s
			"      %s                                                     --- current user).\n" // s
			"      or\n"
			"      %s --action=uninstall-completions <program_name>       --- Uninstalls completions (if run from sudo -\n" // 5
			"      %s                                                     --- for all users, otherwise - only for current\n" // s
			"      %s                                                     --- user).\n", // 11 s
			// 1        s        s        2        3        4       s         s        5        s        s
			appname, spacing, spacing, appname, appname, appname, spacing, spacing, appname, spacing, spacing
	);
	exit(0);
}

std::map<std::string, std::string>& parse_args(int argc, char** const& argv)
{
	auto result = new std::map<std::string, std::string>();
	for (int i = 1; i < argc; ++i)
	{
		std::string arg(argv[i]);
		std::string::size_type pos = arg.find('=');
		if (pos != std::string::npos)
		{
			result->insert({arg.substr(0, pos), arg.substr(++pos, arg.size() - pos)});
		}
		else
		{
			result->insert({arg, ""});
		}
	}
	return *result;
}

template <typename _char = char>
std::basic_string<_char>& read_password(std::basic_istream<_char>& input_stream = std::cin, std::basic_ostream<_char>& output_stream = std::cout)
{
	_char c;
	auto* console_input = new std::basic_string<_char>();
	while (true)
	{
		c = input_stream.get();
		if (c == '\b' || c == 0x7f)
		{
			if (!console_input->empty())
			{
				std::cout << "\b \b";
				console_input->pop_back();
			}
		}
		else if (c == '\n')
		{
			output_stream << '\n';
			break;
		}
		else
		{
			*console_input += c;
			output_stream << '*';
		}
	}
	return *console_input;
}

std::string& read_text()
{
	std::cout << "Type text here to encode. Once you've finished type ESC to start process.\n";
	struct winsize sz;
	ioctl(stdout->_fileno, TIOCGWINSZ, &sz);
	size_t size = sz.ws_col;
	std::cout << '+';
	for (int i = 1; i < size - 1; ++i)
	{
		std::cout << '=';
	}
	std::cout << "+\n| ";
	char c;
	auto* console_input = new std::string();
	std::vector<std::string> lines;
	lines.emplace_back();
	while (true)
	{
		c = std::cin.get();
		if (c == 0x1b)
		{
			std::cout << "\n";
			break;
		}
		else if (c == '\n')
		{
			*console_input += c;
			std::cout << "\n| ";
			lines.back() += c;
			lines.emplace_back();
		}
		else if (c == '\b' || c == 0x7f)
		{
			if (!lines.back().empty())
			{
				lines.back().pop_back();
				std::cout << "\b \b";
			}
			else
			{
				lines.pop_back();
				std::cout << "\r  " << RETURN_TO_BEGIN_OF_PREV_LINE << "\033[" << lines.back().size() + 1 << "C";
			}
			console_input->pop_back();
		}
		else
		{
			*console_input += c;
			lines.back() += c;
			std::cout << c;
		}
	}
	return *console_input;
}

void system(const std::string& cmd)
{
	system(cmd.c_str());
}

void copy(const std::string& from, const std::string& to, bool force)
{
	int input, output;
	if ((input = open(from.c_str(), O_RDONLY)) == -1)
	{
		error("opening " + from + " as input failed : " + std::to_string(errno) + " : " + ::strerror(errno));
	}
	
	struct stat st{ };
	if (::stat(to.c_str(), &st) >= 0)
	{
		if (force)
		{
			std::cout << "\033[31mdeleting\033[1m " << to << "\033[0m\n";
			::remove(to.c_str());
		}
		else
		{
			error("can't continue because file " + to + " already exists.");
		}
	}
	
	::fstat(input, &st);
	
	if ((output = creat(to.c_str(), st.st_mode)) == -1)
	{
		::close(input);
		error("opening " + from + " as output failed : " + std::to_string(errno) + " : " + ::strerror(errno));
	}
	
	off_t offset = 0;
	
	if (::sendfile(output, input, &offset, st.st_size) == st.st_size)
	{
		std::cout << "copying file \033[32msuccessful\033[0m.\n";
	}
	else
	{
		std::cout << "copying file \033[31munsuccessful\033[0m.\n";
	}
	
	::close(input);
	::close(output);
}

#define test(var, val) if ((var) != (val)) return false

bool is_token(const std::string& path)
{
	std::string& out = ::exec("parted -ms " + path + " print");
	std::string garbage, sector_logical, sector_physical, partition_table, partition1_type, filesystem1_type, partition2_type, filesystem2_type;
	
	get_s_in_fmt(
			out, "%s" + path + ":%s:%s:%s:%s:%s:%s;\n1:%s::%s:%s;\n2:%s::%s:%s;", &garbage, &garbage, &garbage, &sector_logical, &sector_physical, &partition_table,
			&garbage, &garbage, &partition1_type, &filesystem1_type, &garbage, &partition2_type, &filesystem2_type
	);
	
	test(sector_logical, "512");
	test(sector_physical, "512");
	test(partition_table, "gpt");
	test(partition1_type, "primary");
	test(filesystem1_type, "msftdata");
	test(partition2_type, "primary");
	test(filesystem2_type, "msftdata");
	
	return true;
}

int main(int argc, char** argv)
{
	setting1();
	setting2();
	
	if (argc < 2)
	{
		help(stdout, argv[0]);
	}
	
	auto parsed_args = parse_args(argc, argv);
	
	auto pos = parsed_args.find("--action");
	if (pos == parsed_args.end() || pos->second.empty())
	{
		help(stdout, argv[0]);
	}
	std::string action = pos->second;
	
	if (action == "create-token" && argc >= 4 && argc <= 8)
	{
		auto token_arg = parsed_args.find("--token");
		auto randompasswd_arg = parsed_args.find("--randompasswd");
		auto passwd_arg = parsed_args.find("--passwd");
		auto passwd_file_arg = parsed_args.find("--passwd-file");
		auto label_arg = parsed_args.find("--label");
		auto passwd_size_arg = parsed_args.find("--passwd-size");
		
		if (token_arg == parsed_args.end() || token_arg->second.empty() || label_arg == parsed_args.end() || label_arg->second.empty())
		{
			help(stdout, argv[0]);
		}
		
		label_arg->second += "\n";
		
		std::string token(token_arg->second), passwd;
		
		if ((randompasswd_arg == parsed_args.end() || randompasswd_arg->second.empty()) && (passwd_arg == parsed_args.end() || passwd_arg->second.empty()) && (passwd_file_arg == parsed_args.end() || passwd_file_arg->second.empty()))
		{
			std::cout << "type password: ";
			std::string console_input = read_password();
			parsed_args["--passwd"] = console_input;
			passwd_arg = parsed_args.find("--passwd");
		}
		
		// init partitions...
		::system(("parted -ms " + token + " mktable gpt").c_str());
		size_t password_size;
		if (passwd_size_arg != parsed_args.end() && !passwd_size_arg->second.empty())
		{
			password_size = std::stoul(passwd_size_arg->second);
			if (password_size % 512)
			{
				password_size += 512 - password_size % 512;
			}
		}
		else
		{
			password_size = 1024;
		}
		
		if (passwd_arg != parsed_args.end() && !passwd_arg->second.empty())
		{
			password_size = passwd_arg->second.size();
			if (password_size % 512)
			{
				size_t size = 512 - password_size % 512;
				char* random = new char[size];
				FILE* random_file = ::fopen("/dev/random", "rb");
				::fread(random, sizeof(char), size, random_file);
				passwd_arg->second.append(random, size);
				::fclose(random_file);
				password_size += size;
			}
		}
		else if (passwd_file_arg != parsed_args.end() && !passwd_file_arg->second.empty())
		{
			struct stat st{ };
			if (::stat(passwd_file_arg->second.c_str(), &st) < 0)
			{
				error("can't stat file \033[3m" + passwd_file_arg->second);
			}
			password_size = st.st_size;
			if (password_size % 512)
			{
				password_size += 512 - password_size % 512;
			}
		}
		
		system("parted -ms " + token + " mkpart primary fat32 34s " + std::to_string(34 + password_size / 512) + "s");
		
		size_t eosecond = 35 + password_size + label_arg->second.size() / 512 + (size_t)(bool)(label_arg->second.size() % 512);
		system("parted -ms " + token + " mkpart primary fat32 " + std::to_string(35 + password_size) + "s " + std::to_string(eosecond) + "s");
		
		std::string& list = ::exec("parted -ms " + token + " print");
		std::string end, garbage;
		get_s_in_fmt(list, "BYT;\n" + token + ":%s:%s;\n%s", &end, &garbage, &garbage);
		
		system("parted -ms " + token + " mkpart primary fat32 " + std::to_string(36 + password_size + eosecond) + "s " + end);
		
		// write values
		
		FILE* token_name = ::fopen((token + "2").c_str(), "wb");
		if (token_name == nullptr)
		{
			error("can't open file \033[1m" + token + "2\033[0m : \033[31m\033[3m" + strerror(errno));
		}
		
		::fwrite(label_arg->second.c_str(), sizeof(char), label_arg->second.size(), token_name);
		
		::fclose(token_name);
		
		if (randompasswd_arg != parsed_args.end())
		{
			system("cat /dev/random > " + token + "1");
		}
		else if (passwd_arg != parsed_args.end() && !passwd_arg->second.empty())
		{
			FILE* passwd_file = ::fopen((token + "1").c_str(), "wb");
			::fwrite(passwd_arg->second.c_str(), sizeof(char), passwd_arg->second.size(), passwd_file);
			::fclose(passwd_file);
			if (password_size % 512)
			{
				size_t remains = 512 - password_size % 512;
				FILE* random = ::fopen("/dev/random", "rb");
				char* random_str = new char[remains];
				::fread(random_str, sizeof(char), remains, random);
				::fclose(random);
				FILE* device = ::fopen((token + "1").c_str(), "ab");
				::fseek(device, password_size, SEEK_SET);
				::fwrite(random_str, sizeof(char), remains, device);
				::fclose(device);
			}
		}
		else if (passwd_file_arg != parsed_args.end() && !passwd_file_arg->second.empty())
		{
			copy(passwd_file_arg->second, token + "1", true);
			struct stat st{ };
			if (::stat(passwd_file_arg->second.c_str(), &st) < 0)
			{
				error("can't stat file \033[3m" + passwd_file_arg->second);
			}
			password_size = st.st_size;
			if (password_size % 512)
			{
				size_t remains = 512 - password_size % 512;
				FILE* random = ::fopen("/dev/random", "rb");
				char* random_str = new char[remains];
				::fread(random_str, sizeof(char), remains, random);
				::fclose(random);
				FILE* device = ::fopen((token + "1").c_str(), "ab");
				::fseek(device, password_size, SEEK_SET);
				::fwrite(random_str, sizeof(char), remains, device);
				::fclose(device);
			}
		}
		system("mkfs.fat -F32 " + token + "3");
	}
	else if (action == "check-token" && argc == 3)
	{
		auto token_arg = parsed_args.find("--token");
		
		if (token_arg == parsed_args.end() || token_arg->second.empty())
		{
			help(stdout, argv[0]);
		}
		
		std::cout << "\033[34mchecking device " << token_arg->second << " ...\n";
		if (is_token(token_arg->second))
		{
			FILE* token_name = ::fopen((token_arg->second + "2").c_str(), "rb");
			linefstream nameblock(token_name);
			std::string& name = nameblock.getline();
			
			std::string spacer;
			for (int i = 0; i < name.size(); ++i)
			{
				spacer += ' ';
			}
			
			std::string& info = exec("parted -ms " + token_arg->second + " print");
			std::string block_size, garbage;
			get_s_in_fmt(info, "%s;\n1:%s:%s:%s:%s", &garbage, &garbage, &garbage, &block_size, &garbage);
			
			std::cout << "\033[32mOK\033[34m token is \033[3mvalid\n\033[0m\033[1m\033[4mName" << spacer << "\b\b\bKey size\n\033[0m";
			std::cout << "\033[36m" << name << " \033[35m" << block_size << "\n\033[0m";
		}
	}
	else if (action == "copy-token" && argc == 4)
	{
		// undefined
		std::cout << "\033[31munavailable\n\033[0m";
	}
	else if (action == "list-tokens" && argc == 2)
	{
		DIR* devices = ::opendir("/dev/");
		if (devices == nullptr)
		{
			error("can't open directory /dev");
		}
		dirent* entry;
		std::vector<std::vector<std::string>> token_list;
		while ((entry = ::readdir(devices)) != nullptr)
		{
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && is_file_or_block("/dev/" + std::string(entry->d_name)) &&
				get_s_in_fmt(entry->d_name, "sd%s") && strlen(entry->d_name) == 3)
			{
				std::string token("/dev/");
				token += entry->d_name;
				std::cout << "\033[34mchecking device " << token << " ...\n";
				if (is_token(token))
				{
					FILE* token_name = ::fopen((token + "2").c_str(), "rb");
					linefstream nameblock(token_name);
					std::string& name = nameblock.getline();
					
					std::string spacer;
					for (int i = 0; i < name.size(); ++i)
					{
						spacer += ' ';
					}
					
					std::string& info = exec("parted -ms " + token + " print");
					std::string block_size, garbage;
					get_s_in_fmt(info, "%s;\n1:%s:%s:%s:%s", &garbage, &garbage, &garbage, &block_size, &garbage);
					
					std::cout << "\033[32mOK\033[34m token is \033[3mvalid\n\033[0m\033[1m\033[4mName" << spacer << "\b\b\bKey size\n\033[0m";
					std::cout << "\033[36m" << name << " \033[35m" << block_size << "\n\033[0m";
					
					token_list.push_back({token, name, block_size});
				}
			}
		}
		
		std::cout << "\033[32m\033[3mfound \033[35m" << token_list.size() << " \033[32mtokens.\n\033[0m";
		
		for (int i = 0; i < token_list.size(); ++i)
		{
			std::cout << "\033[35m" << i + 1 << ".\t\033[36m\033[1m" << token_list[i][0] << "\033[0m\ttoken name: \33[33m\033[3m" << token_list[i][1] << "\033[0m\tsize: \033[35m" << token_list[i][2] << "\n";
		}
	}
	else if (action == "install-completions" && argc == 3)
	{
		completion_init(argv[2]);
		set_completion(argv[2], "help", new const char* []{ }, 0, "print help");
		set_completion(argv[2], "action", new const char* []{"create-token", "check-token", "copy-token", "list-tokens", "help", "install-completions", "uninstall-completions"}, 7, "action");
		set_completion(argv[2], "token", new const char* []{"(ls -p /dev/sd?)"}, 1, "/dev/sdX device");
		set_completion(argv[2], "label", new const char* []{ }, 0, "give a label to new token", "--action=create-token");
		set_completion(argv[2], "randompasswd", new const char* []{ }, 0, "random password generation", "--action=create-token");
		set_completion(argv[2], "passwd", new const char* []{ }, 0, "password", "--action=create-token");
		set_completion(argv[2], "passwd-file", new const char* []{"(ls -p | grep -v /)"}, 1, "file with password", "--action=create-token");
	}
	else if ((action == "uninstall-completions") && argc == 3)
	{
		std::string str("complete -c ");
		str += argv[2];
		completion_remove_all_lines_with(str);
	}
	else
	{
		help(stdout, argv[0]);
	}
	
	default_();
	
	return 0;
}
