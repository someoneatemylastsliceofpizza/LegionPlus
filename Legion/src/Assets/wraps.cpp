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
	Info.DebugInfo = string::Format("0x%02X | 0x%llX | 0x%llX", WrapHdr.flags, WrapHdr.unk, Asset.NameHash);
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

	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.Data.Index, WrapHdr.Data.Offset));

	std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);

	bool IsCompressed = WrapHdr.flags & 1;
	bool ContainsNullByte = WrapHdr.flags & 3;
	//bool IsUnk = WrapHdr.flags == 11;

	uint64_t Size = WrapHdr.dcmpSize;

	if(!name.Contains("bsp"))
	    Size = ContainsNullByte ? Size : Size - 1;

	if (IsCompressed)
	{
		uint8_t* tmpCmpBuf = new uint8_t[Size];

		Reader.Read(tmpCmpBuf, 0, Size);

		auto DecompStream = RTech::DecompressStreamedBuffer(tmpCmpBuf, Size, (uint8_t)CompressionType::OODLE);

		uint8_t* outBuf = new uint8_t[Size];
		DecompStream->Read(outBuf, 0, Size);

		std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);
		out.write((char*)outBuf, Size);

		DecompStream.release();
		delete[] outBuf;
	}
	else
	{
		char* buffer = new char[Size];

		Reader.Read(buffer, 0, Size);

		out.write(buffer, Size);

		delete[] buffer;
	}

	out.close();
};