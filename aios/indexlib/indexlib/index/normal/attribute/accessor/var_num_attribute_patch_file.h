#ifndef __INDEXLIB_VAR_NUM_ATTRIBUTE_PATCH_FILE_H
#define __INDEXLIB_VAR_NUM_ATTRIBUTE_PATCH_FILE_H

#include <queue>
#include <tr1/memory>
#include <autil/MultiValueType.h>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include "indexlib/misc/exception.h"
#include "indexlib/file_system/directory.h"
#include "indexlib/file_system/file_reader.h"
#include "indexlib/common/field_format/attribute/var_num_attribute_formatter.h"
#include "indexlib/config/attribute_config.h"

IE_NAMESPACE_BEGIN(index);

class VarNumAttributePatchFile
{
public:
    VarNumAttributePatchFile(segmentid_t segmentId,
                             const config::AttributeConfigPtr& attrConfig);
    
    ~VarNumAttributePatchFile();

public:
    //TODO: delete
    void Open(const std::string &fileName);

public:
    void Open(const file_system::DirectoryPtr& directory, 
              const std::string& fileName);
    bool HasNext();
    void Next();
    docid_t GetCurDocId() const { return mDocId; }
    segmentid_t GetSegmentId() const { return mSegmentId; }
    
    template <typename T>
    size_t GetPatchValue(uint8_t *value, size_t maxLen);

    template <typename T> 
    void SkipCurDocValue();

    uint32_t GetPatchItemCount() const { return mPatchItemCount; }
    uint32_t GetMaxPatchItemLen() const { return mMaxPatchLen; }
    int64_t GetFileLength() const { return mFileLength; }

private:
    void ReadAndMove(size_t length, uint8_t *&buffPtr, size_t &leftBuffsize);
    uint32_t ReadCount(uint8_t* buffPtr, size_t maxLen, size_t &encodeCountLen);
    void InitPatchFileReader(
            const file_system::DirectoryPtr& directory,
            const std::string& fileName);

    size_t GetPatchDataLen(uint32_t unitSize, uint32_t itemCount)
    {
        return mFixedValueCount == -1 ? (unitSize * itemCount) : mPatchDataLen;
    }
    bool ReachEnd() const;

protected:
    file_system::FileReaderPtr mFile;
    int64_t mFileLength;
    int64_t mCursor;
    docid_t mDocId;
    segmentid_t mSegmentId;
    uint32_t mPatchItemCount;
    uint32_t mMaxPatchLen;
    size_t mPatchDataLen;
    int32_t mFixedValueCount;
    bool mPatchCompressed;
    
private:
    friend class VarNumAttributePatchFileTest;
    IE_LOG_DECLARE();
};

///////////////////////////////////////////////////////
inline bool VarNumAttributePatchFile::HasNext()
{
    // uint32_t * 2 (tail): patchCount & maxPatchLen
    return mCursor < mFileLength - 8;
}

inline bool VarNumAttributePatchFile::ReachEnd() const { return mCursor >= mFileLength - 8; }

inline void VarNumAttributePatchFile::Next()
{
    if (mFile->Read((void*)(&mDocId), sizeof(docid_t), 
                    mCursor) < (ssize_t)sizeof(docid_t))
    {
        INDEXLIB_FATAL_ERROR(IndexCollapsed,
                             "Read patch file failed, file path is: %s", 
                             mFile->GetPath().c_str());
    }

    mCursor += sizeof(docid_t);

    if (unlikely(ReachEnd()))
    {
        INDEXLIB_FATAL_ERROR(OutOfRange, "reach end of patch file!");
    }
}

template <typename T>
inline size_t VarNumAttributePatchFile::GetPatchValue(
        uint8_t *value, size_t maxLen)
{
    if (unlikely(ReachEnd()))
    {
        INDEXLIB_FATAL_ERROR(OutOfRange, "reach end of patch file!");
    }    
    uint8_t* valuePtr = value;
    size_t encodeCountLen = 0;
    uint32_t recordNum = ReadCount(valuePtr, maxLen, encodeCountLen);
    valuePtr += encodeCountLen;
    maxLen -= encodeCountLen;

    size_t length = GetPatchDataLen(sizeof(T), recordNum);
    ReadAndMove(length, valuePtr, maxLen);

    assert((int32_t)(valuePtr - value) == (int32_t)(encodeCountLen + length));
    return valuePtr - value;
}

template <>
inline size_t VarNumAttributePatchFile::GetPatchValue<autil::MultiChar>(
        uint8_t *value, size_t maxLen)
{
    if (unlikely(ReachEnd()))
    {
        INDEXLIB_FATAL_ERROR(OutOfRange, "reach end of patch file!");
    }
    uint8_t* valuePtr = value;
    size_t encodeCountLen = 0;
    uint32_t recordNum = ReadCount(valuePtr, maxLen, encodeCountLen);
    valuePtr += encodeCountLen;
    maxLen -= encodeCountLen;

    if (recordNum > 0)
    {
        // read offsetLen
        uint8_t* offsetLenPtr = valuePtr;
        ReadAndMove(sizeof(uint8_t), valuePtr, maxLen);
        uint8_t offsetLen = *offsetLenPtr;

        // read offsets
        uint8_t* offsetAddr = valuePtr;
        ReadAndMove(offsetLen * recordNum, valuePtr, maxLen);

        // read items except last one
        uint32_t lastOffset = common::VarNumAttributeFormatter::GetOffset(
                (const char*)offsetAddr, offsetLen, recordNum - 1);
        ReadAndMove(lastOffset, valuePtr, maxLen);

        // read last item
        uint32_t lastItemLen = ReadCount(valuePtr, maxLen, encodeCountLen);
        valuePtr += encodeCountLen;
        maxLen -= encodeCountLen;
        ReadAndMove(lastItemLen, valuePtr, maxLen);
    }
    return valuePtr - value;
}

template <typename T>
inline void VarNumAttributePatchFile::SkipCurDocValue()
{
    if (!HasNext())
    {
        INDEXLIB_FATAL_ERROR(OutOfRange, "reach end of patch file!");
    }

    uint8_t encodeCountBuf[4];
    size_t encodeCountLen = 0;
    uint32_t recordNum = ReadCount(encodeCountBuf, 4, encodeCountLen);
    size_t length = GetPatchDataLen(sizeof(T), recordNum);
    mCursor += length;
}

template <>
inline void VarNumAttributePatchFile::SkipCurDocValue<autil::MultiChar>()
{
    if (!HasNext())
    {
        INDEXLIB_FATAL_ERROR(OutOfRange, "reach end of patch file!");
    }

    uint8_t buffer[4];
    size_t encodeCountLen = 0;
    uint32_t recordNum = ReadCount(buffer, 4, encodeCountLen);

    if (recordNum > 0)
    {
        uint8_t* bufferPtr = buffer;
        size_t readLen = 1;
        ReadAndMove(readLen, bufferPtr, readLen);

        uint8_t offsetLen = buffer[0];
        mCursor += (recordNum - 1) * offsetLen;

        bufferPtr = buffer;
        readLen = offsetLen;
        ReadAndMove(readLen, bufferPtr, readLen);
        
        uint32_t lastOffset = common::VarNumAttributeFormatter::GetOffset(
                (const char*)buffer, offsetLen, 0);
        mCursor += lastOffset;

        uint32_t lastItemLen = ReadCount(buffer, 4, encodeCountLen);
        mCursor += lastItemLen;
    }
}

inline void VarNumAttributePatchFile::ReadAndMove(size_t length,
        uint8_t *&buffPtr, size_t &leftBuffSize)
{
    if (length > leftBuffSize)
    {
        INDEXLIB_FATAL_ERROR(BufferOverflow,
                             "Exceed buffer max length, file path is: %s", 
                             mFile->GetPath().c_str()); 
    }        
    if (mFile->Read((void*)buffPtr, length, mCursor) < (size_t)length)
    {
        INDEXLIB_FATAL_ERROR(IndexCollapsed,
                             "Read patch file failed, file path is: %s", 
                             mFile->GetPath().c_str());
    }
    mCursor += length;
    buffPtr += length;
    leftBuffSize -= length;
}

inline uint32_t VarNumAttributePatchFile::ReadCount(
        uint8_t* buffPtr, size_t maxLen, size_t &encodeCountLen)
{
    if (mFixedValueCount != -1)
    {
        encodeCountLen = 0;
        return mFixedValueCount;
    }
    
    uint8_t* valuePtr = buffPtr;
    size_t readLen = sizeof(uint8_t);
    ReadAndMove(readLen, valuePtr, maxLen);
    encodeCountLen = 
        common::VarNumAttributeFormatter::GetEncodedCountFromFirstByte(*buffPtr);
    if (encodeCountLen == 0 || encodeCountLen > 4)
    {
        INDEXLIB_FATAL_ERROR(IndexCollapsed,
                             "Read value count from patch file failed, file path is: %s", 
                             mFile->GetPath().c_str());
    }

    readLen = encodeCountLen - 1;
    ReadAndMove(readLen, valuePtr, maxLen);
    return common::VarNumAttributeFormatter::DecodeCount(
            (char*)buffPtr, encodeCountLen);
}

IE_NAMESPACE_END(index);

#endif //__INDEXLIB_VAR_NUM_ATTRIBUTE_PATCH_FILE_H
