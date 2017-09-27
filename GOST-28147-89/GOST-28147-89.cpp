#include "stdafx.h"
#include <cassert>

#include <iostream>
#include <bitset>
#include <string>

/*
So, A GOST. http://www.certisfera.ru/uploads/28147-89.pdf
*/

/**
Performs circular shift on 11 positions
*/
inline std::bitset<32> Rot11(std::bitset<32> Val)
{
	return (Val << 11) | (Val >> 21);
}

inline std::bitset<32> Sum2321(std::bitset<32> Val1, std::bitset<32> Val2)
{
	//Addition by modulo (2^32)-1
	return  std::bitset<32>(Val1.to_ullong() + Val2.to_ullong() % (long long)4294967295);
}

std::string GetRandomString(int size)
{
	std::string Str;
	for (int i = 0; i < size; i++)
	{
		Str += (unsigned char)(rand() % 256);
	}
	return Str;
}

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


	FourByte(std::bitset<32> bits) {
		byte[0] = (bits & std::bitset<32>(0xFF)).to_ullong();
		byte[1] = ((bits & (std::bitset<32>(0xFF) << 8)) >> 8).to_ullong();
		byte[2] = ((bits & (std::bitset<32>(0xFF) << 16 )) >> 16).to_ullong();
		byte[3] = ((bits & (std::bitset<32>(0xFF) << 24 )) >> 24).to_ullong();
	}

	std::bitset<32> toBitset() {

		return std::bitset<32>((byte[0]) | (byte[1] << 8) | (byte[2] << 16) | (byte[3] << 24));
	}
	
};
struct EightChar {
	unsigned char byte[8];
};
struct KeyHolder {
	std::bitset<32> X[8];
	KeyHolder() {};
	KeyHolder(std::string Str)
	{
		FourByte Tmp;
		for (int i = 0; i < 8; i++)
		{
			Tmp.byte[0] = Str[i * 4 + 0];
			Tmp.byte[1] = Str[i * 4 + 1];
			Tmp.byte[2] = Str[i * 4 + 2];
			Tmp.byte[3] = Str[i * 4 + 3];
			X[i] = Tmp.toBitset();
		}
	}
	std::string toStr()
	{
		std::string Str;
		for (int i = 0; i < 8; i++)
		{
			Str += FourByte(X[i]).byte[0];
			Str += FourByte(X[i]).byte[1];
			Str += FourByte(X[i]).byte[2];
			Str += FourByte(X[i]).byte[3];
		}
		return Str;
	}
};
struct DataBlock {
	FourByte Data[2];

	DataBlock() {}
	DataBlock(std::string Str)
	{
		for (int i = 0; i < 8; i++)
		{
			Data[i / 4].byte[i % 4] = Str[i];
		}
	}

	std::string ToString()
	{
		std::string str;
		for (int i = 0; i < 8; i++)
		{
			str += Data[i / 4].byte[i % 4];
		}
		return str;
	}
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
			//Clear previous values, if there were any. Leaving only first part
			Table[positon / 2] &= 0xf;
		}
		else
		{
			//Clear previous values, if there were any. Leaving only second part
			Table[positon / 2] &= 0xf0;
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

	void SetKey(KeyHolder NewKey)
	{
		Key = NewKey;
	}
	KeyHolder getKey() { return Key; }
	void SetValueInSubBlockofRB(unsigned char Value, int subblock, int position)
	{
		RB.SetValueInSubBlock(Value, subblock, position);
	}

	DataBlock InverseDataBlock(DataBlock Data)
	{
		DataBlock InvData;
		std::bitset<32> N1, N2;
		N1 = Data.Data[0].toBitset();
		N2 = Data.Data[1].toBitset();

		std::bitset<32> RetN1, RetN2;
		for (int i = 0; i < 32; i++)
		{
			//RetN1[i] = N1[31 - i];
			//RetN2[i] = N2[31 - i];

			RetN1[i] = N1[i];
			RetN2[i] = N2[i];

		}
		InvData.Data[0] = RetN1;
		InvData.Data[1] = RetN2;
		return InvData;
	}

	/**!
	Performs one round of cipher.
	\param[in] Data. Values of N1 and N2 registers
	\param[in] KeyPart What key part should be used. Range:0-7; More about it in GOST
	\param[in] LastCycle. In last cycle, Result of calculations should be written in N2, leaving N1 unchanged. In every other cycle, N1 get's the values, and it's previous value becames filling of N2
	*/
	DataBlock DoRound(DataBlock Data, int KeyPart, bool LastCycle)
	{
		//KeyPart and first part of data is added by modulo 2^32.
		std::bitset<32> CM1 = Data.Data[0].toBitset().to_ullong() + Key.X[KeyPart].to_ullong();//Hopefully, this will do as 2^32 modulo addition.
																			  //Resuly of addition get passed through ReplaceBlock
		auto ReplaceResult = RB.DoReplace(FourByte(CM1));
		//Then it get shifted by 11 to the most significant bits

		auto R = Rot11(ReplaceResult.toBitset());
		//And then the result get's bitwise modulo 2 added with second part of InvData
		auto CM2 = R^Data.Data[1].toBitset();		
		if (!LastCycle)
		{
		//Old filling of first part of InvDTB is written to a second part
		Data.Data[1] = Data.Data[0];
		//While value of CM2 is became filling of first part of InvDTB
		Data.Data[0] = CM2;
		}
		else
		{//In last cycle, result it inserted in N2. N1 leaves unchanged
			Data.Data[1] = CM2;
		}
		return Data;
	}

	
	
	DataBlock DoCipher(DataBlock Data)
	{
		//Invert input DataBlock
		auto InvDTB = InverseDataBlock(Data);

		for(int i = 0; i < 24; i++)
		{//Through 1st up to 24th round,  Keys cyclically goes from 0 to 7
			InvDTB = DoRound(InvDTB, i % 8, false);
		}
		for (int i = 7; i > 0; i--)
		{//Through 24st up to 33th round,  Keys goes from 7 to 1
			InvDTB = DoRound(InvDTB, i, false);
		}
		//32th round goes with 0th part of a key, and with a flag, that it's a last round
		InvDTB = DoRound(InvDTB, 0, true);
		
		return InvDTB;
	}

	DataBlock DoDecipher(DataBlock Data)
	{
		//Invert input DataBlock
		auto InvDTB = InverseDataBlock(Data);

		for (int i = 0; i < 8; i++)
		{//Through 1st up to 8th round,  Keys cyclically goes from 0 to 8
			InvDTB = DoRound(InvDTB, i % 8, false);
		}

		for (int i = 0; i < 23; i++)
		{//Through 9st up to 31th round,  Keys cyclically goes from 8 to 0
			InvDTB = DoRound(InvDTB, 7-(i % 8), false);
		}
		//And the last one
		InvDTB = DoRound(InvDTB, 0, true);

		return InvDTB;
	}

	void SetReplaceBlockFromString(std::string Str)
	{
		for(int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 16; j++)
			{
				std::string tmpStr = "0x0";
				tmpStr += Str[i * 16 + j];
				
				RB.SetValueInSubBlock((unsigned char) std::strtol(tmpStr.c_str(), nullptr, 16), i, j);
			}
		}
	}

	/**!
	Encrypts a string of data.
	Key and ReplaceBlock must be set before any cryptographic operations!
	\param[in] Data A string of data, to be encrypted
	\returns Returns encrypted string
	*/
	std::string SRMode_Encode(std::string Data)
	{
		//First 8 bytes encode size of string
		DataBlock SizeBlock;
		
		{
			auto SizeBitRep = std::bitset<64>(Data.size());
			std::bitset<32> FirstPart, SecondPart;
			for (int i = 0; i < 32; i++)
			{
				FirstPart[i] = SizeBitRep[i];
				//SecondPart[i] = SizeBitRep[32 + i];
			}
			SizeBlock.Data[0] = FirstPart;
			SizeBlock.Data[1] = SecondPart;
		}
		
		std::string ResultString;

		ResultString += DoCipher(SizeBlock).ToString();

		//Fill Data untill so that it can be evenly distributed between blocks
		std::string DataToEncode = Data;
		while (DataToEncode.size() % 8 != 0) { DataToEncode += ' '; }

		DataBlock DataBlock;
		//Every block contains 8 characters.
		for (int i = 0; i < DataToEncode.size(); i+=8)
		{
			
			DataBlock = DataToEncode.substr(i, 8);
			

			ResultString += DoCipher(DataBlock).ToString();
		}

		return ResultString;
	}

	/**!
	Decrypts a string of data.
	Key and ReplaceBlock must be set before any cryptographic operations!
	\param[in] Data A string of cipher, to be encrypted. A string must be prepared exclusively by analogous cryptographic system!
	\returns Returns decrypted string
	*/
	std::string SRMode_Decode(std::string Cipher)
	{
		//First 8 bytes Decode size of string
		DataBlock SizeBlock;
		SizeBlock = DoDecipher(Cipher.substr(0, 8));

		int OverallStringSize = 0;
		{
			std::bitset<64> SizeBitRep;
			for (int i = 0; i < 32; i++)
			{				
				SizeBitRep[i] = SizeBlock.Data[0].toBitset()[i];
				SizeBitRep[32 + i] = SizeBlock.Data[1].toBitset()[i];
			}
			OverallStringSize = SizeBitRep.to_ullong();
		}
		

		std::string ResultString;

		DataBlock DataBlock;
		//Every block contains 8 characters. And the first of them we allready processed. It contained String Size
		for (int i = 8; i < Cipher.size(); i += 8)
		{
			DataBlock = Cipher.substr(i, 8);
			ResultString += DoDecipher(DataBlock).ToString();
		}
		//Leave only actual data!
		ResultString = ResultString.substr(0, OverallStringSize);
		return ResultString;
	}

	/**!
	Creates one block of gamma-data based on Initialization Vector
	\param[in] IV Initialization vector
	\returns Returns a block of gamma-data
	*/
	DataBlock GetGammaBlock(DataBlock &IV, DataBlock& N3N4, bool FirstBlock)
	{
		DataBlock N1N2 = IV;
		//Constants from GOST
		FourByte C1(std::bitset<32>(33686024)), C2(std::bitset<32>(33686018));
		//Perform SimpleReplacement on IV
		if (FirstBlock)
		{//If it's first block, then before actual part, we must ReplaceBlock the IV;
			N3N4 = DoCipher(N1N2);
		}
		//N4 Summates modulo 2^32-1 with C1
		N3N4.Data[1] = Sum2321(N3N4.Data[1].toBitset(), C1.toBitset());
		//N3 Summates modulo 2^32 with C2
		N3N4.Data[0] = std::bitset<32>((N3N4.Data[0].toBitset().to_ullong() + C2.toBitset().to_ullong()));

		N1N2 = N3N4;

		N1N2 = DoCipher(N1N2);

		IV = N1N2;

		return N1N2;
	}

	/**
	Performs gammation over a string of data, using setted Key, ReplacementBlock and Initial Vector
	\param[in] Data A data to gammate over. Plaintext becames ciphertext. Ciphertext becames Plaintext. Sun and Moon, Yin and Yang.
	\param[in] IV Inintial Vector. Must be identical in encrypt/decrypt pair
	\return Returns a gammated version of input data
	*/
	std::string Gammate(std::string Data, DataBlock IV)
	{
		std::string ResultString;
		//How many blocks we need?
		int BlockNumber = Data.size() / 8;
		
		DataBlock N3N4Registers;
		bool FirstBlock = true;//Controlling first block
		for (int i = 0; i < BlockNumber; i++)
		{		
			DataBlock ReturnedGamma = GetGammaBlock(IV, N3N4Registers, FirstBlock);
			if (FirstBlock) { FirstBlock = false; }

			DataBlock ResultData;
			auto InputDataBlock = DataBlock(Data.substr(i*8, 8));
			ResultData.Data[0] = InputDataBlock.Data[0].toBitset() ^ ReturnedGamma.Data[0].toBitset();
			ResultData.Data[1] = InputDataBlock.Data[1].toBitset() ^ ReturnedGamma.Data[1].toBitset();

			ResultString += ResultData.ToString();
		}


		//And if there some leftout, do another block!
		if (Data.size() % 8 != 0) {

			DataBlock ReturnedGamma = GetGammaBlock(IV, N3N4Registers, FirstBlock);
			auto InputDataStrToBeAugmented = Data.substr((Data.size() / 8)*8, 8);
			for (int i = 0; i < Data.size() % 8; i++) { InputDataStrToBeAugmented += ' '; }
			DataBlock InputDataBlock(InputDataStrToBeAugmented);

			DataBlock ResultData;
			//Just for simplicity, Add complete InputDataBlock to 8 chars, so that we could perform XOR normally
			ResultData.Data[0] = InputDataBlock.Data[0].toBitset() ^ ReturnedGamma.Data[0].toBitset();
			ResultData.Data[1] = InputDataBlock.Data[1].toBitset() ^ ReturnedGamma.Data[1].toBitset();

			for (int i = 0; i < Data.size() % 8; i++)
			{
				ResultString += ResultData.Data[i / 4].byte[i % 4];
			}
		}
		return ResultString;
	}

	/**!
	Performs chained gammation, using setted Key, ReplacementBlock and Initial Vector
	\param[in] Data A data to gammate over.
	\param[in] IV Inintial Vector. Must be identical in encrypt/decrypt pair
	\param[in] Decrypt If set to true, method will decrypt, otherwise it's encrypting
	*/
	std::string ChainGammate(std::string Data, DataBlock IV, bool Decrypt)
	{

		std::string ResultString;
		//How many blocks we need?
		int BlockNumber = Data.size() / 8;
		std::string DataCopy(Data);
		DataBlock N1N2 = IV;
		if (Data.size() % 8 != 0)
		{
			for (int i = 0; i < 8 - Data.size() % 8; i++) { DataCopy += ' '; }
			BlockNumber++;
		}

		N1N2 = DoCipher(N1N2);
		for (int i = 0; i < BlockNumber; i++)
		{
			DataBlock ResultData;
			auto InputDataBlock = DataBlock(DataCopy.substr(i * 8, 8));
			
			ResultData.Data[0] = InputDataBlock.Data[0].toBitset() ^ N1N2.Data[0].toBitset();
			ResultData.Data[1] = InputDataBlock.Data[1].toBitset() ^ N1N2.Data[1].toBitset();
			
			Decrypt ? N1N2 = InputDataBlock : N1N2 = ResultData;


			ResultString += ResultData.ToString();
		}

		return ResultString.substr(0, Data.size());
	}
};

void SetRFC4357ReplaceBlock(GOST28147& GOST)
{
	GOST.SetReplaceBlockFromString(
		"4A92D80E6B1C7F53" 
		"EB4C6DFA23810759" 
		"581DA342EFC7609B"
		"7DA1089FE46CB253"
		"6C715FD84A9E03B2"
		"4BA0721D36859CFE"
		"DB413F590AE7682C"
		"1FD057A4923E6B8C"	
	);
}

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
\test Tests ReplaceSubBlock. Checks random access 1000 times \c get.
Fills random position with random value, and then retrives it.
It's a positive test
*/
bool TestCase4()
{

	ReplaceSubBlock Sub1;
	unsigned char TestVal = 0;
	int Pos = 0;
	for (int i = 0; i < 1000; i++)
	{
		TestVal = rand() % 16;
		Pos = rand() % 16;
		Sub1.fillTable(TestVal, Pos);
		if (Sub1.Get(Pos) != TestVal)
		{
			printf("TestCase4:ReplaceSubBlock.get returns not what it's suppose to!");
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

/**!
\test Tests GOST28147. Checks if it can correctly encrypt and decrypt values.
It fills ReplaceBlock with values from RFC4357, sets a key(Random string), and encrypts/decrypts other random string.
It's a positive test
*/
bool TestCase5()
{
	GOST28147 Cipher;
	std::string Key = GetRandomString(4 * 8);
	Cipher.SetKey(KeyHolder(Key));

	SetRFC4357ReplaceBlock(Cipher);

	std::string Data = GetRandomString(4 * 2);
	DataBlock DBL(Data);
	if (Cipher.DoDecipher(Cipher.DoCipher(DBL)).ToString() != DBL.ToString())
	{
		printf("Fail! key:%s, Data:%s", Cipher.getKey().toStr().c_str(), DBL.ToString().c_str());
		return false;
	}
	return true;
}

/**!
\test Tests GOST28147. Checks if it actually uses key
It fills ReplaceBlock with values from RFC4357, sets a key(Random string), and encrypts random block of data. Then it changes key, and cheks, that
result of decryption is different from original block.
It's a negative test
\warning. key-space is 2^256 while datablocks only 2^64, so there are chances, that random block of data can be encrypted/decrypted with two different keys, so this test can be false-positive
*/
bool TestCase6()
{
	GOST28147 Cipher;
	std::string Key = GetRandomString(4 * 8);
	Cipher.SetKey(KeyHolder(Key));

	SetRFC4357ReplaceBlock(Cipher);

	std::string Data = GetRandomString(4 * 2);
	DataBlock DBL(Data);

	auto EncRes = Cipher.DoCipher(DBL);
	std::string NewKey = GetRandomString(4 * 8);
	while (NewKey == Key) { NewKey = GetRandomString(4 * 8); }//Make shure that keys do differ
	Cipher.SetKey(NewKey);
	auto DecRes = Cipher.DoDecipher(EncRes);

	if (DecRes.ToString() == DBL.ToString())
	{
		printf("Fail! Data have been deciphered with different key!\nkey:%s\nNewKey:%s\nData:%s", Key.c_str(), NewKey.c_str(), DBL.ToString().c_str());
		printf("Warning! This test can be false-positive! Check documentation.");
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
	assert(TestCase4());
	assert(TestCase5());
	assert(TestCase6());

	GOST28147 Cipher;
	DataBlock DBL("gribheox");

	SetRFC4357ReplaceBlock(Cipher);	
	Cipher.SetKey(KeyHolder("1324adewxhvj8469kjgyxcshujgvjuyd"));
	printf("KEY:%s\n", Cipher.getKey().toStr().c_str());

	auto ResStr = Cipher.SRMode_Decode(Cipher.SRMode_Encode("Blue shadows on a blue water"));


	auto ResStr2 = Cipher.Gammate(Cipher.Gammate("Blue shadows on a blue water", DataBlock("6741gfes")), DataBlock("6741gfes"));


	auto ResStr3 = Cipher.ChainGammate(Cipher.ChainGammate("Is it Realy Working? Eh?", DataBlock("6741gfes"), false), DataBlock("6741gfes"), true);
	
	printf("ORIGINAL  : %s\n", DBL.ToString().c_str());
	auto CipherText = Cipher.DoCipher(DBL);
	printf("CIPHERTEXT: %s\n", CipherText.ToString().c_str());
	auto Decipher = Cipher.DoDecipher(CipherText);
	printf("DECIPHER  : %s\n", Decipher.ToString().c_str());
    return 0;
}

