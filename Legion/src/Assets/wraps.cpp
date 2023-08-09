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
	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.Name.Index, WrapHdr.Name.Offset));
	Info.Name = StripName(Reader.ReadCString());

	if (!ExportManager::Config.GetBool("UseFullPaths"))
		Info.Name = IO::Path::GetFileNameWithoutExtension(Info.Name).ToLower();

	bool IsCompressed = WrapHdr.flags & 1;

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
	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.Name.Index, WrapHdr.Name.Offset));
	string name = StripName(Reader.ReadCString());

	string dirpath = IO::Path::Combine(Path, IO::Path::GetDirectoryName(name));

	IO::Directory::CreateDirectory(dirpath);

	string DestinationPath = IO::Path::Combine(Path, name);

	if (!Utils::ShouldWriteFile(DestinationPath))
		return;

	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.Data.Index, WrapHdr.Data.Offset));

	std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);

	bool IsCompressed = WrapHdr.flags & 1;

	if ((WrapHdr.flags & 0x10) == 0)
	{
		if (IsCompressed)
		{
			uint64_t Size = WrapHdr.dcmpSize;
			uint8_t* tmpCmpBuf = new uint8_t[Size];

			Reader.Read(tmpCmpBuf, 0, Size);

			auto DecompStream = RTech::DecompressStreamedBuffer(tmpCmpBuf, Size, (uint8_t)CompressionType::OODLE);

			uint8_t* outBuf = new uint8_t[WrapHdr.dcmpSize];
			DecompStream->Read(outBuf, 0, WrapHdr.dcmpSize);

			std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);
			out.write((char*)outBuf, WrapHdr.dcmpSize);

			DecompStream.release();
			delete[] outBuf;
		}
		else
		{
			char* buffer = new char[WrapHdr.dcmpSize];

			Reader.Read(buffer, 0, WrapHdr.dcmpSize);

			out.write(buffer, WrapHdr.dcmpSize);

			delete[] buffer;
		}
	}

	out.close();
};