#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"

void RpakLib::BuildWrapInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));
	WrapHeader WrapHdr = Reader.Read<WrapHeader>();


	RpakFile& File = this->LoadedFiles[Asset.FileIndex];


	auto CPUBLOCK = File.SegmentBlocks[0];

	uint64_t StringDataBlock = this->GetFileOffset(Asset, 0, CPUBLOCK.Offset);

	RpakStream->SetPosition(StringDataBlock + WrapHdr.CpuFullPathOffset);

	Info.Name = Reader.ReadCString();





	//Info.Name = string::Format("datatable_0x%llx", Asset.NameHash);
	Info.Type = ApexAssetType::Wrap;
	Info.Status = ApexAssetStatus::Loaded;
	//Info.Info = string::Format("Columns: %d Rows: %d", DtblHeader.ColumnCount, DtblHeader.RowCount);
}

void RpakLib::ExportWrap(const RpakLoadAsset& Asset, const string& Path)
{
	//RpakFile& File = this->LoadedFiles[Asset.FileIndex];

}