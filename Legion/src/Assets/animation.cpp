#include "pch.h"
#include "RpakLib.h"
#include "Path.h"
#include "Directory.h"
#include <rtech.h>
#include <animtypes.h>

void RpakLib::BuildAnimInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	AnimRigHeader RigHeader{};
	RigHeader.ReadFromAssetStream(&RpakStream, Asset.AssetVersion);

	RpakStream->SetPosition(this->GetFileOffset(Asset, RigHeader.name.Index, RigHeader.name.Offset));

	string RigName = Reader.ReadCString();

	if (ExportManager::Config.GetBool("UseFullPaths"))
		Info.Name = RigName;
	else
		Info.Name = IO::Path::GetFileNameWithoutExtension(RigName).ToLower();

	Info.Type = ApexAssetType::AnimationSet;
	Info.Status = ApexAssetStatus::Loaded;

	RpakStream->SetPosition(this->GetFileOffset(Asset, RigHeader.studioData.Index, RigHeader.studioData.Offset));

	if (Asset.AssetVersion < 5)
	{
		studiohdr_t studiohdr = Reader.Read<studiohdr_t>();

		Info.Info = string::Format("Animations: %d, Bones: %d", RigHeader.animSeqCount, studiohdr.numbones);
	}
	else {
		studiohdr_t_v16 studiohdr = Reader.Read<studiohdr_t_v16>();

		Info.Info = string::Format("Animations: %d, Bones: %d", RigHeader.animSeqCount, studiohdr.numbones);
	}
}

void RpakLib::BuildRawAnimInfo(const RpakLoadAsset& Asset, ApexAsset& Info)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	ASeqHeader AnHeader = Reader.Read<ASeqHeader>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, AnHeader.pName.Index, AnHeader.pName.Offset));

	string AnimName = Reader.ReadCString();

	const uint64_t AnimationOffset = this->GetFileOffset(Asset, AnHeader.pAnimation.Index, AnHeader.pAnimation.Offset);

	RpakStream->SetPosition(AnimationOffset);

	string ActivityName = "";

	if (Asset.AssetVersion < 11)
	{
		mstudioseqdesc_t AnimSequenceHeader = Reader.Read<mstudioseqdesc_t>();

		RpakStream->SetPosition(AnimationOffset + AnimSequenceHeader.szactivitynameindex);

		ActivityName = Reader.ReadCString();
	}
	else {
		mstudioseqdesc_t_v16 AnimSequenceHeader = Reader.Read<mstudioseqdesc_t_v16>();

		RpakStream->SetPosition(AnimationOffset + AnimSequenceHeader.szactivitynameindex);

		ActivityName = Reader.ReadCString();
	}

	if (ExportManager::Config.GetBool("UseFullPaths"))
		Info.Name = AnimName;
	else
		Info.Name = IO::Path::GetFileNameWithoutExtension(AnimName).ToLower();

	Info.Type = ApexAssetType::AnimationSeq;
	Info.Status = ApexAssetStatus::Loaded;

	if (ActivityName != "")
		Info.Info = string::Format("%s", ActivityName.ToCString());
}

void RpakLib::ExportAnimationRig_V5(const RpakLoadAsset& Asset, const string& Path)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	AnimRigHeader RigHeader{};
	RigHeader.ReadFromAssetStream(&RpakStream, Asset.AssetVersion);

	RpakStream->SetPosition(this->GetFileOffset(Asset, RigHeader.name));

	string FullAnimSetName = Reader.ReadCString();
	string AnimSetName = IO::Path::GetFileNameWithoutExtension(FullAnimSetName);
	string AnimSetPath = IO::Path::Combine(Path, AnimSetName);

	IO::Directory::CreateDirectory(AnimSetPath);

	AnimExportFormat_t AnimFormat = (AnimExportFormat_t)ExportManager::Config.Get<System::SettingType::Integer>("AnimFormat");

	if (AnimFormat == AnimExportFormat_t::RAnim)
	{
		uint64_t SkeletonOffset = this->GetFileOffset(Asset, RigHeader.studioData);
		RpakStream->SetPosition(SkeletonOffset);

		studiohdr_t_v16 studiohdr = Reader.Read<studiohdr_t_v16>();

		RpakStream->SetPosition(SkeletonOffset);

		char* skelBuf = new char[studiohdr.bonedataindex + (sizeof(mstudiobonedata_t_v16) * studiohdr.numbones)];
		Reader.Read(skelBuf, 0, studiohdr.bonedataindex + (sizeof(mstudiobonedata_t_v16) * studiohdr.numbones));

		std::ofstream skelOut(IO::Path::Combine(AnimSetPath, AnimSetName + ".rrig"), std::ios::out | std::ios::binary);
		skelOut.write(skelBuf, studiohdr.bonedataindex + (sizeof(mstudiobonedata_t_v16) * studiohdr.numbones));
		skelOut.close();

		// ignore for now
		const uint64_t ReferenceOffset = this->GetFileOffset(Asset, RigHeader.animSeqs);

		for (uint32_t i = 0; i < RigHeader.animSeqCount; i++)
		{
			RpakStream->SetPosition(ReferenceOffset + ((uint64_t)i * 0x8));

			uint64_t AnimHash = Reader.Read<uint64_t>();
			if (Assets.ContainsKey(AnimHash))
			{
				auto SeqAsset = Assets[AnimHash];

				auto AnimStream = this->GetFileStream(SeqAsset);

				IO::BinaryReader AnimReader = IO::BinaryReader(AnimStream.get(), true);

				AnimStream->SetPosition(this->GetFileOffset(SeqAsset, SeqAsset.SubHeaderIndex, SeqAsset.SubHeaderOffset));

				ASeqHeaderV10 AnHeader = AnimReader.Read<ASeqHeaderV10>();

				AnimStream->SetPosition(this->GetFileOffset(SeqAsset, AnHeader.pName.Index, AnHeader.pName.Offset));

				string SeqName = AnimReader.ReadCString();
				string SeqSetName = IO::Path::GetFileNameWithoutExtension(SeqName);
				string SeqSetPath = IO::Path::Combine(Path, AnimSetName);

				g_Logger.Info("-> %s\n", SeqName.ToCString());

				const uint64_t AnimationOffset = this->GetFileOffset(SeqAsset, AnHeader.pAnimation.Index, AnHeader.pAnimation.Offset);

				this->ExportAnimationSeq(Assets[AnimHash], AnimSetPath);
				AnimStream.release();
			}

			continue;
		}

		return;
	}

	// version is 99 because it's supposed to only check model version, not arig version
	const List<Assets::Bone> Skeleton = this->ExtractSkeleton_V16(Reader, this->GetFileOffset(Asset, RigHeader.studioData), 99);

	const uint64_t ReferenceOffset = this->GetFileOffset(Asset, RigHeader.animSeqs);

	for (uint32_t i = 0; i < RigHeader.animSeqCount; i++)
	{
		RpakStream->SetPosition(ReferenceOffset + ((uint64_t)i * 0x8));

		uint64_t AnimHash = Reader.Read<uint64_t>();

		// excluded by DFS
		if (AnimHash == 0xdf5)
		{
			g_Logger.Warning("unable to export anim idx %i for '%s' because it is self-excluded\n", i, FullAnimSetName.ToCString());
			continue;
		}

		if (!Assets.ContainsKey(AnimHash))
		{
			g_Logger.Warning("missing anim 0x%llx for '%s'\n", AnimHash, FullAnimSetName.ToCString());
			continue;
		}

		// We need to make sure the skeleton is kept alive (copied) here...
		this->ExtractAnimation_V11(Assets[AnimHash], Skeleton, AnimSetPath);
	}

	if (AnimFormat == AnimExportFormat_t::SMD)
	{
		uint64_t SkeletonOffset = this->GetFileOffset(Asset, RigHeader.studioData);
		RpakStream->SetPosition(SkeletonOffset);
		studiohdr_t_v16 studiohdr = Reader.Read<studiohdr_t_v16>();
		RpakStream->SetPosition(SkeletonOffset);

		std::unique_ptr<char[]> studioBuf(new char[studiohdr.bonedataindex + (sizeof(mstudiobonedata_t_v16) * studiohdr.numbones)]);
		Reader.Read(studioBuf.get(), 0, studiohdr.bonedataindex + (sizeof(mstudiobonedata_t_v16) * studiohdr.numbones));

		auto Model = std::make_unique<Assets::Model>(0, 0);
		Model->Name = AnimSetName;
		Model->Bones = std::move(this->ExtractSkeleton_V16(Reader, this->GetFileOffset(Asset, RigHeader.studioData), 99));

		this->ExportQC(Asset, IO::Path::Combine(AnimSetPath, AnimSetName + ".qc"), FullAnimSetName, Model, studioBuf.get(), nullptr);
	}
}

void RpakLib::ExportAnimationRig(const RpakLoadAsset& Asset, const string& Path)
{
	if (Asset.AssetVersion >= 5)
	{
		this->ExportAnimationRig_V5(Asset, Path);
		return;
	}

	IO::Directory::CreateDirectory(Path);
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	AnimRigHeader RigHeader{};
	RigHeader.ReadFromAssetStream(&RpakStream, Asset.AssetVersion);

	RpakStream->SetPosition(this->GetFileOffset(Asset, RigHeader.name));

	string FullAnimSetName = Reader.ReadCString();
	string AnimSetName = IO::Path::GetFileNameWithoutExtension(FullAnimSetName);
	string AnimSetPath = IO::Path::Combine(Path, AnimSetName);

	IO::Directory::CreateDirectory(AnimSetPath);

	AnimExportFormat_t AnimFormat = (AnimExportFormat_t)ExportManager::Config.Get<System::SettingType::Integer>("AnimFormat");

	if (AnimFormat == AnimExportFormat_t::RAnim)
	{
		uint64_t SkeletonOffset = this->GetFileOffset(Asset, RigHeader.studioData);
		RpakStream->SetPosition(SkeletonOffset);

		studiohdr_t studiohdr = Reader.Read<studiohdr_t>();

		RpakStream->SetPosition(SkeletonOffset);

		char* skelBuf = new char[studiohdr.length];
		Reader.Read(skelBuf, 0, studiohdr.length);

		std::ofstream skelOut(IO::Path::Combine(AnimSetPath, AnimSetName + ".rrig"), std::ios::out | std::ios::binary);
		skelOut.write(skelBuf, studiohdr.length);
		skelOut.close();

		const uint64_t ReferenceOffset = this->GetFileOffset(Asset, RigHeader.animSeqs);

		for (uint32_t i = 0; i < RigHeader.animSeqCount; i++)
		{
			RpakStream->SetPosition(ReferenceOffset + ((uint64_t)i * 0x8));

			uint64_t AnimHash = Reader.Read<uint64_t>();
			if (Assets.ContainsKey(AnimHash))
			{
				auto SeqAsset = Assets[AnimHash];

				auto AnimStream = this->GetFileStream(SeqAsset);

				IO::BinaryReader AnimReader = IO::BinaryReader(AnimStream.get(), true);

				AnimStream->SetPosition(this->GetFileOffset(SeqAsset, SeqAsset.SubHeaderIndex, SeqAsset.SubHeaderOffset));

				ASeqHeader AnHeader = AnimReader.Read<ASeqHeader>();

				AnimStream->SetPosition(this->GetFileOffset(SeqAsset, AnHeader.pName.Index, AnHeader.pName.Offset));

				string SeqName = AnimReader.ReadCString();
				string SeqSetName = IO::Path::GetFileNameWithoutExtension(SeqName);
				string SeqSetPath = IO::Path::Combine(Path, AnimSetName);

				g_Logger.Info("-> %s\n", SeqName.ToCString());

				const uint64_t AnimationOffset = this->GetFileOffset(SeqAsset, AnHeader.pAnimation.Index, AnHeader.pAnimation.Offset);

				this->ExportAnimationSeq(Assets[AnimHash], AnimSetPath);

				AnimStream.release();
			}

			continue;
		}

		return;
	}

	// version is 99 because it's supposed to only check model version, not arig version
	const List<Assets::Bone> Skeleton = this->ExtractSkeleton(Reader, this->GetFileOffset(Asset, RigHeader.studioData), 99);

	const uint64_t ReferenceOffset = this->GetFileOffset(Asset, RigHeader.animSeqs);

	for (uint32_t i = 0; i < RigHeader.animSeqCount; i++)
	{
		RpakStream->SetPosition(ReferenceOffset + ((uint64_t)i * 0x8));

		uint64_t AnimHash = Reader.Read<uint64_t>();

		// excluded by DFS
		if (AnimHash == 0xdf5)
		{
			g_Logger.Warning("unable to export anim idx %i for '%s' because it is self-excluded\n", i, FullAnimSetName.ToCString());
			continue;
		}

		if (!Assets.ContainsKey(AnimHash))
		{
			g_Logger.Warning("missing anim 0x%llx for '%s'\n", AnimHash, FullAnimSetName.ToCString());
			continue;
		}

		// We need to make sure the skeleton is kept alive (copied) here...
		this->ExtractAnimation(Assets[AnimHash], Skeleton, AnimSetPath);
	}

	if (AnimFormat == AnimExportFormat_t::SMD)
	{
		uint64_t SkeletonOffset = this->GetFileOffset(Asset, RigHeader.studioData);
		RpakStream->SetPosition(SkeletonOffset);
		studiohdr_t studiohdr = Reader.Read<studiohdr_t>();
		RpakStream->SetPosition(SkeletonOffset);

		std::unique_ptr<char[]> studioBuf(new char[studiohdr.length]);
		Reader.Read(studioBuf.get(), 0, studiohdr.length);

		auto Model = std::make_unique<Assets::Model>(0, 0);
		Model->Name = AnimSetName;
		Model->Bones = std::move(this->ExtractSkeleton(Reader, this->GetFileOffset(Asset, RigHeader.studioData), 99));

		this->ExportQC(Asset, IO::Path::Combine(AnimSetPath, AnimSetName + ".qc"), FullAnimSetName, Model, studioBuf.get(), nullptr);
	}
}

string RpakLib::ExtractAnimationSeq(const RpakLoadAsset& Asset)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	ASeqHeader AnHeader = Reader.Read<ASeqHeader>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, AnHeader.pName.Index, AnHeader.pName.Offset));

	return Reader.ReadCString();
}

string RpakLib::ExtractAnimationRig(const RpakLoadAsset& Asset)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	AnimRigHeader RigHeader = Reader.Read<AnimRigHeader>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, RigHeader.name));

	return Reader.ReadCString();
}

void RpakLib::ExtractAnimation(const RpakLoadAsset& Asset, const List<Assets::Bone>& Skeleton, const string& Path)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	ASeqHeader animHeader = Reader.Read<ASeqHeader>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, animHeader.pName.Index, animHeader.pName.Offset));

	string animName = IO::Path::GetFileNameWithoutExtension(Reader.ReadCString());

	const uint64_t seqOffset = this->GetFileOffset(Asset, animHeader.pAnimation.Index, animHeader.pAnimation.Offset);

	RpakStream->SetPosition(seqOffset);

	mstudioseqdesc_t seqdesc = Reader.Read<mstudioseqdesc_t>();

	uint64_t ActualStarpakOffset = Asset.StarpakOffset & 0xFFFFFFFFFFFFFF00;
	uint64_t ActualOptStarpakOffset = Asset.OptimalStarpakOffset & 0xFFFFFFFFFFFFFF00;

	uint64_t starpakDataOffset = 0;
	std::unique_ptr<IO::FileStream> StarpakStream = nullptr;

	if (Asset.OptimalStarpakOffset != -1)
	{
		starpakDataOffset = ActualOptStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, true);
	}
	else if (Asset.StarpakOffset != -1)
	{
		starpakDataOffset = ActualStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, false);
	}

	IO::BinaryReader StarpakReader = IO::BinaryReader(StarpakStream.get(), true);

	AnimExportFormat_t AnimFormat = (AnimExportFormat_t)ExportManager::Config.Get<System::SettingType::Integer>("AnimFormat");

	for (uint32_t i = 0; i < seqdesc.numblends; i++)
	{
		RpakStream->SetPosition(seqOffset + seqdesc.animindexindex + ((uint64_t)i * sizeof(uint32_t)));

		int animindex = Reader.Read<int>();

		RpakStream->SetPosition(seqOffset + animindex);
		mstudioanimdescv54_t animdesc = Reader.Read<mstudioanimdescv54_t>();

		RpakStream->SetPosition(seqOffset + animindex + animdesc.sznameindex);
		string szanimname = Reader.ReadCString();

		// unsure what this flag is
		if (!(animdesc.flags & 0x20000))
			continue;

		Assets::AnimationCurveMode AnimCurveType = Assets::AnimationCurveMode::Absolute;

		// anim is delta
		if (animdesc.flags & STUDIO_DELTA)
			AnimCurveType = Assets::AnimationCurveMode::Additive;

		std::unique_ptr<Assets::Animation> Anim = std::make_unique<Assets::Animation>(Skeleton.Count());

		for (auto& Bone : Skeleton)
		{
			Anim->Bones.EmplaceBack(Bone.Name(), Bone.Parent(), Bone.LocalPosition(), Bone.LocalRotation());

			List<Assets::Curve>& CurveNodes = Anim->GetNodeCurves(Bone.Name());

			// Inject curve nodes here, we can use the purge empty later to remove them
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::RotateQuaternion, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateX, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateY, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateZ, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleX, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleY, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleZ, AnimCurveType);
		}

		const uint64_t AnimHeaderPointer = seqOffset + animindex;

		for (uint32_t Frame = 0; Frame < animdesc.numframes; Frame++)
		{
			uint32_t ChunkTableIndex = 0;
			uint32_t ChunkFrame = Frame;
			uint32_t FrameCountOneLess = 0;
			uint32_t FirstChunk = animdesc.animindex;
			uint64_t ChunkDataOffset = 0;
			uint32_t IsExternal = 0;
			uint64_t ResultDataPtr = 0;

			if (!animdesc.mediancount)
			{
				// Nothing here
				goto nomedian;
			}
			else if (ChunkFrame >= animdesc.sectionframes)
			{
				uint32_t FrameCount = animdesc.numframes;
				uint32_t ChunkFrameMinusSplitCount = ChunkFrame - animdesc.sectionframes;
				if (FrameCount <= animdesc.sectionframes || ChunkFrame != FrameCount - 1)
				{
					ChunkTableIndex = ChunkFrameMinusSplitCount / animdesc.mediancount + 1;
					ChunkFrame = ChunkFrame - (animdesc.mediancount * (ChunkFrameMinusSplitCount / animdesc.mediancount)) - animdesc.sectionframes;
				}
				else
				{
					ChunkFrame = 0;
					ChunkTableIndex = (FrameCount - animdesc.sectionframes - 1) / animdesc.mediancount + 2;
				}
			}

			ChunkDataOffset = animdesc.sectionindex + 8 * (uint64_t)ChunkTableIndex;

			RpakStream->SetPosition(AnimHeaderPointer + ChunkDataOffset);
			FirstChunk = Reader.Read<uint32_t>();
			IsExternal = Reader.Read<uint32_t>();

			if (IsExternal)
			{
				uint64_t v13 = animdesc.somedataoffset;
				if (v13)
				{
					ResultDataPtr = v13 + FirstChunk;
				}
				else
				{
					ResultDataPtr = starpakDataOffset + FirstChunk;
				}
			}
			else
			{
			nomedian:
				ResultDataPtr = AnimHeaderPointer + FirstChunk;
			}

			char boneFlagData[256]{};

			if (IsExternal)
			{
				StarpakStream->SetPosition(ResultDataPtr);
				StarpakStream->Read((uint8_t*)boneFlagData, 0, ((4 * (uint64_t)Skeleton.Count() + 7) / 8 + 1) & 0xFFFFFFFFFFFFFFFE);
			}
			else
			{
				RpakStream->SetPosition(ResultDataPtr);
				RpakStream->Read((uint8_t*)boneFlagData, 0, ((4 * (uint64_t)Skeleton.Count() + 7) / 8 + 1) & 0xFFFFFFFFFFFFFFFE);
			}

			IO::BinaryReader* currentReader = IsExternal ? &StarpakReader : &Reader;

			for (uint32_t b = 0; b < Skeleton.Count(); b++)
			{
				uint32_t Shift = 4 * (b % 2);
				char boneFlags = boneFlagData[b / 2] >> Shift;

				if (boneFlags & 0x7)
				{
					mstudio_rle_anim_t anim = currentReader->Read<mstudio_rle_anim_t>();

					short sizeToRead = anim.size > 0 ? anim.size - sizeof(mstudio_rle_anim_t) : 0;

					std::unique_ptr<uint8_t[]> boneTrackData = currentReader->Read(sizeToRead);

					uint16_t* BoneTrackDataPtr = (uint16_t*)boneTrackData.get();

					// this flag does not exist
					anim.bAdditiveCustom = (AnimCurveType == Assets::AnimationCurveMode::Additive);

					if (boneFlags & STUDIO_ANIM_BONEPOS)
						CalcBonePosition(anim, &BoneTrackDataPtr, Anim, b, ChunkFrame, Frame);
					if (boneFlags & STUDIO_ANIM_BONEROT)
						CalcBoneQuaternion(anim, &BoneTrackDataPtr, Anim, b, ChunkFrame, Frame);
					if (boneFlags & STUDIO_ANIM_BONESCALE)
						CalcBoneScale(anim, &BoneTrackDataPtr, Anim, b, ChunkFrame, Frame);
				}
			}
		}

		Anim->RemoveEmptyNodes();

		string DestinationPath = IO::Path::Combine(Path, animName + string::Format("_%d", i) + (const char*)this->AnimExporter->AnimationExtension());

		if (AnimFormat == AnimExportFormat_t::SMD)
		{
			DestinationPath = string::Format("%s\\Anims\\%s\\%s.smd", Path.ToCString(), animName.ToCString(), szanimname.ToCString());

			IO::Directory::CreateDirectory(IO::Path::GetDirectoryName(DestinationPath));
		}

		try
		{
			if (Utils::ShouldWriteFile(DestinationPath))
				this->AnimExporter->ExportAnimation(*Anim.get(), DestinationPath);
		}
		catch (...)
		{
		}
	}
}

void RpakLib::ExtractAnimation_V11(const RpakLoadAsset& Asset, const List<Assets::Bone>& Skeleton, const string& Path)
{
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	ASeqHeaderV10 animHeader = Reader.Read<ASeqHeaderV10>();

	RpakStream->SetPosition(this->GetFileOffset(Asset, animHeader.pName.Index, animHeader.pName.Offset));

	string animName = IO::Path::GetFileNameWithoutExtension(Reader.ReadCString());

	const uint64_t seqOffset = this->GetFileOffset(Asset, animHeader.pAnimation.Index, animHeader.pAnimation.Offset);

	RpakStream->SetPosition(seqOffset);

	mstudioseqdesc_t_v16 seqdesc = Reader.Read<mstudioseqdesc_t_v16>();

	uint64_t ActualStarpakOffset = Asset.StarpakOffset & 0xFFFFFFFFFFFFFF00;
	uint64_t ActualOptStarpakOffset = Asset.OptimalStarpakOffset & 0xFFFFFFFFFFFFFF00;

	uint64_t starpakDataOffset = 0;
	std::unique_ptr<IO::FileStream> StarpakStream = nullptr;

	if (Asset.OptimalStarpakOffset != -1)
	{
		starpakDataOffset = ActualOptStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, true);
	}
	else if (Asset.StarpakOffset != -1)
	{
		starpakDataOffset = ActualStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, false);
	}

	IO::BinaryReader StarpakReader = IO::BinaryReader(StarpakStream.get(), true);

	AnimExportFormat_t AnimFormat = (AnimExportFormat_t)ExportManager::Config.Get<System::SettingType::Integer>("AnimFormat");

	for (uint32_t i = 0; i < seqdesc.numblends; i++)
	{
		if (AnimFormat == AnimExportFormat_t::SMD && i > 1)
			continue;

		// sizeof(VAR) needs to match animindex!!!!!!
		RpakStream->SetPosition(seqOffset + seqdesc.animindexindex + ((uint64_t)i * sizeof(short)));

		uint16 animindex = Reader.Read<uint16>();

		RpakStream->SetPosition(seqOffset + animindex);

		mstudioanimdescv54_t_v16 animdesc = Reader.Read<mstudioanimdescv54_t_v16>(); // lower case because normal source is like this :)

		RpakStream->SetPosition(seqOffset + animindex + FIX_OFFSET(animdesc.sznameindex));
		string szanimname = Reader.ReadCString();

		// unsure what this flag is
		if (!(animdesc.flags & 0x20000))
			continue;

		std::unique_ptr<Assets::Animation> Anim = std::make_unique<Assets::Animation>(Skeleton.Count());

		Assets::AnimationCurveMode AnimCurveType = Assets::AnimationCurveMode::Absolute;

		// anim is delta
		if (animdesc.flags & STUDIO_DELTA)
			AnimCurveType = Assets::AnimationCurveMode::Additive;

		for (auto& Bone : Skeleton)
		{
			Anim->Bones.EmplaceBack(Bone.Name(), Bone.Parent(), Bone.LocalPosition(), Bone.LocalRotation());

			List<Assets::Curve>& CurveNodes = Anim->GetNodeCurves(Bone.Name());

			// Inject curve nodes here, we can use the purge empty later to remove them
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::RotateQuaternion, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateX, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateY, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::TranslateZ, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleX, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleY, AnimCurveType);
			CurveNodes.EmplaceBack(Bone.Name(), Assets::CurveProperty::ScaleZ, AnimCurveType);
		}

		const uint64_t animDescPtr = seqOffset + animindex;

		for (uint32_t frameIdx = 0; frameIdx < animdesc.numframes; frameIdx++)
		{
			int sectionIdx = 0; // the index of the section we are in
			short sectionFrameIdx = frameIdx; // frame index for sections
			int sectionOffset = 0; // offset into current section
			uint64_t ResultDataPtr = 0; // ptr to the data
			int AnimIndex = animdesc.animindex; // offset to animation or first section if section animation
			bool IsExternal = false; // if this is a section, is the section outside of the actual sequence

			if (!animdesc.sectionframes)
			{
				// Nothing here
				goto nomedian;
			}
			else if (sectionFrameIdx >= animdesc.sectionstaticframes)
			{
				uint32_t sectionFrameMinusSplitCount = sectionFrameIdx - animdesc.sectionstaticframes; // I don't really know what unk2 is for but porter uses it so *shrug*
				if (animdesc.numframes <= animdesc.sectionstaticframes || sectionFrameIdx != animdesc.numframes - 1)
				{
					sectionIdx = sectionFrameMinusSplitCount / animdesc.sectionframes + 1;
					sectionFrameIdx = sectionFrameIdx - (animdesc.sectionframes * (sectionFrameMinusSplitCount / animdesc.sectionframes)) - animdesc.sectionstaticframes;
				}
				else
				{
					sectionFrameIdx = 0;
					sectionIdx = (animdesc.numframes - animdesc.sectionstaticframes - 1) / animdesc.sectionframes + 2;
				}
			}

			// Make sure sizeof(VAR) is right datatype!!!
			sectionOffset = animdesc.sectionindex + sizeof(int) * sectionIdx;

			RpakStream->SetPosition(animDescPtr + sectionOffset);
			AnimIndex = Reader.Read<int>();

			if (AnimIndex < 0 && StarpakStream)
			{
				AnimIndex = abs(AnimIndex) - 1;

				ResultDataPtr = starpakDataOffset + AnimIndex;
				IsExternal = true;
			}
			else
			{
			nomedian:
				IsExternal = false;
				ResultDataPtr = animDescPtr + AnimIndex;
			}

			// I have a really bad feeling we will see > 256 bones sooner than later
			char boneFlagData[256]{};

			if (IsExternal)
			{
				StarpakStream->SetPosition(ResultDataPtr);
				StarpakStream->Read((uint8_t*)boneFlagData, 0, ((4 * (uint64_t)Skeleton.Count() + 7) / 8 + 1) & 0xFFFFFFFFFFFFFFFE);
			}
			else
			{
				RpakStream->SetPosition(ResultDataPtr);
				RpakStream->Read((uint8_t*)boneFlagData, 0, ((4 * (uint64_t)Skeleton.Count() + 7) / 8 + 1) & 0xFFFFFFFFFFFFFFFE);
			}

			IO::BinaryReader* currentReader = IsExternal ? &StarpakReader : &Reader;

			for (uint32_t b = 0; b < Skeleton.Count(); b++)
			{
				uint32_t Shift = 4 * (b % 2);
				char BoneTrackFlags = boneFlagData[b / 2] >> Shift;

				if (BoneTrackFlags & (STUDIO_ANIM_BONEPOS | STUDIO_ANIM_BONEROT | STUDIO_ANIM_BONESCALE))
				{
					mstudio_rle_anim_t anim = currentReader->Read<mstudio_rle_anim_t>();

					short sizeToRead = anim.size > 0 ? anim.size - sizeof(mstudio_rle_anim_t) : 0;

					std::unique_ptr<uint8_t[]> boneTrackData = currentReader->Read(sizeToRead);

					uint16_t* BoneTrackDataPtr = (uint16_t*)boneTrackData.get();

					// Set this so when we do translations we will know whether or not to add the rest position onto it...
					anim.bAdditiveCustom = (AnimCurveType == Assets::AnimationCurveMode::Additive);

					if (BoneTrackFlags & STUDIO_ANIM_BONEPOS)
						CalcBonePosition(anim, &BoneTrackDataPtr, Anim, b, sectionFrameIdx, frameIdx);
					if (BoneTrackFlags & STUDIO_ANIM_BONEROT)
						CalcBoneQuaternion(anim, &BoneTrackDataPtr, Anim, b, sectionFrameIdx, frameIdx);
					if (BoneTrackFlags & STUDIO_ANIM_BONESCALE)
						CalcBoneScale(anim, &BoneTrackDataPtr, Anim, b, sectionFrameIdx, frameIdx);
				}
			}
		}

		Anim->RemoveEmptyNodes();

		string DestinationPath = IO::Path::Combine(Path, animName + string::Format("_%d", i) + (const char*)this->AnimExporter->AnimationExtension());

		// no animation blends for smd
		if (AnimFormat == AnimExportFormat_t::SMD)
		{
			DestinationPath = string::Format("%s\\Anims\\%s\\%s.smd", Path.ToCString(), animName.ToCString(), szanimname.ToCString());

			IO::Directory::CreateDirectory(IO::Path::GetDirectoryName(DestinationPath));
		}

		try
		{
			if (Utils::ShouldWriteFile(DestinationPath))
				this->AnimExporter->ExportAnimation(*Anim.get(), DestinationPath);
		}
		catch (...)
		{
		}
	}
}

void RpakLib::ExportAnimationSeq(const RpakLoadAsset& Asset, const string& Path)
{
	IO::Directory::CreateDirectory(Path);
	auto RpakStream = this->GetFileStream(Asset);
	IO::BinaryReader Reader = IO::BinaryReader(RpakStream.get(), true);

	AnimExportFormat_t AnimFormat = (AnimExportFormat_t)ExportManager::Config.Get<System::SettingType::Integer>("AnimFormat");
	if (AnimFormat != AnimExportFormat_t::RAnim)
		return;

	RpakStream->SetPosition(this->GetFileOffset(Asset, Asset.SubHeaderIndex, Asset.SubHeaderOffset));

	ASeqHeaderV10 AnHeader{};
	switch (Asset.SubHeaderSize)
	{
	case 0x30: // 7
		AnHeader = Reader.Read<ASeqHeader>().Upgrade();
		break;
	case 0x38: // 7.1
		AnHeader = Reader.Read<ASeqHeaderV71>().Upgrade();
		break;
	case 0x40: // 10
		AnHeader = Reader.Read<ASeqHeaderV10>();
		break;
	}

	RpakStream->SetPosition(this->GetFileOffset(Asset, AnHeader.pName.Index, AnHeader.pName.Offset));

	string FullAnimSetName = Reader.ReadCString();
	string AnimSetPath = IO::Path::Combine(Path, FullAnimSetName);
	string FolderPath = IO::Path::GetDirectoryName(AnimSetPath);

	IO::Directory::CreateDirectory(FolderPath);

	const uint64_t AnimationOffset = this->GetFileOffset(Asset, AnHeader.pAnimation.Index, AnHeader.pAnimation.Offset);

	RpakStream->SetPosition(AnimationOffset);

	mstudioseqdesc_t_v16 seqdesc{};
	if (Asset.AssetVersion < 11)
		seqdesc = Reader.Read<mstudioseqdesc_t>().Upgrade();
	else
		seqdesc = Reader.Read<mstudioseqdesc_t_v16>();

	RpakStream->SetPosition(AnimationOffset);

	uint64_t ActualStarpakOffset = Asset.StarpakOffset & 0xFFFFFFFFFFFFFF00;
	uint64_t ActualOptStarpakOffset = Asset.OptimalStarpakOffset & 0xFFFFFFFFFFFFFF00;

	uint64_t starpakDataOffset = 0;
	std::unique_ptr<IO::FileStream> StarpakStream = nullptr;

	if (Asset.OptimalStarpakOffset != -1)
	{
		starpakDataOffset = ActualOptStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, true);
	}
	else if (Asset.StarpakOffset != -1)
	{
		starpakDataOffset = ActualStarpakOffset;
		StarpakStream = this->GetStarpakStream(Asset, false);
	}

	char* stringreadBuf = new char[3];

	uint64_t ReadValue = 0;
	while (true)
	{
		RpakStream->SetPosition(AnimationOffset + FIX_OFFSET(seqdesc.szlabelindex) + ReadValue);

		Reader.Read(stringreadBuf, 0, 3);

		if (stringreadBuf[0] == 0 && stringreadBuf[1] == 0 && stringreadBuf[2] == 0)
			break;

		ReadValue++;
	};

	delete[] stringreadBuf;

	if (Asset.AssetVersion == 11)
	{
		uint64_t oldpos = RpakStream->GetPosition();
		uint64_t newpos = 0;

		const int numbones = (seqdesc.activitymodifierindex - seqdesc.weightlistindex) / 4;

		const int blendcount = seqdesc.groupsize[0] * seqdesc.groupsize[1];
		std::vector<short> AnimIndexes(blendcount);

		for (int i = 0; i < blendcount; i++)
		{
			RpakStream->SetPosition(AnimationOffset + seqdesc.animindexindex + (i * sizeof(short)));
			AnimIndexes[i] = Reader.Read<short>();
		}

		for (int i = 0; i < blendcount; i++)
		{
			uint32_t index = AnimationOffset + AnimIndexes[i];
			RpakStream->SetPosition(index);
			mstudioanimdescv54_t_v16 animdesc = Reader.Read<mstudioanimdescv54_t_v16>();

			if (animdesc.flags & STUDIO_ALLZEROS)
				continue;

			int flagSize = ((4 * numbones + 7) / 8 + 1) & 0xFFFFFFFE;



			if (animdesc.sectionindex)
			{
				const uint64_t sectionoffset = index + animdesc.sectionindex;
				const int sectionlength = ceil(((float)animdesc.numframes - 1) / (float)animdesc.sectionframes) + 1;
				RpakStream->SetPosition(sectionoffset);

				std::vector<int> sectionindexes(sectionlength);
				{
					for (int j = 0; j < sectionlength; j++)
						sectionindexes[j] = Reader.Read<int>();
				}

				

				for (int o = 0; o < sectionlength; o++)
				{
					int animationSize = 0;

					if (animdesc.flags & 0x20000)
					{
						RpakStream->SetPosition(index + sectionindexes[o] + flagSize + animationSize);

						for (int i = 0; i < flagSize; i++)
						{
							mstudio_rle_anim_t pOldRleAnim = Reader.Read<mstudio_rle_anim_t>();

							animationSize += pOldRleAnim.size;
						}

						newpos = RpakStream->GetPosition();
					}

				}
			}
			else
			{
				RpakStream->SetPosition(index + animdesc.animindex);

				std::vector<uint8_t> Flags(flagSize);
				{
					for (int i = 0; i < flagSize; i++)
						Flags[i] = Reader.Read<uint8_t>();
				}

				RpakStream->SetPosition(index + animdesc.animindex + flagSize);

				uint64_t animationSize = 0;

				for (int i = 0; i < flagSize; i++)
				{
					if (Flags[i] != 0)
					{
						mstudio_rle_anim_t pOldRleAnim = Reader.Read<mstudio_rle_anim_t>();
						animationSize += pOldRleAnim.size;
					}
				}

				RpakStream->SetPosition(index + animdesc.animindex + flagSize + animationSize);
			}

			if (animdesc.framemovementindex)
			{
				RpakStream->SetPosition(index + animdesc.framemovementindex);
				mstudioframemovement_t_v54 framemov = Reader.Read<mstudioframemovement_t_v54>();

				const int numsections = ceil((float)animdesc.numframes / (float)framemov.sectionframes);

				std::vector<short> sectionindexes(numsections);
				{
					for (int j = 0; j < numsections; j++)
						sectionindexes[j] = Reader.Read<short>();
				}

				for (int j = 0; j < numsections; j++)
				{
					RpakStream->SetPosition(index + animdesc.framemovementindex + sectionindexes[j]);
					uint64_t offset = RpakStream->GetPosition();

					std::vector<short> offsets(4);
					{
						for (int j = 0; j < 4; j++)
							offsets[j] = Reader.Read<short>();
					}

					for (int g = 0; g < 4; g++)
					{
						if (offsets[g])
						{
							RpakStream->SetPosition(offset + (g * sizeof(short)) + offsets[g]);
							//mstudioanimvalue_t an imvalue = Reader.Read<mstudioanimvalue_t>();
						}
					}
				}

			}



			if (newpos > oldpos)
				RpakStream->SetPosition(newpos);
		}


	}

	uint64_t RSeqSize = RpakStream->GetPosition() - AnimationOffset;

	RpakStream->SetPosition(AnimationOffset);
	char* rseqBuf = new char[RSeqSize];
	Reader.Read(rseqBuf, 0, RSeqSize);

	std::ofstream rseqOut(AnimSetPath, std::ios::out | std::ios::binary);
	rseqOut.write(rseqBuf, RSeqSize);
	rseqOut.close();

	// WIP EXTERNAL DATA
	if (AnHeader.pExternalData.Index || AnHeader.externalDataSize)
	{
		char* externalBuf = new char[AnHeader.externalDataSize];

		if (StarpakStream != nullptr)
		{
			StarpakStream->SetPosition(starpakDataOffset);
			StarpakStream->Read((uint8_t*)externalBuf, 0, AnHeader.externalDataSize);
		}
		else
		{
			RpakStream->SetPosition(this->GetFileOffset(Asset, AnHeader.pExternalData));
			RpakStream->Read((uint8_t*)externalBuf, 0, AnHeader.externalDataSize);
		}

		std::ofstream externalOut(IO::Path::ChangeExtension(AnimSetPath, ".rseq_ext"), std::ios::out | std::ios::binary);
		externalOut.write(externalBuf, AnHeader.externalDataSize);
		externalOut.close();

		delete[] externalBuf;
	}
}