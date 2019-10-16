#include <cstdint>
#include <fstream>
#include <chrono>
#include <random>

const unsigned int START_ADDRESS = 0x200;
const unsigned int FONTSET_START_ADDRESS = 0x50;
const unsigned int FONTSET_SIZE = 80;
const unsigned int VIDEO_HEIGHT = 32;
const unsigned int VIDEO_WIDTH = 64;

//Sprites for characters
uint8_t fontset[FONTSET_SIZE] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

class Chip8 {

  public:

    //Components of CHIP-8
    uint8_t registers[16];
    uint8_t memory[4096];
    uint16_t index;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t delayTimer;
    uint8_t soundTimer;
    uint8_t keypad[16];
    uint32_t video[VIDEO_WIDTH * VIDEO_HEIGHT];
    uint16_t opcode;

    //Helper member variables
    std::default_random_engine randGen;
    std::uniform_int_distribution<uint8_t> randByte;

    //Constructor
    Chip8() : randGen(std::chrono::system_clock::now().time_since_epoch().count())
    {

      // Initialize PC
      pc = START_ADDRESS;

      // Load fonts into memory
      for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
      {
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
      }

      // Initialize RNG
      randByte = std::uniform_int_distribution<uint8_t>(0, 255U);

    }

    void LoadROM(char const* filename) {
      // Open the file as a stream of binary and move the file pointer to the end
      std::ifstream file(filename, std::ios::binary | std::ios::ate);

      if (file.is_open())
      {
        // Get size of file and allocate a buffer to hold the contents
        std::streampos size = file.tellg();
        char* buffer = new char[size];

        // Go back to the beginning of the file and fill the buffer
        file.seekg(0, std::ios::beg);
        file.read(buffer, size);
        file.close();

        // Load the ROM contents into the Chip8's memory, starting at 0x200
        for (long i = 0; i < size; ++i)
        {
          memory[START_ADDRESS + i] = buffer[i];
        }

        // Free the buffer
        delete[] buffer;
      }
    }
    
    
    /**
     * 00E0: CLS
     * Clears the display.
     */
    void OP_00E0() {
      memset(video, 0, VIDEO_HEIGHT * VIDEO_WIDTH);
    }
    
    /**
     * 00EE: RET
     * Returns from a subroutine.
     */
    void OP_00EE() {
      --sp;
      pc = stack[sp];
    }

    /**
     * 1nnn: JP addr
     * Jumps to location nnn.
     */
    void OP_1nnn() {
      uint16_t address = opcode & 0x0FFFu; //bitmask to get location
      pc = address;
    }

    /**
     * 2nnn: CALL addr
     * Calls subroutine at location nnn.
     */
    void OP_2nnn() {
      uint16_t address = opcode & 0x0FFFu;
      pc = address;
      stack[sp] = pc;
      ++sp;
      pc = address;
    }

    /**
     * 3xkk: SE Vx, byte
     * Skips next instruction if Vx = kk.
     */
    void OP_3xkk() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      if (registers[Vx] == byte) {
        pc += 2;
      }
    }

    /**
     * 4xkk: SNE Vx, byte
     * Skips next instruction if Vx != kk.
     */
    void OP_4xkk() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      if (registers[Vx] != byte) {
        pc += 2;
      }
    }

    /**
     * 5xy0: SE Vx, Vy
     * Skips next instruction if Vx = Vy.
     */
    void OP_5xy0() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      if (registers[Vx] == registers[Vy]) {
        pc += 2;
      }
    }

    /**
     * 6xnn: LD Vx, nn
     * Loads 8-bit number nn into register Vx.
     */
    void OP_6xnn() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      registers[Vx] = byte;
    }

    /**
     * 7xnn: ADD Vx, nn
     * Adds number nn to register Vx.
     */
    void OP_7xnn() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      registers[Vx] += byte;
    }

    /**
     * 8xy0: LD Vx, Vy
     * Sets register Vx with the value of register Vy.
     */
    void OP_8xy0() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      registers[Vx] = registers[Vy];
    }

    /**
     * 8xy1: OR Vx, Vy
     * Logical OR values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy1() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      registers[Vx] |= registers[Vy];
    }

    /**
     * 8xy2: AND Vx, Vy
     * Logical AND values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy2() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      registers[Vx] &= registers[Vy];
    } 

    /**
     * 8xy3: XOR Vx, Vy
     * Logical XOR values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy3() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      registers[Vx] ^= registers[Vy];
    }

    /**
     * 8xy4: ADD Vx, Vy
     * Adds values of registers Vx and Vy and stores them in Vx.
     * If the result is more than 8 bits, register VF (16) is 
     * set to 1, otherwise 0.
     */
    void OP_8xy4() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      uint16_t result = registers[Vx] + registers[Vy];
      if (result > 255u){
        registers[15] = 1;
      } else {
        registers[15] = 0;
      }
      registers[Vx] += registers[Vy];
      //registers[Vx] = result & 0xFFu;
    }

    /**
     * 8xy4: SUB Vx, Vy
     * Subtracts value of registers Vy from Vx and stores the result in Vx.
     * If value of Vx is more than Vy, register VF (16) is 
     * set to 1, otherwise 0.
     */
    void OP_8xy5() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      if (registers[Vx] > registers[Vy]){
        registers[15] = 1;
      } else {
        registers[15] = 0;
      }
      registers[Vx] -= registers[Vy];
      //registers[Vx] = result & 0xFFu;
    }

    /**
     * 8xy6: SHR Vx, Vy
     * Store value of register Vy shifted right by one bit in register
     * Vx. Set register VF to value of LSB of Vy before the shift.
     */
    void OP_8xy6() {
      uint8_t Vx = (opcode & 0x0F00) >> 8u;
      uint8_t Vy = (opcode & 0x00F0) >> 4u;
      registers[15] = (registers[Vy] & 0x1u);
      registers[Vx] = (registers[Vy]>>1);
    } 




};
