﻿#include "Chip8.h"
#include <vector>
#include <fstream>
#include <iostream>

Chip8::Chip8(SDL_Renderer* const renderer)
	: m_renderer(renderer)
{
	// There’s a special instruction for setting I to a character’s address,
	// so you can choose where to put it.But 050-09F is the standard used, unsure why
	constexpr std::array fonts{
		std::to_array<uint8_t>({
			0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
			0x20, 0x60, 0x20, 0x20, 0x70,  // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
			0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
			0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
			0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
			0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
			0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
		})
	};

	// Adding fonts from 0x050-0x09F (80-159)
	int start = fontStart;
	for (auto x : fonts)
	{
		memory[start] = x;
		++start;
	}
}

bool Chip8::run()
{
	// Fetch:
	// Read the instruction that PC is currently pointing at from memory. An instruction is two bytes,
	// so you will need to read two successive bytes from memory and combine them into one 16-bit instruction.
	uint16_t instruction = (memory[pc] << 8) | memory[pc + 1];
	pc += 2;

	// Decode
	// All instructions are 2 bytes long and are stored most-significant-byte first. In memory,
	// the first byte of each instruction should be located at an even addresses. If a program
	// includes sprite data, it should be padded so any instructions following it will be properly
	// situated in RAM.

	uint16_t nibble = instructionNibble(instruction);
	uint16_t x = instructionX(instruction);
	uint16_t y = instructionY(instruction);
	uint16_t n = instructionN(instruction);
	uint16_t nn = instructionNN(instruction);
	uint16_t nnn = instructionNNN(instruction);
	uint16_t kk = instructionKK(instruction);

	uint8_t xcoordinate{};
	uint8_t ycoordinate{};
	uint16_t ivalue{};
	uint16_t nthByte{};

	uint16_t temp{};

	switch (nibble)
	{
	case 0x0000:

		switch (kk)
		{
		case 0xE0:
			// CLS -> Clear the display
			for (int i = 0; i < 64; i++)
			{
				for (int j = 0; j < 32; j++)
				{
					display[i][j] = 0;
				}
			}

			break;

		case 0xEE:
			// RET -> Return from a subroutine.
			// The interpreter sets the program counter to the address at the top of the stack,
			// then subtracts 1 from the stack pointer.
			pc = mystack.top();
			mystack.pop();
			break;
		default:
			// Technically there is a SYS addr jump, but not handled anymore in more modern interpreters
			std::cerr << "Invalid Instruction: " << instruction << '\n';
			return false;

		}
		break;

	case 0x1000:
		// JP addr -> Jump to location nnn.
		// The interpreter sets the program counter to nnn.
		pc = nnn;
		break;
	case 0x2000:
		// CALL addr -> Call subroutine at nnn The interpreter increments the stack pointer,
		// then puts the current PC on the top of the stack. The PC is then set to nnn.
		mystack.push(pc);
		pc = nnn;
		break;
	case 0x3000:
		// SE Vx, byte -> Skip next instruction if Vx = kk.
		// The interpreter compares register Vx to kk, and if they are equal, increments the program counter by 2.
		if (V[x] == kk)
		{
			pc += 2;
		}
		break;
	case 0x4000:
		// SNE Vx, byte
		// Skip next instruction if Vx != kk.
		if (V[x] != kk)
		{
			pc += 2;
		}
		break;
	case 0x5000:
		// SE Vx, Vy
		// Skip next instruction if Vx = Vy.
		if (V[x] == V[y])
		{
			pc += 2;
		}
		break;
	case 0x6000:
		// LD Vx, byte
		// Set Vx = kk.
		V[x] = kk;
		break;
	case 0x7000:
		// ADD Vx, byte
		// Set Vx = Vx + kk.
		V[x] = V[x] + kk;
		break;
	case 0x8000:
		switch (n)
		{
		case 0x0:
			// LD Vx, Vy
			// Set Vx = Vy.
			V[x] = V[y];
			break;
		case 0x1:
			// OR Vx, Vy
			// Set Vx = Vx OR Vy.
			V[x] = V[x] | V[y];
			break;
		case 0x2:
			// AND Vx, Vy
			// Set Vx = Vx AND Vy.
			V[x] = V[x] & V[y];
			break;
		case 0x3:
			// XOR Vx, Vy
			// Set Vx = Vx XOR Vy.
			V[x] = V[x] ^ V[y];
			break;
		case 0x4: //modified
			// ADD Vx, Vy
			// Set Vx = Vx + Vy, set VF = carry.
			// The values of Vx and Vy are added together. If the result is greater than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest 8 bits of the result are kept, and stored in Vx.
			temp = V[x] + V[y];
			V[0xF] = (temp > 255) ? 1 : 0;
			V[x] = temp & 0xFF;
			break;
		case 0x5:
			// SUB Vx, Vy
			// Set Vx = Vx - Vy, set VF = NOT borrow.
			// If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx.
			V[0xF] = (V[x] > V[y]) ? 1 : 0;
			V[x] = V[x] - V[y];
			break;
		case 0x6://Work on this
			// SHR Vx {, Vy}
			// Set Vx = Vx SHR 1.
			// If the least - significant bit of Vx is 1, then VF is set to 1, otherwise 0. Then Vx is divided by 2.
			if ((0x1 & V[x]) == 0x1)
			{
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}
			V[x] = V[x] >> 1;
			break;
		case 0x7:
			V[0xF] = (V[y] > V[x]) ? 1 : 0;
			V[x] = V[y] - V[x];
			break;
		case 0xE:
			if ((char)V[x] < 0 == 0x1)
			{
				V[0xF] = 1;
			}
			else
			{
				V[0xF] = 0;
			}
			V[x] = V[x] << 1;

			break;
		default:
			std::cerr << "Invalid Instruction: " << std::hex << instruction << '\n';
			return false;

		}
		break;
	case 0x9000:
		if (V[x] != V[y])
		{
			pc += 2;
		}
		break;
	case 0xA000:
		I = nnn;
		break;
	case 0xB000:
		pc = nnn + V[0];
		break;
	case 0xC000:
		V[x] = (rand() % 256) & kk; // 0-255
		break;
	case 0xD000:
		//Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = 0.
		V[0xF] = 0;
		//Read n bytes from memory
		//for each byte
		for (int nth = 0; nth < n; ++nth)
		{
			ycoordinate = (V[y] % screenHeight) + nth;
			nthByte = memory[I + nth];
			for (int pixel = 0; pixel < 8; ++pixel)
			{
				xcoordinate = (V[x] % screenWidth) + pixel;
				/*If the current pixel in the sprite row is on and the pixel at coordinates X,Y
				on the screen is also on, turn off the pixel and set VF to 1*/
				bool currentPixel = (nthByte >> (7 - pixel)) & 1;
				if (currentPixel && display[xcoordinate][ycoordinate])
				{
					display[xcoordinate][ycoordinate] = 0;
					SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
					SDL_RenderDrawPoint(m_renderer, xcoordinate, ycoordinate);
					V[15] = 1;
				}
				else if (currentPixel && display[xcoordinate][ycoordinate] == 0)
				{
					//Current bit is on and display not drawn (display[xcoordinate][ycoordinate] == 0)
					display[xcoordinate][ycoordinate] = 1;
					SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
					SDL_RenderDrawPoint(m_renderer, xcoordinate, ycoordinate);
				}

				SDL_RenderPresent(m_renderer);
				if (xcoordinate >= screenWidth)
				{
					break;
				}
			}
			//If you reach the right edge of the screen, stop drawing this row
			if (ycoordinate >= screenHeight + 1)
			{
				break;
			}
		}

		break;
	case 0xE000:
		switch (kk)
		{
		case 0x9E:
			if (V[x] <= 0xF && keys[V[x]] == 1)
			{
				pc += 2;
			}
			break;
		case 0xA1:
			if (V[x] <= 0xF && keys[V[x]] == 0)
			{
				pc += 2;
			}
			break;
		}
		break;
	case 0xF000:
		switch (kk)
		{
		case 0x07:
			V[x] = delay;
			break;
		case 0x0A:
			if (keyPressed)
			{
				V[x] = scanCode;
			}
			else {
				pc -= 2;
			}
			break;
		case 0x15:
			delay = V[x];
			break;
		case 0x18:
			sound = V[x];
			break;
		case 0x1E:
			I = I + V[x];
			break;
		case 0x29:
			I = fontStart + (V[x] * 5);
			break;
		case 0x33:
			temp = V[x];//temp was an int
			memory[I + 2] = temp % 10;
			temp = temp / 10;
			memory[I + 1] = temp % 10;
			temp = temp / 10;
			memory[I] = temp % 10;
			break;
		case 0x55:
			for (int index = 0; index <= x; index++)
			{
				memory[I + index] = V[index];
			}
			break;
		case 0x65:
			for (int index = 0; index <= x; index++)
			{
				V[index] = memory[I + index];
			}
			break;
		default:
			std::cerr << instruction << " is Invalid" << '\n';
			exit(1);
			break;
		}
		break;

	default:
		break;
	}

	if (delay > 0)
	{
		--delay;
	}
	if (sound > 0)
	{
		if (sound == 1)
		{
			--sound;
		}
	}
	return true;
}
//Note: string_view is convient but ifstream needs .data() and it does NOT ensure null termination
bool Chip8::loadRom(const std::string& romFile)
{
	std::ifstream rom{ romFile, std::ios::binary | std::ios::ate };
	if (!rom)
	{
		std::cerr << "Failed to open file: " << romFile << '\n';
		return false;
	}

	//used ios::ate so already at end
	int size = rom.tellg();
	rom.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	rom.read(buffer.data(), size);

	if (rom.fail() || rom.bad())
	{
		std::cerr << "Error reading from file: " << romFile << std::endl;
		return false;
	}

	//Load the rom into memory
	for (int i = 0; i < size; i++)
	{
		memory[startIndex + i] = buffer[i];
	}
	return true;
}

void Chip8::draw(int x, int y)
{
	SDL_SetRenderDrawColor(m_renderer, 225, 225, 225, 225);
	SDL_RenderDrawPoint(m_renderer, x, y);
	SDL_RenderPresent(m_renderer);
	SDL_Delay(100);
}