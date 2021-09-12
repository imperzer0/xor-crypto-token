#include <map>
#include <thread>
#include <dirent.h>
#include <cstdarg>
#include <fcntl.h>
#include <sys/sendfile.h>
#include "terminal_output.hpp"
#include <fish-completions>
#include <parse-arguments>
#include <cstr-functions>
#include "debug_construct.h"

inline static void error(const std::string& message)
{
	std::cerr << "\033[31m\033[1merror\033[0m : \033[31m\033[3m" << message << "\n";
	throw std::runtime_error(message);
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
	
	for (int i   = 0; i < len; ++i)
	{
		spacing[i] = ' ';
	}
	spacing[len] = 0;
	
	fprintf(
			output_stream,
			"Usage:\n"
			"      %s --action=create-token --token=/dev/sdX             --- Setup /dev/sdX device as token (data will be lost).\n" // 1
			"      %s--label=<label> (--randompasswd/                    --- Password might be of size k*512 characters.\n" // S
			"      %s--passwd=<password>/--passwd-file=<password-file>   --- Program will ask to input password if no argument provided.\n" // S
			"      %s--passwd-size=<size_in_bytes>)                      ---\n" // S
			"      or\n"
			"      %s --action=check-token --token=/dev/sdX              --- Checks if /dev/sdX device is token.\n" // 2
			"      or\n"
			"      %s --action=list-tokens                               --- List all tokens connected to the system\n" // 3
			"      or\n"
			"      %s --action=copy-token --src=/dev/sdX --dest=/dev/sdY --- Copy token keys from source (/dev/sdX)\n"
			"      %s                                                    --- to destination (/dev/sdY)\n" // 4
			"      or\n"
			"      %s --action=install-completions <program_name>        --- Installs completions (if run from sudo -\n" // 5
			"      %s                                                    --- for all users, otherwise - only for\n" // S
			"      %s                                                    --- current user).\n" // S
			"      or\n"
			"      %s --action=uninstall-completions <program_name>      --- Uninstalls completions (if run from sudo -\n" // 6
			"      %s                                                    --- for all users, otherwise - only for current\n" // S
			"      %s                                                    --- user).\n", // S
			// 1        S        S        S        2        3        4        S        5       S         S        6        S        S
			appname, spacing, spacing, spacing, appname, appname, appname, spacing, appname, spacing, spacing, appname, spacing, spacing
	);
	exit(0);
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
			std::cout << "\033[31mfile\033[1m " << to << "\033[0m\033[31m will be overwritten\033[0m.\n";
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
//	DEBUG(std::cout, "is_token");
	std::string& out = ::exec("parted -ms " + path + " print");
	std::string garbage, sector_logical, sector_physical, partition_table, partition1_type, filesystem1_type, partition2_type, filesystem2_type;
	
	get_s_in_fmt(
			out, "%s" + path + ":%s:%s:%s:%s:%s:%s;\n1:%s::%s:%s;\n2:%s::%s:%s;",
			&garbage, &garbage, &garbage, &sector_logical, &sector_physical, &partition_table,
			&garbage, &garbage, &partition1_type, &filesystem1_type, &garbage, &partition2_type, &filesystem2_type, &garbage
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

void require_sudo(int argc, const char** argv)
{
	if (geteuid())
	{
		std::cout << "\033[31mPlease run command with \033[1m\033[3mroot\033[0m\033[31m permissions:\n\033[33m\tsudo";
		for (int i = 0; i < argc; ++i)
		{
			std::cout << " " << argv[i];
		}
		std::cout << "\n\033[0m";
		exit(-2);
	}
}

namespace anm
{
	static constexpr const char* action       = "--action";
	static constexpr const char* token        = "--token";
	static constexpr const char* randompasswd = "--randompasswd";
	static constexpr const char* passwd       = "--passwd";
	static constexpr const char* passwd_file  = "--passwd-file";
	static constexpr const char* label        = "--label";
	static constexpr const char* passwd_size  = "--passwd-size";
	static constexpr const char* src          = "--src";
	static constexpr const char* dest         = "--dest";
}

int main(int argc, const char** argv)
{
	setting1();
	setting2();
	
	if (argc < 2)
	{
		help(stdout, argv[0]);
	}
	
	arguments args(argc, argv);
	
	if (!args.exists_with_value(anm::action))
	{
		help(stdout, argv[0]);
	}
	std::string action = args.get_arg_value(anm::action);
	
	if (action == "create-token" && argc >= 4 && argc <= 8)
	{
		require_sudo(argc, argv);
		
		if (!args.exists_with_value(anm::token) || !args.exists_with_value(anm::label))
		{
			help(stdout, argv[0]);
		}
		
		std::string token(args.get_arg_value(anm::token)), label(args.get_arg_value(anm::label)), passwd;
		label += "\n";
		
		if (!args.exists(anm::randompasswd) && !args.exists_with_value(anm::passwd) && !args.exists_with_value(anm::passwd_file))
		{
			std::cout << "type password: ";
			passwd = read_password();
		}
		
		std::string passwd_arg(args.get_arg_value(anm::passwd));
		
		// init partitions...
		
		
		promt(
				"Writing token to device " + token + " will destroy user data", [](void*)
				{
					default_();
					exit(-1);
				}
		);
		
		system("parted -ms " + token + " mktable gpt");
		size_t password_size = 1024;
		if (args.exists_with_value(anm::passwd_size))
		{
			password_size = std::stoul(args.get_arg_value(anm::passwd_size));
		}
		else
		{
			if (args.exists_with_value(anm::passwd))
			{
				password_size = passwd_arg.size();
				if (password_size % 512)
				{
					size_t size = 512 - password_size % 512;
					char* random      = new char[size];
					FILE* random_file = ::fopen("/dev/random", "rb");
					::fread(random, sizeof(char), size, random_file);
					::fclose(random_file);
					passwd_arg.append(random, size);
					password_size += size;
				}
			}
			else if (args.exists_with_value(anm::passwd_file))
			{
				std::string passwd_file(args.get_arg_value(anm::passwd_file));
				struct stat st{ };
				if (::stat(passwd_file.c_str(), &st) < 0)
				{
					error("can't stat file \033[3m" + passwd_file + "\033[0m");
				}
				password_size = st.st_size;
				if (password_size % 512)
				{
					password_size += 512 - password_size % 512;
				}
			}
		}
		
		system("parted -ms " + token + " mkpart primary fat32 34s " + std::to_string(34 + password_size / 512) + "s");
		
		size_t eosecond = 35 + password_size + label.size() / 512 + (size_t)(bool)(label.size() % 512);
		system("parted -ms " + token + " mkpart primary fat32 " + std::to_string(35 + password_size) + "s " + std::to_string(eosecond) + "s");
		
		std::string& list = ::exec("parted -ms " + token + " print");
		std::string end, garbage;
		get_s_in_fmt(list, "%sBYT;\n" + token + ":%s:%s;\n%s", &garbage, &end, &garbage, &garbage);
		
		system("parted -ms " + token + " mkpart primary fat32 " + std::to_string(eosecond + 1) + "s " + end);
		
		// write values
		
		FILE* token_name = ::fopen((token + "2").c_str(), "wb");
		if (token_name == nullptr)
		{
			error("can't open file \033[1m" + token + "2\033[0m : \033[31m\033[3m" + strerror(errno));
		}
		
		::fwrite(label.c_str(), sizeof(char), label.size(), token_name);
		
		::fclose(token_name);
		
		if (args.exists(anm::randompasswd))
		{
			FILE* random = ::fopen("/dev/random", "rb");
			char random_str[512];
			FILE* device = ::fopen((token + "1").c_str(), "wb");
			for (int i = 0; i < password_size / 512; ++i)
			{
				::fread(random_str, sizeof(char), 512, random);
				::fwrite(random_str, sizeof(char), 512, device);
			}
			::fclose(random);
			::fclose(device);
		}
		else if (args.exists_with_value(anm::passwd))
		{
			FILE* passwd_file = ::fopen((token + "1").c_str(), "wb");
			passwd = args.get_arg_value(anm::passwd);
			::fwrite(passwd.c_str(), sizeof(char), passwd.size(), passwd_file);
			::fclose(passwd_file);
			if (password_size % 512)
			{
				size_t remains = 512 - password_size % 512;
				FILE* random     = ::fopen("/dev/random", "rb");
				char* random_str = new char[remains];
				::fread(random_str, sizeof(char), remains, random);
				::fclose(random);
				FILE* device = ::fopen((token + "1").c_str(), "ab");
				::fseek(device, password_size, SEEK_SET);
				::fwrite(random_str, sizeof(char), remains, device);
				::fclose(device);
			}
		}
		else if (args.exists_with_value(anm::passwd_file))
		{
			std::string passwd_file(args.get_arg_value(anm::passwd_file));
			copy(passwd_file, token + "1", true);
			struct stat st{ };
			if (::stat(passwd_file.c_str(), &st) < 0)
			{
				error("can't stat file \033[3m" + passwd_file);
			}
			password_size = st.st_size;
			if (password_size % 512)
			{
				size_t remains = 512 - password_size % 512;
				FILE* random     = ::fopen("/dev/random", "rb");
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
		require_sudo(argc, argv);
		
		
		if (!args.exists_with_value(anm::token))
		{
			help(stdout, argv[0]);
		}
		
		std::string token = args.get_arg_value(anm::token);
		
		std::cout << "\033[34mchecking device " << token << " ...\n";
		if (is_token(token))
		{
			FILE* token_name = ::fopen((token + "2").c_str(), "rb");
			linefstream nameblock(token_name);
			std::string& name = nameblock.getline();
			
			std::string spacer;
			for (int    i     = 0; i < name.size(); ++i)
			{
				spacer += ' ';
			}
			
			std::string& info = exec("parted -ms " + token + " print");
			std::string block_size, garbage;
			get_s_in_fmt(info, "%s;\n1:%s:%s:%s:%s", &garbage, &garbage, &garbage, &block_size, &garbage);
			
			std::cout << "\033[32mOK\033[34m token is \033[3mvalid\n\033[0m\033[1m\033[4mName" << spacer << "\b\b\bKey size\n\033[0m";
			std::cout << "\033[36m" << name << " \033[35m" << block_size << "\n\033[0m";
		}
	}
	else if (action == "copy-token" && argc == 4)
	{
		require_sudo(argc, argv);
		
		if (!args.exists_with_value(anm::src) && !args.exists_with_value(anm::dest))
		{
			help(stdout, argv[0]);
		}
		
		std::string src = args.get_arg_value(anm::src), dest = args.get_arg_value(anm::dest);
		
		if (!is_token(src))
		{
			error("device \033[36m" + src + "\033[31m isn't a valid token");
		}
		
		promt(
				"Copying token to device " + dest + " will destroy user data", [](void*)
				{
					default_();
					exit(-1);
				}
		);
		
		std::string& params = exec("parted -ms " + src + " print");
		std::string part1_start, part1_end, part2_start, part2_end, part1_size, garbage;
		get_s_in_fmt(
				params, "%s;\n1:%s:%s:%s:%s;\n2:%s:%s:%s",
				&garbage, &part1_start, &part1_end, &part1_size, &garbage, &part2_start, &part2_end, &garbage
		);
		
		system("parted -ms " + dest + " mktable gpt");
		
		system("parted -ms " + dest + " mkpart primary fat32 " + part1_start + " " + part1_end);
		
		system("parted -ms " + dest + " mkpart primary fat32 " + part2_start + " " + part2_end);
		
		std::string& list = ::exec("parted -ms " + dest + " print");
		std::string end;
		get_s_in_fmt(list, "%sBYT;\n%s:%s:%s;", &garbage, &garbage, &end, &garbage);
		
		system("parted -ms " + dest + " mkpart primary fat32 " + part2_end + " " + end);
		
		system("mkfs.fat -F32 " + dest + "3");
		
		std::cout << "\033[33mcopying " << part1_size << " from \033[36m" << src << "1\033[33m to \033[36m" << dest
				  << "1\n\033[0m";
		copy(src + "1", dest + "1", true);
		
		std::cout << "\033[33mcopying \033[37mlabel\033[33m from \033[36m" << src << "2\033[33m to \033[36m" << dest
				  << "2\n\033[0m";
		linefstream label(::fopen((src + "2").c_str(), "rb"));
		std::string& label_str = label.getline();
		label_str += "\n";
		FILE* label2 = ::fopen((dest + "2").c_str(), "wb");
		::fwrite(label_str.c_str(), sizeof(char), label_str.size(), label2);
		::fclose(label2);
	}
	else if (action == "list-tokens" && argc == 2)
	{
		require_sudo(argc, argv);
		
		DIR* devices = ::opendir("/dev/");
		if (devices == nullptr)
		{
			error("can't open directory /dev");
		}
		
		dirent* entry;
		std::vector<std::vector<std::string>> token_list;
		std::string                           garbage;
		while ((entry = ::readdir(devices)) != nullptr)
		{
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && is_file_or_block("/dev/" + std::string(entry->d_name)) &&
				get_s_in_fmt(entry->d_name, "sd%s", &garbage) && strlen(entry->d_name) == 3)
			{
				std::string token("/dev/");
				token += entry->d_name;
				std::cout << "\033[34mchecking device " << token << " ...\n";
				if (is_token(token))
				{
					std::string& info = exec("parted -ms " + token + " print");
					std::string part1_size, part2_size;
					get_s_in_fmt(
							info, "%s;\n1:%s:%s:%s:%s;\n2:%s:%s:%s:%s", &garbage, &garbage, &garbage, &part1_size, &garbage,
							&garbage, &garbage, &part2_size, &garbage
					);
					
					size_t size;
					if (std::tolower(part2_size.back()) == 'b')
					{
						if (std::tolower(part2_size[part2_size.size() - 2]) == 'k')
						{
							size = std::stoul(part2_size.substr(0, part2_size.size() - 2)) * 1024;
						}
						else if ('0' <= part2_size[part2_size.size() - 2] && part2_size[part2_size.size() - 2] <= '9')
						{
							size = std::stoul(part2_size.substr(0, part2_size.size() - 1));
						}
						else
						{
							continue;
						}
					}
					else
					{
						std::cout << "\033[31mcan't parse partition size\033[0m\n";
						continue;
					}
					
					if (size > 4096)
					{
						continue;
					}
					
					FILE* token_name = ::fopen((token + "2").c_str(), "rb");
					linefstream nameblock(token_name);
					std::string& name = nameblock.getline();
					
					std::cout << "\033[32mOK\033[34m token is \033[3mvalid\n\033[0m";
					std::cout << "\033[36m" << name << " \033[35m" << part1_size << "\n\033[0m";
					
					token_list.push_back({token, name, part1_size});
				}
			}
		}
		
		std::cout << "\033[32m\033[3mfound \033[35m" << token_list.size() << " \033[32mtokens.\n\033[0m";
		
		for (int i = 0; i < token_list.size(); ++i)
		{
			std::cout << "\033[35m" << i + 1 << ".\t\033[36m\033[1m" << token_list[i][0] << "\033[0m\ttoken name: \33[33m\033[3m" << token_list[i][1]
					  << "\033[0m\tsize: \033[35m" << token_list[i][2] << "\n";
		}
	}
	else if (action == "install-completions" && argc == 3)
	{
		completions comp(argv[2]);
		comp.init();
		comp.set("help", nullptr, "print help");
		comp.set(
				anm::action, new const char* []{
						"create-token",
						"check-token",
						"copy-token",
						"list-tokens",
						"help",
						"install-completions",
						"uninstall-completions",
						nullptr
				}, "action"
		);
		comp.set(anm::token, new const char* []{"(ls /dev/sd?)", nullptr}, "/dev/sdX device");
		comp.set(
				anm::label, nullptr, "give a label to new token",
				new const char* []{"--action=create-token", nullptr}
		);
		comp.set(
				anm::randompasswd, nullptr, "random password generation",
				new const char* []{"--action=create-token", nullptr}
		);
		comp.set(anm::passwd_size, nullptr, "password size", new const char* []{"--action=create-token", nullptr});
		comp.set(anm::passwd, nullptr, "password", new const char* []{"--action=create-token", nullptr});
		comp.set(
				anm::passwd_file, new const char* []{"(ls -p | grep -v /)"}, "file with password",
				new const char* []{"--action=create-token", nullptr}
		);
		comp.set(
				anm::src, new const char* []{"(ls /dev/sd?)", nullptr}, "source token device",
				new const char* []{"--action=copy-token", nullptr}
		);
		comp.set(
				anm::dest, new const char* []{"(ls /dev/sd?)", nullptr}, "destination token device",
				new const char* []{"--action=copy-token", nullptr}
		);
	}
	else if ((action == "uninstall-completions") && argc == 3)
	{
		completions comp(argv[2]);
		comp.uninstall();
	}
	else
	{
		help(stdout, argv[0]);
	}
	
	default_();
	
	return 0;
}
