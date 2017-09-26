#include "stdafx.h"
#include <cassert>

#include <iostream>
#include <bitset>

/*
So, A GOST. http://www.certisfera.ru/uploads/28147-89.pdf
*/

//Required structures
struct FourByte {
	unsigned char byte[4];
	FourByte() { for (auto& b : byte) { b = 0; } }
	bool operator==(const FourByte& other) {
		return (byte[0] == other.byte[0]) &&
			(byte[1] == other.byte[1]) &&
			(byte[2] == other.byte[2]) &&
			(byte[3] == other.byte[3]);
	}
	bool operator!=(const FourByte& other) { return !(*this == other); }

};
struct EightChar {
	unsigned char byte[8];
};
struct KeyHolder {
	FourByte X[8];
};
struct DataBlock {
	FourByte Data[2];
};

class ReplaceSubBlock {
private:
	unsigned char Table[8];//The table is actually contains of 16 4-bit elements. Where table[0] && 0xF is first element, table[0] && 0xF0 -is the second and so on
public:
	
	ReplaceSubBlock()
	{
		for (auto& Tb : Table)
		{
			Tb = 0;
		}
	}
	/**!
	Fills values in a table
	\param Value[in] - Only 4 least significant bits counts!
	\param positon[in] - Position of value in a table. Can be between 0-15
	*/
	void fillTable(unsigned char Value, unsigned int positon)
	{
		assert((Value & 0xf0) == 0);//Value contains more then 4 active bits!
		Value &= 0xf;//Leave only 4 least significant bits
		if (positon % 2 == 1)//If it's odd, than it should be located in a second part of the word. So, move!
		{
			Value = Value << 4;
			//It also means, that Every t+1'th value should be located in a t'th element of a table
			positon--;
		}

		Table[positon / 2] |= Value;
	}

	/**!
	Returns value from table, assigned to a given value
	\param[in] Value A number of value from a table to return. Warning, Only 4 least singificant bits actually used, all other bits must be set to zero
	\returns Returns value from a table, assigned to a given Value. We use only 4 least significant bits, so yeah, be carefull when contagating
	*/
	unsigned char Get(unsigned char Value) {
		assert((Value & 0xf0) == 0);//Value contains more then 4 active bits!

		auto RetVal = Table[Value / 2];//Get a word from table
		if (Value % 2 == 1)//If it's odd, then we sould use second part of the word
		{
			RetVal = RetVal >> 4;//All new bits from most significant part will be zero
		}
		else//If it's even, then get only first part
		{
			RetVal &= 0xf;
		}
		return RetVal;
	}

};

class ReplaceBlock {
private:
	ReplaceSubBlock ReplaceSubBlocks[8];

public:
	/**!
	Devides FourByte, in 8 bytes containers, so that every container contains 4 bites from FourByte in it's least significant bits.
	First container contains first 4 bits, second contains next 4 bits and so on.
	\param[in] input Input FourBytes
	\result EightChar, which every char, contains corresponding 4 bits from FourByte
	*/
	EightChar DevideFourByte(FourByte input)
	{
		//Devide 32-bit value to 8 4-bit values;
		EightChar Values;
		//Fill these values
		for (int i = 0; i < 8; i += 2)
		{
			Values.byte[i] = (input.byte[i/2] & 0xf0) >> 4;
			Values.byte[i + 1] = input.byte[i/2] & 0xf;
		}
		return Values;
	}
	
	/**!
	Concatinates EightChar in a single FourByte. A reverse to a \c DevideFourByte
	\param[in] chars  EightChar to be concatinated. Only 4 least significant bits counts and every other bit must be zero
	\returns FourByte as result of concatination of EightChar
	*/
	FourByte ConcatFourByte(EightChar chars)
	{
		FourByte Res;
		Res.byte[0] = (chars.byte[0] << 4) | chars.byte[1];
		Res.byte[1] = (chars.byte[2] << 4) | chars.byte[3];
		Res.byte[2] = (chars.byte[4] << 4) | chars.byte[5];
		Res.byte[3] = (chars.byte[6] << 4) | chars.byte[7];
		return Res;
	}

	/**!
	Sets a value in a SubBlock
	\param[in] Value A value to set. Only 4 least singificant bits counts, all other bits must be zero
	\param[in] subblock A number of subblock, of which value should be set. Even though GOST conts them from 1 to 8,
	in this realization they are counted 0-7. Just a little shift

	\param[in] position Position of a value in a block. Range: 0-15
	*/
	void SetValueInSubBlock(unsigned char Value, int subblock, int position)
	{
		assert((subblock >= 0) && (subblock < 8));//There is only 8 subblocks, counted from 0 to 7
		ReplaceSubBlocks[subblock].fillTable(Value, position);
	}

	/**!
	Perfoms a replce, according to a GOST
	\param[in] input, 32-bit value, that should be replaced
	\return A result of replacement.
	*/
	FourByte DoReplace(FourByte input)
	{
		EightChar EightCharRep = DevideFourByte(input);
		for (int i = 0; i < 8; i++)
		{
			EightCharRep.byte[i] = ReplaceSubBlocks[i].Get(EightCharRep.byte[i]);
		}
		return ConcatFourByte(EightCharRep);
	}

};

class GOST28147
{
private:
	ReplaceBlock RB;
	KeyHolder Key;

public:
	
	DataBlock DoCipher(DataBlock Data)
	{
		//Invert input DataBlock
		DataBlock InvData;
		for (int i = 0; i = 32; i++)
		{
			InvData.Data[0].byte[(i)/8]
		}
	}

};

/**!
\test Tests ReplaceSubBlock. Checks that every value in a table can be set, and that it's exactly that will be returned by \c get.
It's justs sets a value in a table, and then reads it, checking it's didn't changed. It does it sequentially. Value to set decided by rand().
It's a positive test
*/
bool TestCase1()
{

	ReplaceSubBlock Sub1;
	unsigned char TestVal = 0;
	for (int i = 0; i < 16; i++)
	{
		TestVal = rand() % 16;
		Sub1.fillTable(TestVal, i);
		if (Sub1.Get(i) != TestVal)
		{
			printf("TestCase1:ReplaceSubBlock.get returns not what it's suppose to!");
			return false;
		}

	}
	return true;
}

/**!
\test Tests ReplaceBlock. Checks if it correctly devides and concatinates FourBytes.
It randomly fills FourBytes and then Devides and concats it, checking that it stayed the same
It's a positive test
*/
bool TestCase2()
{
	FourByte Val;
	Val.byte[0] = rand() % 256;
	Val.byte[1] = rand() % 256;
	Val.byte[2] = rand() % 256;
	Val.byte[3] = rand() % 256;

	ReplaceBlock RB;
	if (RB.ConcatFourByte(RB.DevideFourByte(Val)) != Val)
	{
		printf("ReplaceBlock failed to correctly devide and concat random FourByte!");
		return false;
	}
	return true;

}

/**!
\test Tests ReplaceBlock. Checks if it can correctly act on a FourByte.
It fills every SubBlock so that they would become identity maps. Then it passes rand() fourByte through the entire ReplaceBlock and checks if it's
stayed the same
It's a positive test
*/
bool TestCase3()
{
	FourByte Val;
	Val.byte[0] = rand() % 256;
	Val.byte[1] = rand() % 256;
	Val.byte[2] = rand() % 256;
	Val.byte[3] = rand() % 256;

	ReplaceBlock RB;
	//Set identity mapping in SubBlocks
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 16; j++)
		{
			RB.SetValueInSubBlock((unsigned char)j, i, j);
		}
	}

	if (RB.DoReplace(Val) != Val)
	{
		printf("ReplaceBlock fails to leave random Value untached during replacing with identity mapper SubBlocks");
		return false;
	}
	return true;
}

int main()
{
	//Tests
	assert(TestCase1());
	assert(TestCase2());
	assert(TestCase3());


    return 0;
}

