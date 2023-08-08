#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"

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

	Info.Type = ApexAssetType::Wrap;
	Info.Status = ApexAssetStatus::Loaded;
	Info.Info = "N/A";
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

	RpakStream->SetPosition(this->GetFileOffset(Asset, WrapHdr.DataIndex, WrapHdr.DataOffset));
	char* buffer = new char[WrapHdr.DataSize];

	Reader.Read(buffer, 0, WrapHdr.DataSize);

	std::ofstream out(DestinationPath, std::ios::out | std::ios::binary);
	out.write(buffer, WrapHdr.DataSize - 1);
	out.close();


	delete[] buffer;
};