#include "utils.h"


static const uintptr_t GPT_BASE = 0x21030000;


void WriteReg32(uintptr_t baseAddr, size_t offset, uint32_t value)
{
	*(volatile uint32_t*)(baseAddr + offset) = value;
}

uint32_t ReadReg32(uintptr_t baseAddr, size_t offset)
{
	return *(volatile uint32_t*)(baseAddr + offset);
}

void Gpt3_WaitUs(int microseconds)
{
	// GPT3_INIT = initial counter value
	WriteReg32(GPT_BASE, 0x54, 0x0);

	// GPT3_CTRL
	uint32_t ctrlOn = 0x0;
	ctrlOn |= (0x19) << 16; // OSC_CNT_1US (default value)
	ctrlOn |= 0x1;          // GPT3_EN = 1 -> GPT3 enabled
	WriteReg32(GPT_BASE, 0x50, ctrlOn);

	// GPT3_CNT
	while (ReadReg32(GPT_BASE, 0x58) < microseconds)
	{
		// empty.
	}

	// GPT_CTRL -> disable timer
	WriteReg32(GPT_BASE, 0x50, 0x0);
}