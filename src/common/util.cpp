// GB Enhanced Copyright Daniel Baxter 2015
// Licensed under the GPLv2
// See LICENSE.txt for full license text

// File : util.h
// Date : August 06, 2015
// Description : Misc. utilites
//
// Provides miscellaneous utilities for the emulator

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "util.h"

namespace util
{

//CRC32 Polynomial
u32 poly32 = 0x04C11DB7;

//CRC lookup table
u32 crc32_table[256];

/****** Saves an SDL Surface to a PNG file ******/
bool save_png(SDL_Surface* source, std::string filename)
{
	//TODO - Everything for this function

	if(source == NULL) 
	{
		std::cout<<"GBE::Error - Source data for " << filename << " is null\n";
		return false;
	}
	
	//Lock SDL_Surface
	if(SDL_MUSTLOCK(source)){ SDL_LockSurface(source); }
	u8* src_pixels = (u8*)source->pixels;

	std::vector<u8> png_bytes;

	//PNG Magic Number - 8 bytes
	png_bytes.push_back(0x89);
	png_bytes.push_back(0x50);
	png_bytes.push_back(0x4E);
	png_bytes.push_back(0x47);
	png_bytes.push_back(0x0D);
	png_bytes.push_back(0x0A);
	png_bytes.push_back(0x1A);
	png_bytes.push_back(0x0A);

	//IHDR byte length (always 0x0D for us) - 4 bytes
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x0D);

	//"IHDR" ASCII string - 4 bytes
	png_bytes.push_back(0x49);
	png_bytes.push_back(0x48);
	png_bytes.push_back(0x44);
	png_bytes.push_back(0x52);

	//Image width - 4 bytes
	u32 width = source->w;

	for(int x = 24; x >= 0; x-= 8)
	{
		png_bytes.push_back((width >> x) & 0xFF);
	}

	//Image height - 4 bytes
	u32 height = source->h;

	for(int x = 24; x >= 0; x-= 8)
	{
		png_bytes.push_back((height >> x) & 0xFF);
	}

	//Bit depth (always 0x08 for us) - 1 byte
	png_bytes.push_back(0x08);

	//Color type (always 0x02 for us) 1 byte
	png_bytes.push_back(0x02);

	//Compression method (always 0x00 for us, use DEFLATE) - 1 byte
	//Filter method (always 0x00 for us) - 1 byte
	//Interlace method (always 0x00 for us) - 1 byte
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);

	//CRC32 of IHDR chunk - 4 bytes
	u32 ihdr_crc = util::get_crc32((png_bytes.data() + 12), 17);

	for(int x = 24; x >= 0; x-= 8)
	{
		png_bytes.push_back((ihdr_crc >> x) & 0xFF);
	}

	//IDAT byte length - 4 bytes
	u32 dat_length = (source->w * source->h) * 3;

	for(int x = 24; x >= 0; x-= 8)
	{
		png_bytes.push_back((dat_length >> x) & 0xFF);
	}

	//"IDAT" ASCII string - 4 bytes
	png_bytes.push_back(0x49);
	png_bytes.push_back(0x44);
	png_bytes.push_back(0x41);
	png_bytes.push_back(0x54);

	//ZLib header - 4 bytes
	png_bytes.push_back(0x78);
	png_bytes.push_back(0x00);
	//TODO Addler32 checksum of data

	//IDAT Chunk header - TODO
	//Final chunk flag - 1 Byte
	//Chunk size byte length - 2 Bytes
	//Chunk size complement - 2 Bytes
	png_bytes.push_back(0x01);

	//Grab RGB values, then store them
	for(int x = 0; x < (source->w * source->h) * 4;)
	{
		//Skip Alpha values
		x++;

		png_bytes.push_back(src_pixels[x++]);
		png_bytes.push_back(src_pixels[x++]);
		png_bytes.push_back(src_pixels[x++]);
	}

	//CRC32 of IDAT chunk - 4 bytes
	u32 idat_crc = util::get_crc32((png_bytes.data() + 37), (dat_length + 4));

	for(int x = 24; x >= 0; x-= 8)
	{
		png_bytes.push_back((idat_crc >> x) & 0xFF);
	}

	//IEND byte length (always 0x00 for us) - 4 bytes
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);
	png_bytes.push_back(0x00);

	//"IEND" ASCII string
	png_bytes.push_back(0x49);
	png_bytes.push_back(0x45);
	png_bytes.push_back(0x4E);
	png_bytes.push_back(0x44);

	//CRC32 of IEND - Precalculated
	png_bytes.push_back(0xAE);
	png_bytes.push_back(0x42);
	png_bytes.push_back(0x60);
	png_bytes.push_back(0x82);

	std::ofstream file(filename.c_str(), std::ios::binary);
	file.write(reinterpret_cast<char*> (&png_bytes[0]), png_bytes.size());
	file.close();

	//Unlock SDL_Surface
	if(SDL_MUSTLOCK(source)) { SDL_UnlockSurface(source); }

	return true;
}

/****** Returns the minimum RGB component of a color ******/
u8 rgb_min(u32 color)
{
	u8 r = (color >> 16);
	u8 g = (color >> 8);
	u8 b = color;

	u8 min = r < g ? r : g;
	min = min < b ? min : b;

	return min;
}

/****** Returns the maximum RGB component of a color ******/
u8 rgb_max(u32 color)	
{
	u8 r = (color >> 16);
	u8 g = (color >> 8);
	u8 b = color;

	u8 max = r > g ? r : g;
	max = max > b ? max : b;

	return max;
}

/****** Converts RGB to HSV ******/
hsv rgb_to_hsv(u32 color)
{
	hsv final_color;

	u8 r = (color >> 16);
	u8 g = (color >> 8);
	u8 b = color;

	u8 max_raw = rgb_max(color);

	//Calculate the color delta
	double max = rgb_max(color) / 255.0;
	double min = rgb_min(color) / 255.0;	
	double color_delta = max - min;

	//Calculate the hue
	double pre_hue = 0.0;
	double hue_r = r / 255.0;
	double hue_g = g / 255.0;
	double hue_b = b / 255.0;

	if(color_delta == 0)
	{
		//Assign a color delta of 0 to 0 degrees as the hue
		//If all the RGB components are the same, color is a pure shade of gray (black, white, somewhere in between)
		final_color.hue = 0.0;
	}

	//Calculation if Red is the greatest RGB component		
	else if(max_raw == r)
	{
		pre_hue = (hue_g - hue_b) / color_delta;
		final_color.hue = (pre_hue * 60.0);
	}

	//Calculation if Green is the greatest RGB component		
	else if(max_raw == g)
	{
		pre_hue = ((hue_b - hue_r) / color_delta) + 2;
		final_color.hue = (pre_hue * 60.0);
	}

	//Calculation if Blue is the greatest RGB component		
	else
	{
		pre_hue = ((hue_r - hue_g) / color_delta) + 4;
		final_color.hue = (pre_hue * 60.0);
	}

	if(final_color.hue < 0.0) { final_color.hue += 360.0; }

	//Calculate the saturation
	if(max > 0) { final_color.saturation = (color_delta / max); }
	else { final_color.saturation = 0.0; }

	//Calculate the value
	final_color.value = max;

	return final_color;
}

/****** Converts HSV to RGB ******/
u32 hsv_to_rgb(hsv color)
{
	u32 final_color = 0x0;

	double r, g, b = 0.0;

	s32 i;
	double f, p, q, t;

	//If there is zero saturation, this is a shade of gray
	if(color.saturation == 0) { r = g = b = color.value; }

	else 
	{
		color.hue /= 60.0;
		i = color.hue;
		f = color.hue - i;
		p = color.value * (1.0 - color.saturation);
		q = color.value * (1.0 - (color.saturation * f));
		t = color.value * (1.0 - (color.saturation * (1.0 - f)));

		switch(i)
		{
			case 0:
				r = color.value;
				g = t;
				b = p;
				break;

			case 1:
				r = q;
				g = color.value;
				b = p;
				break;

			case 2:
				r = p;
				g = color.value;
				b = t;
				break;

			case 3:
				r = p;
				g = q;
				b = color.value;
				break;

			case 4:
				r = t;
				g = p;
				b = color.value;
				break;

			default:		
				r = color.value;
				g = p;
				b = q;
				break;
		}
	}

	//Process final color
	//Convert RGB percentages to u8
	u8 out_r = (2.55 * (r * 100));
	u8 out_g = (2.55 * (g * 100));
	u8 out_b = (2.55 * (b * 100));

	final_color = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
	return final_color;
}

/****** Converts RGB to HSL ******/
hsl rgb_to_hsl(u32 color)
{
	hsl final_color;

	u8 r = (color >> 16);
	u8 g = (color >> 8);
	u8 b = color;

	//Calculate the color delta
	double max = rgb_max(color) / 255.0;
	double min = rgb_min(color) / 255.0;	
	double color_delta = max - min;

	//Calculate lightness
	final_color.lightness = (max + min) / 2.0;

	if(color_delta == 0) { final_color.hue = final_color.saturation = 0; }

	else
	{
		//Calculate saturation
		if(final_color.lightness < 0.5) { final_color.saturation = color_delta / (max + min); }
		else { final_color.saturation = color_delta / (2 - max - min); }

		//Calculate hue
		double hue_r = r / 255.0;
		double hue_g = g / 255.0;
		double hue_b = b / 255.0;

		double r_delta = (((max - hue_r ) / 6.0) + (color_delta / 2.0)) / color_delta;
		double g_delta = (((max - hue_g ) / 6.0) + (color_delta / 2.0)) / color_delta;
		double b_delta = (((max - hue_b ) / 6.0) + (color_delta / 2.0)) / color_delta;

		if(hue_r == max) { final_color.hue = b_delta - g_delta; }
		else if(hue_g == max) { final_color.hue = (1 / 3.0) + r_delta - b_delta; }
		else if(hue_b == max) { final_color.hue = (2 / 3.0 ) + g_delta - r_delta; }

		if(final_color.hue < 0 ) { final_color.hue += 1; }
   		else if(final_color.hue > 1 ) { final_color.hue -= 1; }
	}

	return final_color;
}
		
/****** Converts HSL to RGB ******/
u32 hsl_to_rgb(hsl color)
{
	u32 final_color = 0;
	
	u8 r, g, b = 0;

	if(color.saturation == 0) { r = g = b = (color.lightness * 255); }

	else
	{
		double hue_factor_1, hue_factor_2 = 0.0;

		if(color.lightness < 0.5) { hue_factor_2 = color.lightness * (1 + color.saturation); }
		else { hue_factor_2 = (color.lightness + color.saturation) - (color.saturation * color.lightness); }

		hue_factor_1 = (2 * color.lightness) - hue_factor_2;

		r = hue_to_rgb(hue_factor_1, hue_factor_2, color.hue + (1/3.0));
		g = hue_to_rgb(hue_factor_1, hue_factor_2, color.hue);
		b = hue_to_rgb(hue_factor_1, hue_factor_2, color.hue - (1/3.0));
	}

	final_color = 0xFF000000 | (r << 16) | (g << 8) | b;
	return final_color;	
}

/****** Converts an HSL hue to an RGB component ******/
u8 hue_to_rgb(double hue_factor_1, double hue_factor_2, double hue)
{
	if(hue < 0) { hue += 1; }
	else if(hue > 1) { hue -= 1; }

	if((6 * hue) < 1) { return 255 * (hue_factor_1 + (hue_factor_2 - hue_factor_1) * 6 * hue); }
	else if((2 * hue) < 1) { return 255 * hue_factor_2; }
	else if((3 * hue) < 2) { return 255 * (hue_factor_1 + (hue_factor_2 - hue_factor_1) * ((2/3.0) - hue) * 6); }

	return 255 * hue_factor_1;
}

/****** Get the perceived brightness of a pixel - Quick and dirty version ******/
u8 get_brightness_fast(u32 color)
{
	u8 r = (color >> 16);
	u8 g = (color >> 8);
	u8 b = color;

	return (r+r+r+g+g+g+g+b) >> 3;
}

/****** Blends the RGB channels of 2 colors ******/
u32 rgb_blend(u32 color_1, u32 color_2)
{
	if(color_1 == color_2) { return color_1; }

	u16 r = ((color_1 >> 16) & 0xFF) + ((color_2 >> 16) & 0xFF);
	u16 g = ((color_1 >> 8) & 0xFF) + ((color_2 >> 8) & 0xFF);
	u16 b = (color_1 & 0xFF) + (color_2 & 0xFF);

	r >>= 1;
	g >>= 1;
	b >>= 1;

	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/****** Mirrors bits ******/
u32 reflect(u32 src, u8 bit)
{
	//2nd parameter 'bit' defines which to stop mirroring, e.g. generally Bit 7, Bit 15, or Bit 31 (count from zero)

	u32 out = 0;

	for(int x = 0; x <= bit; x++)
	{
		if(src & 0x1) { out |= (1 << (bit - x)); }
		src >>= 1;
	}

	return out;
}

/****** Sets up the CRC lookup table ******/
void init_crc32_table()
{
	for(int x = 0; x < 256; x++)
	{
		crc32_table[x] = (reflect(x, 7) << 24);

		for(int y = 0; y < 8; y++)
		{
			crc32_table[x] = (crc32_table[x] << 1) ^ (crc32_table[x] & (1 << 31) ? poly32 : 0);
		}

		crc32_table[x] = reflect(crc32_table[x], 31);
	}
}

/****** Return CRC for given data ******/
u32 get_crc32(u8* data, u32 length)
{
	u32 crc32 = 0xFFFFFFFF;

	for(int x = 0; x < length; x++)
	{
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 & 0xFF) ^ (*data)];
		data++;
	}

	return (crc32 ^ 0xFFFFFFFF);
}

/****** Convert a number into hex as a C++ string ******/
std::string to_hex_str(u32 input)
{
	std::stringstream temp;

	//Auto fill with '0's
	if(input < 0x10) { temp << "0x0" << std::hex << std::uppercase << input; }
	else if((input < 0x1000) && (input >= 0x100)) { temp << "0x0" << std::hex << std::uppercase << input; }
	else if((input < 0x100000) && (input >= 0x10000)) { temp << "0x0" << std::hex << std::uppercase << input; }
	else if((input < 0x10000000) && (input >= 0x1000000)) { temp << "0x0" << std::hex << std::uppercase << input; }
	else { temp << "0x" << std::hex << std::uppercase << input; }

	return temp.str();
}

/****** Converts C++ string representing a hex number into an integer value ******/
bool from_hex_str(std::string input, u32 &result)
{
	//This function expects the hex string to contain only hexadecimal numbers and letters
	//E.g. it expects "8000" rather than "0x8000" or "$8000"
	//Returns false + result = 0 if it encounters any unexpected characters

	result = 0;
	u32 hex_size = (input.size() - 1);
	std::string hex_char = "";

	//Convert hex string into usable u32
	for(int x = hex_size, y = 0; x >= 0; x--, y+=4)
	{
		hex_char = input[x];

		if(hex_char == "0") { result += 0; }
		else if(hex_char == "1") { result += (1 << y); }
		else if(hex_char == "2") { result += (2 << y); }
		else if(hex_char == "3") { result += (3 << y); }
		else if(hex_char == "4") { result += (4 << y); }
		else if(hex_char == "5") { result += (5 << y); }
		else if(hex_char == "6") { result += (6 << y); }
		else if(hex_char == "7") { result += (7 << y); }
		else if(hex_char == "8") { result += (8 << y); }
		else if(hex_char == "9") { result += (9 << y); }
		else if(hex_char == "A") { result += (10 << y); }
		else if(hex_char == "a") { result += (10 << y); }
		else if(hex_char == "B") { result += (11 << y); }
		else if(hex_char == "b") { result += (11 << y); }
		else if(hex_char == "C") { result += (12 << y); }
		else if(hex_char == "c") { result += (12 << y); }
		else if(hex_char == "D") { result += (13 << y); }
		else if(hex_char == "d") { result += (13 << y); }
		else if(hex_char == "E") { result += (14 << y); }
		else if(hex_char == "e") { result += (14 << y); }
		else if(hex_char == "F") { result += (15 << y); }
		else if(hex_char == "f") { result += (15 << y); }
		else { result = 0; return false; }
	}

	return true;
}

/****** Convert an intger into a C++ string ******/
std::string to_str(u32 input)
{
	std::stringstream temp;
	temp << input;
	return temp.str();
}

/****** Converts a string into an integer value ******/
bool from_str(std::string input, u32 &result)
{
	result = 0;
	u32 size = (input.size() - 1);
	std::string value_char = "";

	//Convert string into usable u32
	for(int x = size, y = 1; x >= 0; x--, y *= 10)
	{
		value_char = input[x];

		if(value_char == "0") { result += 0; }
		else if(value_char == "1") { result += (1 * y); }
		else if(value_char == "2") { result += (2 * y); }
		else if(value_char == "3") { result += (3 * y); }
		else if(value_char == "4") { result += (4 * y); }
		else if(value_char == "5") { result += (5 * y); }
		else if(value_char == "6") { result += (6 * y); }
		else if(value_char == "7") { result += (7 * y); }
		else if(value_char == "8") { result += (8 * y); }
		else if(value_char == "9") { result += (9 * y); }
		else if(value_char == "-") { result *= -1; }
		else { result = 0; return false; }
	}

	return true;
}

/****** Converts a string IP address to an integer value ******/
bool ip_to_u32(std::string ip_addr, u32 &result)
{
	u8 digits[4] = { 0, 0, 0, 0 };
	u8 dot_count = 0;
	std::string temp = "";
	std::string current_char = "";
	u32 str_end = ip_addr.length() - 1;

	//IP address comes in the form 123.456.678.ABC
	//Grab each character between the dots and convert them into integer
	for(u32 x = 0; x < ip_addr.length(); x++)
	{
		current_char = ip_addr[x];

		//Convert characters into u32 when a "." is encountered
		if((current_char == ".") || (x == str_end))
		{
			if(x == str_end) { temp += current_char; }

			//If somehow parsing more than 3 dots, string is malformed
			if(dot_count == 4) { return false; }

			u32 digit = 0;

			if(!from_str(temp, digit)) { return false; }

			digits[dot_count++] = digit & 0xFF;
			temp = "";
		}

		else { temp += current_char; }
	}

	//Encode result in network byte order aka big endian
	result = (digits[0] << 24) | (digits[1] << 16) | (digits[2] << 8) | digits[3];
}	

/****** Loads icon into SDL Surface ******/
SDL_Surface* load_icon(std::string filename)
{
	SDL_Surface* source = SDL_LoadBMP(filename.c_str());

	if(source == NULL)
	{
		std::cout<<"GBE::Error - Could not load icon file " << filename << ". Check file path or permissions. \n";
		return NULL;
	}

	SDL_Surface* output = SDL_CreateRGBSurface(SDL_SWSURFACE, source->w, source->h, 32, 0, 0, 0, 0);

	//Cycle through all pixels, then set the alpha of all green pixels to zero
	u8* in_pixel_data = (u8*)source->pixels;
	u32* out_pixel_data = (u32*)output->pixels;

	for(int a = 0, b = 0; a < (source->w * source->h); a++, b+=3)
	{
		out_pixel_data[a] = (0xFF000000 | (in_pixel_data[b+2] << 16) | (in_pixel_data[b+1] << 8) | (in_pixel_data[b]));

		if(out_pixel_data[a] == 0xFF00FF00) { out_pixel_data[a] = 0; }
	}

	return output;
}

} //Namespace
