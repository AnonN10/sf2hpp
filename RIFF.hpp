#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#ifdef RIFF_DEBUG
#include <iostream>
#endif

namespace RIFF
{
	typedef uint8_t BYTE;
	typedef uint16_t WORD;
	typedef uint32_t DWORD;
	typedef uint32_t FOURCC;

	inline bool FOURCC_equals(FOURCC code, const char* str)
	{
		return
			((char*)&code)[0] == str[0] && 
			((char*)&code)[1] == str[1] &&
			((char*)&code)[2] == str[2] &&
			((char*)&code)[3] == str[3];
	}

	inline std::string FOURCC_to_string(FOURCC code)
	{
		std::string result;
		result += ((char*)&code)[0];
		result += ((char*)&code)[1];
		result += ((char*)&code)[2];
		result += ((char*)&code)[3];

		return result;
	}

	inline FOURCC string_to_FOURCC(const char* str)
	{
		FOURCC result = 0;
		FOURCC* reee = &result;

		/*int i = 0;
		while(i < 4 && str[i] != '\0') ((char*)&result)[i] = str[i++];
		while(i < 4) ((char*)&result)[i++] = ' ';*/

		//result = (str[0] << 24) | (str[1] << 16) | (str[2] << 8) | str[3];

		int i = 0;
		while(i < 4 && str[i] != '\0')
		{
			result |= str[i] << ((i*8));
			++i;
		}
		while(i < 4) result |= ' ' << ((i*8));

		return result;
	}

	constexpr inline FOURCC constexpr_string_to_FOURCC(const char* str)
	{
		constexpr FOURCC result = 0;

		int i = 0;
		while(i < 4 && str[i] != '\0') ((char*)&result)[i] = str[i++];
		while(i < 4) ((char*)&result)[i++] = ' ';

		return result;
	}

	//"abstract" data stream
	//can be used to read from file or from memory
	struct stream
	{
		void* src;
		size_t (*func_read_ptr)(void* src, void* dest, size_t size);
		size_t (*func_skip_ptr)(void* src, size_t size);
		size_t (*func_getpos_ptr)(void* src);
		void (*func_setpos_ptr)(void* src, size_t pos);
		size_t read(void* dest, size_t size)
		{
			return func_read_ptr(src, dest, size);
		}
		size_t skip(size_t size)
		{
			return func_skip_ptr(src, size);
		}
		size_t getpos()
		{
			return func_getpos_ptr(src);
		}
		void setpos(size_t pos)
		{
			func_setpos_ptr(src, pos);
		}
	};

	struct RIFF
	{
		struct chunk {
			//A chunk ID identifies the type of data within the chunk.
			FOURCC id;
			//The size of the chunk data in bytes, excluding any pad byte.
			DWORD size;
			//The actual data plus a pad byte if req’d to word align.
			std::unique_ptr<BYTE[]> data;

			//Form type for "RIFF" chunks or the list type for "LIST" chunks.
			FOURCC type; 

			//Data stream offset of the beginning of the chunk's data member,
			//relative to the beginning of the data stream.
			size_t data_offset;

			//Calculate size padded to WORD
			size_t get_padded_data_size() {return (size % 2)?(size+1):size;}

			//Loads data from stream using saved offset
			bool load_data(stream& s)
			{
				size_t old_pos = s.getpos();
				size_t data_size = get_padded_data_size();
				data = std::make_unique<BYTE[]>(data_size);
				s.setpos(data_offset);			
				if(s.read(data.get(), data_size) < data_size)
				{
					s.setpos(old_pos);
					return false;
				}
				s.setpos(old_pos);
				return true;
			}
		};

		std::vector<std::unique_ptr<chunk>> chunks;

		//In RIFF, order of chunks matters.
		//Chunks of RIFF or LIST type contain subchunks,
		//and that's why following functions provide
		//ability to start lookup from a certain index.
		chunk* get_chunk_by_id(FOURCC id, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->id == id) return chunks[i].get();
			}
			return nullptr;
		}
		chunk* get_chunk_by_type(FOURCC type, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->type == type) return chunks[i].get();
			}
			return nullptr;
		}
		chunk* get_chunk_by_id_type(FOURCC id, FOURCC type, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->id == id && chunks[i]->type == type) return chunks[i].get();
			}
			return nullptr;
		}
		size_t get_chunk_index_by_id(FOURCC id, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->id == id) return i;
			}
			return -1;
		}
		size_t get_chunk_index_by_type(FOURCC type, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->type == type) return i;
			}
			return -1;
		}
		size_t get_chunk_index_by_id_type(FOURCC id, FOURCC type, size_t start_index = 0)
		{
			for(size_t i = start_index; i < chunks.size(); ++i)
			{
				if(chunks[i]->id == id && chunks[i]->type == type) return i;
			}
			return -1;
		}

		//Collect chunks from binary data stream
		//"load_data" flag dictates whether data is immedieately loaded
		//during parsing or ignored to be loaded manually later
		void parse(stream& s, bool load_data = false)
		{
			auto read_chunk_info = [](stream& s, chunk* c)->bool
			{
			#define read_and_check(src, dest, size)\
			{\
				if(src.read(dest, size) < size) return false;\
			}
			#define skip_and_check(src, size)\
			{\
				if(src.skip(size) < size) return false;\
			}
				//Read FOURCC id
				read_and_check(s, &c->id, sizeof(FOURCC));
				//Read chunk data size (no padding)
				read_and_check(s, &c->size, sizeof(DWORD));
				if(FOURCC_equals(c->id, "RIFF") || FOURCC_equals(c->id, "LIST"))
				{
					//Read Form/List Type
					read_and_check(s, &c->type, sizeof(FOURCC));
					//save stream position to read data when needed
					c->data_offset = s.getpos();
					c->data = nullptr;
					//don't read any data, because those chunks
					//are special and only contain subchunks
				}
				else
				{
					c->type = 0;
					//save stream position to read data when needed
					c->data_offset = s.getpos();
					c->data = nullptr;
					//skip data field
					skip_and_check(s, c->get_padded_data_size());
				}
				return true;
			#undef read_and_check
			#undef skip_and_check
			};

			while(true)
			{
				auto c = std::make_unique<chunk>();
				if(!read_chunk_info(s, c.get()))
					break;
				if(load_data && !c->load_data(s))
					break;
				chunks.emplace_back(std::move(c));
			}

#ifdef RIFF_DEBUG
			for(auto c : chunks)
			{
				std::cout << "//=======================" << std::endl;
				std::cout << "ID:" << FOURCC_to_string(c->id) << std::endl;
				std::cout << "Type:" << FOURCC_to_string(c->type) << std::endl;
				std::cout << "Size:" << c->size << std::endl;
				std::cout << "Data offset:" << c->data_offset << std::endl;
			}
#endif
		}
	};
}