#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"
#include "rtech.h"

static string GetSizeinString(uint64_t size) {
	const uint32_t KB = 1024;
	const uint32_t MB = 1024 * 1024;

	if (size >= MB)
		return string::Format("%.2f MB", (float)size / MB);

	if (size >= KB)
		return string::Format("%.2f KB", (float)size / KB);

	return string::Format("%d B", size);
}

void RpakLib::BuildWrapInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));
	WrapHeader WrapHdr = Reader.Read<WrapHeader>();

	Info.Name = string::Format("Wrap_0x%llX", Asset.NameHash);

	// nam
	if (WrapHdr.Name.Index || WrapHdr.Name.Offset)
	{
		Info.Name = this->ReadStringFromPointer(Asset, WrapHdr.Name);
		if (WrapHdr.nameLength)
			Info.Name = Info.Name.Substring(0, WrapHdr.nameLength);
	}

	if (!ExportManager::Config.GetBool("UseFullPaths"))
		Info.Name = IO::Path::GetFileNameWithoutExtension(Info.Name).ToLower();

	bool IsCompressed = WrapHdr.flags & 1;

	Info.Type = ApexAssetType::Wrap;
	Info.Status = ApexAssetStatus::Loaded;
	Info.Info = string::Format("%s | %s", GetSizeinString(WrapHdr.dcmpSize).ToCString(), IsCompressed ? GetSizeinString(WrapHdr.cmpSize).ToCString() : "N/A");
	Info.DebugInfo = string::Format("0x%02X | 0x%llX", WrapHdr.flags, Asset.NameHash);
}

void RpakLib::ExportWrap(const RpakLoadAsset& Asset, const string& Path)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));
	WrapHeader WrapHdr = Reader.Read<WrapHeader>();

	string name = string::Format("Wrap_0x%llX", Asset.NameHash);

	// nam
	if (WrapHdr.Name.Index || WrapHdr.Name.Offset)
	{
		name = this->ReadStringFromPointer(Asset, WrapHdr.Name);
		if (WrapHdr.nameLength)
			name = name.Substring(0, WrapHdr.nameLength);
	}

	string dirpath = IO::Path::Combine(Path, IO::Path::GetDirectoryName(name));

	IO::Directory::CreateDirectory(dirpath);

	string DestinationPath = IO::Path::Combine(Path, name);

	if (!Utils::ShouldWriteFile(DestinationPath))
		return;
	  
	bool IsCompressedBigger = WrapHdr.cmpSize > WrapHdr.dcmpSize;
	bool IsCompressed = WrapHdr.flags & 1 && !IsCompressedBigger;
	bool ContainsNullByte = WrapHdr.flags & 3;
	bool IsStreamed = Asset.OptimalStarpakOffset != -1 || Asset.StarpakOffset != -1;

	uint64_t Size = WrapHdr.dcmpSize;

	if (!name.Contains("bsp"))
		Size = ContainsNullByte ? Size : Size - 1;

	std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);

	uint8_t* tmpBuf = new uint8_t[Size];

	if (!IsStreamed)
	{
		RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.Data.Index, WrapHdr.Data.Offset));
		Reader.Read(tmpBuf, 0, Size);
		Reader.Close();
	}
	else
	{
		std::unique_ptr<IO::FileStream> StarpakStream = nullptr;
		uint64_t StreamOffset = 0;

		if (Asset.OptimalStarpakOffset != -1)
		{
			StreamOffset = Asset.OptimalStarpakOffset & 0xFFFFFFFFFFFFFF00;
			StarpakStream = this->GetStarpakStream(Asset, true);
		}
		else if (Asset.StarpakOffset != -1)
		{
			StreamOffset = Asset.StarpakOffset & 0xFFFFFFFFFFFFFF00;
			StarpakStream = this->GetStarpakStream(Asset, false);
		}

		uint64_t OutputOffset = IsCompressedBigger ? (StreamOffset - (WrapHdr.dcmpSize - WrapHdr.cmpSize)) : StreamOffset;

		StarpakStream->SetPosition(OutputOffset);
		IO::BinaryReader StarReader = IO::BinaryReader(StarpakStream.get(), true);

		StarReader.Read(tmpBuf, 0, Size);
		StarReader.Close();
	}

	if (IsCompressed)
	{
		std::unique_ptr<IO::MemoryStream> DecompStream = RTech::DecompressStreamedBuffer(tmpBuf, Size, (uint8_t)CompressionType::OODLE);

		uint8_t* outtmpBuf = new uint8_t[Size];

		DecompStream->Read(outtmpBuf, 0, Size);

		out.write((char*)outtmpBuf, Size);

		DecompStream.release();

		delete[] outtmpBuf;
	}
	else
	{
		out.write((char*)tmpBuf, Size);
		delete[] tmpBuf;
	}

	out.close();
};