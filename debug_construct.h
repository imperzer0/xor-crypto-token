//
// Created by imper on 12/13/20.
//

#ifndef VERY_LONG_NUMBER_DEBUG_CONSTRUCT_H
#define VERY_LONG_NUMBER_DEBUG_CONSTRUCT_H

#include <iostream>

template <typename _char>
class debug
{
 private:
	static long long tabs_count;
	long long additional_tabs_count;
	std::basic_ostream<_char>* stream;
	std::basic_string<_char> name;
 public:
	debug(std::basic_ostream<_char>& stream, const std::basic_string<_char>& name) : stream(&stream), name(name),
																					 additional_tabs_count(0)
	{
		for (int i(0); i < tabs_count; ++i)
		{
			*this->stream << "\t";
		}
		*this->stream << " >> IN:  " << name << "\n";
		++tabs_count;
	}
	
	void print_message(const std::basic_string<_char>& name, const std::basic_string<_char>& val)
	{
		for (int i(0); i < tabs_count + additional_tabs_count; ++i)
		{
			*this->stream << "\t";
		}
		*this->stream << " INFO: " << name << " = " << val << "\n";
	}
	
	template <typename ... tail>
	debug<_char>& print_all(const std::basic_string<_char>& name, const std::basic_string<_char>& val, tail ... T)
	{
		print_message(name, val);
		print_all(T...);
		return *this;
	}
	
	debug<_char>& print_all(const std::basic_string<_char>& name, const std::basic_string<_char>& val)
	{
		print_message(name, val);
		return *this;
	}
	
	template <typename ... tail>
	debug<_char>& print_all_hex(const std::basic_string<_char>& name, unsigned long long& val, tail ... T)
	{
		print_hex_num(name, val);
		print_all_hex(T...);
		return *this;
	}
	
	debug<_char>& print_all_hex(const std::basic_string<_char>& name, unsigned long long& val)
	{
		print_hex_num(name, val);
		return *this;
	}
	
	template <typename ... tail>
	debug<_char>& print_all_bin(const std::basic_string<_char>& name, unsigned long long& val, tail ... T)
	{
		print_binary_num(name, val);
		print_all_bin(T...);
		return *this;
	}
	
	debug<_char>& print_all_bin(const std::basic_string<_char>& name, unsigned long long& val)
	{
		print_binary_num(name, val);
		return *this;
	}
	
	debug<_char>& operator<<(const std::basic_string<_char>& val)
	{
		print_message("var", val);
		return *this;
	}
	
	debug<_char>& operator<<(unsigned long long& val)
	{
		print_binary_num("bin", val);
		print_hex_num("hex", val);
		return *this;
	}
	
	debug<_char>& operator++()
	{
		++this->additional_tabs_count;
		return *this;
	}
	
	debug<_char>& operator++(int)
	{
		++this->additional_tabs_count;
		return *this;
	}
	
	debug<_char>& operator--()
	{
		if (this->additional_tabs_count)
		{
			--this->additional_tabs_count;
		}
		return *this;
	}
	
	debug<_char>& operator--(int)
	{
		if (this->additional_tabs_count)
		{
			--this->additional_tabs_count;
		}
		return *this;
	}
	
	static void binarize(char** bin, unsigned char num)
	{
		*bin = new char[9];
		for (int i(7); i >= 0; --i)
		{
			(*bin)[i] = num % 2 + '0';
			num >>= 1;
		}
		(*bin)[8] = 0;
	}
	
	template <typename _mem_type>
	void print_bin_memory(_mem_type* mem, size_t size)
	{
		auto* mem_ = (unsigned char*)mem;
		mem_ += size - 1;
		for (::std::size_t i(0); i < size; --mem_, ++i)
		{
			char* bin;
			binarize(&bin, *mem_);
			*this->stream << bin << " ";
		}
	}
	
	static void hexify(char** hex, unsigned char num)
	{
		*hex = new char[3];
		for (int i(1); i >= 0; --i)
		{
			int rem = num % 16;
			if (rem < 10)
			{
				(*hex)[i] = rem + '0';
			}
			else
			{
				rem -= 10;
				(*hex)[i] = rem + 'a';
			}
			num /= 16;
		}
		(*hex)[2] = 0;
	}
	
	template <typename _mem_type>
	void print_hex_memory(_mem_type* mem, size_t size)
	{
		auto* mem_ = (unsigned char*)mem;
		mem_ += size - 1;
		for (::std::size_t i(0); i < size; --mem_, ++i)
		{
			char* hex;
			hexify(&hex, *mem_);
			*this->stream << hex << " ";
		}
	}
	
	void print_binary_num(const std::basic_string<_char>& name, unsigned long long& val)
	{
		for (int i(0); i < tabs_count + additional_tabs_count; ++i)
		{
			*this->stream << "\t";
		}
		*this->stream << " INFO: " << name << " = ";
		this->template print_bin_memory(&val, sizeof(val));
		*this->stream << "\n";
	}
	
	void print_hex_num(const std::basic_string<_char>& name, unsigned long long& val)
	{
		for (int i(0); i < tabs_count + additional_tabs_count; ++i)
		{
			*this->stream << "\t";
		}
		*this->stream << " INFO: " << name << " = ";
		this->template print_hex_memory(&val, sizeof(val));
		*this->stream << "\n";
	}
	
	~debug()
	{
		additional_tabs_count = 0;
		--tabs_count;
		for (int i(0); i < tabs_count; ++i)
		{
			*this->stream << "\t";
		}
		*this->stream << " << OUT: " << name << "\n";
	}
};

#define DEBUG(stream, name) debug<decltype(stream)::char_type> DEBUG(stream, name)
#define print_debug_info(variable) DEBUG.print_message(#variable, variable)
#define print_binary_debug_info(variable) DEBUG.print_binary_num(#variable, variable)
#define print_hex_debug_info(variable) DEBUG.print_hex_num(#variable, variable)


#define custom_debug(custom_member_name, stream, name) debug<decltype(stream)::char_type> custom_member_name(stream, name)
#define print_custom_debug_info(custom_member_name, variable) custom_member_name.print_message(#variable, variable)
#define print_binary_custom_debug_info(custom_member_name, variable) custom_member_name.print_binary_num(#variable, variable)
#define print_hex_custom_debug_info(custom_member_name, variable) custom_member_name.print_hex_num(#variable, variable)

template <typename _char>
long long debug<_char>::tabs_count = 0;

#endif //VERY_LONG_NUMBER_DEBUG_CONSTRUCT_H
