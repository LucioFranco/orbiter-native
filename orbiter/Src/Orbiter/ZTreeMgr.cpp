// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "ZTreeMgr.h"
#include "zlib.h"
#include <cstring>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
#include <filesystem>
#ifdef ORBITER_NATIVE_CORE
#include "Platform/Vfs.h"
#endif
namespace {
constexpr DWORD missing = DWORD(-1);
constexpr DWORD headerBytes = 48, nodeBytes = 32;
constexpr DWORD maxTileBytes = 256 * 1024 * 1024;
bool Read32(FILE *file, DWORD &value) {
    BYTE bytes[4]; if (::fread(bytes,1,4,file) != 4) return false;
    value = DWORD(bytes[0]) | DWORD(bytes[1])<<8 | DWORD(bytes[2])<<16 | DWORD(bytes[3])<<24;
    return true;
}
bool Read64(FILE *file, __int64 &value) {
    DWORD low, high; if (!Read32(file,low) || !Read32(file,high) || (high & 0x80000000u)) return false;
    value = (static_cast<__int64>(high)<<32) | low; return true;
}
bool Write32(FILE *file, DWORD value) {
    BYTE bytes[4]; for (unsigned i=0;i<4;++i) bytes[i]=BYTE(value>>(8*i));
    return ::fwrite(bytes,1,4,file)==4;
}
bool Write64(FILE *file, __int64 value) {
    return Write32(file,DWORD(value)) && Write32(file,DWORD(std::uint64_t(value)>>32));
}
}

// =======================================================================
// File header for compressed tree files

TreeFileHeader::TreeFileHeader ()
{
	magic[0] = 'T';
	magic[1] = 'X';
	magic[2] = 1;
	magic[3] = 0;
	size = headerBytes;
	flags = 0;
	nodeCount = 0;
	dataOfs = size;
	dataLength = 0;
	rootPos1 = rootPos2 = rootPos3 = rootPos4[0] = rootPos4[1] = (DWORD)-1;
}

// -----------------------------------------------------------------------

size_t TreeFileHeader::fwrite(FILE *f)
{
    return ::fwrite(magic,1,4,f)==4 && Write32(f,size) && Write32(f,flags) &&
        Write32(f,dataOfs) && Write64(f,dataLength) && Write32(f,nodeCount) &&
        Write32(f,rootPos1) && Write32(f,rootPos2) && Write32(f,rootPos3) &&
        Write32(f,rootPos4[0]) && Write32(f,rootPos4[1]);
}

bool TreeFileHeader::fread(FILE *f)
{
    BYTE id[4]; DWORD bytes;
    return ::fread(id,1,4,f)==4 && !std::memcmp(id,magic,4) &&
        Read32(f,bytes) && bytes==headerBytes && Read32(f,flags) &&
        Read32(f,dataOfs) && Read64(f,dataLength) && Read32(f,nodeCount) &&
        Read32(f,rootPos1) && Read32(f,rootPos2) && Read32(f,rootPos3) &&
        Read32(f,rootPos4[0]) && Read32(f,rootPos4[1]);
}

// =======================================================================
// Tree table of contents

TreeTOC::TreeTOC()
{
	ntree = 0;
	ntreebuf = 0;
	tree = NULL;
	totlength = 0;
}

// -----------------------------------------------------------------------

TreeTOC::~TreeTOC()
{
	if (ntreebuf) {
		delete[] tree;
		tree = NULL;
	}
}

// -----------------------------------------------------------------------

size_t TreeTOC::fread(DWORD count, FILE *f)
{
    if (!count) { delete[] tree; tree=nullptr; ntree=ntreebuf=0; return 0; }
    // On disk records are 32 bytes, including four padding bytes at the end.
    // Do not depend on the host compiler's structure packing or endianness.
    auto nodes = std::make_unique<TreeNode[]>(count);
    for (DWORD i=0;i<count;++i) {
        auto &node=nodes[i]; DWORD padding;
        if (!Read64(f,node.pos) || !Read32(f,node.size)) return 0;
        for (unsigned j=0;j<4;++j) if (!Read32(f,node.child[j])) return 0;
        if (!Read32(f,padding)) return 0;
    }
    delete[] tree; tree=nodes.release(); ntree=ntreebuf=count;
    return count;
}

// =======================================================================
// ZTreeMgr class: manage a single layer tree for a planet

ZTreeMgr *ZTreeMgr::CreateFromFile(const char *PlanetPath, Layer _layer)
{
	if (!PlanetPath || !*PlanetPath || _layer < LAYER_SURF || _layer > LAYER_CLOUD) return nullptr;
	ZTreeMgr *mgr = new ZTreeMgr(PlanetPath, _layer);
	if (!mgr->TOC().size()) {
		delete mgr;
		mgr = 0;
	}
	return mgr;
}

// -----------------------------------------------------------------------

ZTreeMgr::ZTreeMgr(const char *PlanetPath, Layer _layer)
: rootPos1(missing), rootPos2(missing), rootPos3(missing), rootPos4{missing,missing}, dofs(0)
{
	const char *root = PlanetPath ? PlanetPath : "";
	path = new char[strlen(root)+1];
	strcpy(path, root);
	layer = _layer;
	treef = 0;
	OpenArchive();
}

// -----------------------------------------------------------------------

ZTreeMgr::~ZTreeMgr()
{
	delete []path;
	path = NULL;
	if (treef) fclose(treef);
}

// -----------------------------------------------------------------------

bool ZTreeMgr::OpenArchive()
{
    if (!*path || layer < LAYER_SURF || layer > LAYER_CLOUD) return false;
    const char *name[] = {"Surf","Mask","Elev","Elev_mod","Label","Cloud"};
    std::filesystem::path filename = std::filesystem::path(path) / "Archive" / (std::string(name[layer])+".tree");
#ifdef ORBITER_NATIVE_CORE
    filename = orbiter::platform::ResolvePath(filename.string());
#endif
    treef = fopen(filename.string().c_str(),"rb");
    if (!treef) return false;
    auto fail = [&]() { fclose(treef); treef=nullptr; toc.ntree=0; return false; };
    std::error_code error;
    const auto fileBytes=std::filesystem::file_size(filename,error);
    TreeFileHeader header;
    if (error || fileBytes>std::uint64_t(std::numeric_limits<__int64>::max()) || !header.fread(treef) || (header.flags & ~DWORD(1)) || !header.nodeCount ||
        std::uint64_t(header.nodeCount)*nodeBytes+headerBytes > header.dataOfs ||
        header.dataOfs > fileBytes || std::uint64_t(header.dataLength) > fileBytes-header.dataOfs)
        return fail();
    if (toc.fread(header.nodeCount,treef) != header.nodeCount) return fail();
    toc.totlength=header.dataLength;
    const auto valid = [&](DWORD index) { return index==missing || index<toc.ntree; };
    if (!valid(header.rootPos1) || !valid(header.rootPos2) || !valid(header.rootPos3) ||
        !valid(header.rootPos4[0]) || !valid(header.rootPos4[1])) return fail();
    for (DWORD i=0;i<toc.ntree;++i) {
        const auto &node=toc.tree[i];
        const auto end=i+1<toc.ntree ? toc.tree[i+1].pos : toc.totlength;
        if (node.pos<0 || end<node.pos || end>toc.totlength ||
            std::uint64_t(end-node.pos)>maxTileBytes || node.size>maxTileBytes ||
            (node.size && end==node.pos)) return fail();
        for (DWORD child:node.child) if (!valid(child)) return fail();
    }
    rootPos1=header.rootPos1; rootPos2=header.rootPos2; rootPos3=header.rootPos3;
    rootPos4[0]=header.rootPos4[0]; rootPos4[1]=header.rootPos4[1];
    dofs=header.dataOfs; compressed=(header.flags & 1)!=0;
    return true;
}

// -----------------------------------------------------------------------

DWORD ZTreeMgr::Idx(int lvl, int ilat, int ilng)
{
    if (!treef || lvl<1 || lvl>30 || ilat<0 || ilng<0) return missing;
    if (lvl<4) {
        if (ilat || ilng) return missing;
        return lvl==1 ? rootPos1 : lvl==2 ? rootPos2 : rootPos3;
    }
    const unsigned rows=1u<<(lvl-4);
    if (unsigned(ilat)>=rows || unsigned(ilng)>=2*rows) return missing;
    DWORD index=rootPos4[unsigned(ilng)/rows];
    for (int bit=lvl-5; bit>=0 && index!=missing; --bit) {
        if (index>=toc.size()) return missing;
        const unsigned child=(((unsigned(ilat)>>bit)&1)<<1) | ((unsigned(ilng)>>bit)&1);
        index=toc[index].child[child];
    }
    return index;
}

DWORD ZTreeMgr::ReadData(DWORD idx, BYTE **outp)
{
    if (!outp) return 0;
    *outp=nullptr;
    if (!treef || idx>=toc.size()) return 0;
    const DWORD size=NodeSizeInflated(idx), packedSize=NodeSizeDeflated(idx);
    if (!size || !packedSize) return 0;
    std::vector<BYTE> packed(packedSize);
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (_fseeki64(treef,toc[idx].pos+dofs,SEEK_SET) ||
            ::fread(packed.data(),1,packedSize,treef)!=packedSize) return 0;
    }
    auto data=std::make_unique<BYTE[]>(size);
    if (compressed) {
        if (Inflate(packed.data(),packedSize,data.get(),size)!=size) return 0;
    } else {
        if (packedSize!=size) return 0;
        std::memcpy(data.get(),packed.data(),size);
    }
    *outp=data.release();
    return size;
}

// -----------------------------------------------------------------------

DWORD ZTreeMgr::Inflate(const BYTE *inp, DWORD ninp, BYTE *outp, DWORD noutp)
{
	uLongf ndata = noutp;
	if (uncompress (outp, &ndata, inp, ninp) != Z_OK)
		return 0;
	return ndata;
}

// -----------------------------------------------------------------------

void ZTreeMgr::ReleaseData(BYTE *data)
{
	delete []data;
	data = NULL;
}