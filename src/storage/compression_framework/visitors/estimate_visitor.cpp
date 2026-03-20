#include "estimate_visitor.hpp"

// u32 EstimateStringDictionary(StringData data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     auto result = DictionaryStringEncodedRes{
//         .codes = arena->Alloc<u32>(data.nitems),
//         .indexes = arena->Alloc<u32>(data.nitems + 1),
//         .stringBuf = arena->Alloc<u8>(data.totalLen),
//         .totalStrLen = 0,
//         .strCount = 0};
//     DictionaryStringEncoder::Encode(&result, data.src, data.lenSrc, data.nullmap, data.nitems);

//     auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

//     u32 val1 = EstimateNext(codes, arena, queue);

//     u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
//     u32 *lens = arena->Alloc<u32>(result.strCount);
//     TransformStrings(result, strPtrs, lens);
//     auto strings = StringData(strPtrs, lens, result.totalStrLen, result.strCount, data.nullmap, data.depth);

//     u32 val2 = EstimateString(strings, arena, queue);

//     return val1 + val2;
// }

// u32 EstimateFsst(StringData data, SlabArena *arena)
// {
//     size_t *lens64 = arena->Alloc<size_t>(data.nitems); // TODO this is a hack
//     for (u32 i = 0; i < data.nitems; i++)
//         lens64[i] = data.lenSrc[i];

//     auto encoder = fsst_create(data.nitems, lens64, (const u8 **)data.src, 0);

//     u32 outSize = 7 + 4 * data.totalLen;
//     auto strBuffer = arena->Alloc<u8>(outSize);
//     auto strLens = arena->Alloc<size_t>(outSize);
//     auto strings = arena->Alloc<u8 *>(outSize);
//     auto src = (const u8 **)data.src;

//     u32 nstrings = fsst_compress(encoder, data.nitems, lens64, src, outSize, strBuffer, strLens, strings);
//     if (nstrings != data.nitems)
//     {
//         throw std::runtime_error("fsst failed");
//     }

//     u32 totalSize = 0;
//     for (u32 i = 0; i < nstrings; i++)
//         totalSize += strLens[i];

//     return totalSize;
// }

// u32 EstimateString(StringData data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     if (data.depth > MAX_COMPRESSION_DEPTH)
//     {
//         return data.totalLen;
//     }

//     data.depth++;

//     u32 fsst = EstimateFsst(data, arena);
//     u32 dict = EstimateStringDictionary(data, arena, queue);
//     u32 raw = data.totalLen;

//     u32 best = std::min({fsst, dict, raw * UNCOMPRESSED_FAVOR / 100});

//     if (best == fsst)
//         queue->Push(SchemeAlgorithm::Fsst);
//     else if (best == dict)
//         queue->Push(SchemeAlgorithm::Dictionary);
//     else
//         queue->Push(SchemeAlgorithm::Uncompressed);

//     return best;
// }