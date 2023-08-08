#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"
#include "rtech.h"

string StripName(string name)
{
	return name.Replace(".client", "").Replace(".ui", "");
}

void RpakLib::BuildWrapInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));
	WrapHeader WrapHdr = Reader.Read<WrapHeader>();

	// name
	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.NameIndex, WrapHdr.NameOffset));
	Info.Name = StripName(Reader.ReadCString());

	if (!ExportManager::Config.GetBool("UseFullPaths"))
		Info.Name = IO::Path::GetFileNameWithoutExtension(Info.Name).ToLower();

	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.DataIndex, WrapHdr.DataOffset));

	bool IsCompressed = Reader.Read<uint16_t>() == 0xC8C;

	Info.Info = IsCompressed ? "Compressed" : "N/A";

	Info.Type = ApexAssetType::Wrap;
	Info.Status = ApexAssetStatus::Loaded;
}

void RpakLib::ExportWrap(const RpakLoadAsset& Asset, const string& Path)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));
	WrapHeader WrapHdr = Reader.Read<WrapHeader>();

	// name
	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.NameIndex, WrapHdr.NameOffset));
	string name = StripName(Reader.ReadCString());

	string dirpath = IO::Path::Combine(Path, IO::Path::GetDirectoryName(name));

	IO::Directory::CreateDirectory(dirpath);

	string DestinationPath = IO::Path::Combine(Path, name);

	if (!Utils::ShouldWriteFile(DestinationPath))
		return;

	uint64_t datapos = this->GetFileOffset(Asset, WrapHdr.DataIndex, WrapHdr.DataOffset);

	RpakStream->SetPosition(datapos);
	bool IsCompressed = Reader.Read<uint16_t>() == 0xC8C;
	RpakStream->SetPosition(datapos);

	if (IsCompressed)
	{
		uint64_t cmpSize = WrapHdr.DataSize;

		uint8_t* tmpCmpBuf = new uint8_t[cmpSize];

		Reader.Read(tmpCmpBuf, 0, WrapHdr.DataSize);

		// read into vg stream and decompress
		auto DecompStream = RTech::DecompressStreamedBuffer(tmpCmpBuf, cmpSize, (uint8_t)CompressionType::OODLE);

		uint8_t* outBuf = new uint8_t[cmpSize];

		DecompStream->Read(outBuf, 0, cmpSize);

		std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);
		out.write((char*)outBuf, cmpSize);
		out.close();

		delete[] outBuf;
	}
	else
	{
		char* buffer = new char[WrapHdr.DataSize];

		Reader.Read(buffer, 0, WrapHdr.DataSize);

		std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);
		out.write(buffer, WrapHdr.DataSize - 1);
		out.close();

		delete[] buffer;
	}
};