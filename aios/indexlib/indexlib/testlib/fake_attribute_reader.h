#ifndef __INDEXLIB_FAKE_ATTRIBUTE_READER_H
#define __INDEXLIB_FAKE_ATTRIBUTE_READER_H

#include <tr1/memory>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include <indexlib/index/normal/attribute/accessor/attribute_reader.h>
#include <indexlib/index/normal/attribute/accessor/single_value_attribute_reader.h>

IE_NAMESPACE_BEGIN(testlib);

template <typename T>
class FakeAttributeReader : public index::SingleValueAttributeReader<T>
{
public:
    class FakeSegmentReader : public index::SingleValueAttributeSegmentReader<T>
    {
    private:
        struct Config {
            config::FieldConfigPtr DEFAULT_FIELD_CONFIG;
            config::AttributeConfigPtr DEFAULT_ATTR;
            Config()
            {
                DEFAULT_FIELD_CONFIG.reset(new config::FieldConfig("", FieldType::ft_uint32, false, false));
                DEFAULT_ATTR.reset(new config::AttributeConfig());
                DEFAULT_ATTR->Init(DEFAULT_FIELD_CONFIG);
            }
        };
    public:
        FakeSegmentReader()
            : index::SingleValueAttributeSegmentReader<T>(Config().DEFAULT_ATTR)
        {
        }
        FakeSegmentReader(uint32_t docCount)
            : index::SingleValueAttributeSegmentReader<T>(Config().DEFAULT_ATTR)
        {
            this->mDocCount = docCount;
            this->mData = new uint8_t[this->mDocCount * sizeof(T)];
            this->mFormatter.Init(0, sizeof(T));
        }

        ~FakeSegmentReader()
        {
            release();
        }
    public:
        void open(T* data, uint32_t docCount) {
            release();
            this->mData = (uint8_t*)(new T[docCount]);
            memcpy(this->mData, data, docCount * sizeof(T));
            this->mDocCount = docCount;
        }
    private:
        void release() {
            if (this->mData) {
                delete[] this->mData;
                this->mData = NULL;
            }
        }
    };
public:
    FakeAttributeReader() {}
    ~FakeAttributeReader() {}
public:
    bool Read(docid_t docId, std::string& attrValue,
              autil::mem_pool::Pool* pool) const override
    {
        T value = (size_t)docId < mValues.size() ? mValues[docId] : std::numeric_limits<T>::max();
        attrValue = autil::StringUtil::toString(value);
        return true;
    }

    bool Read(docid_t docId, uint8_t* buf, uint32_t bufLen) const override
    { assert(false); return true; }
public:
    void setAttributeValues(const std::string &strValues);
    void setAttributeValues(const std::vector<T> &values);
    void SetSortType(SortPattern sortType)
    { index::SingleValueAttributeReader<T>::mSortType = sortType; }

private:
    std::vector<T> mValues;

private:
    IE_LOG_DECLARE();
};
//////////////////////////////////////////////
template<typename T>
static inline void convertStringToValues(std::vector<T> &values, const std::string &strValues)
{
    autil::StringUtil::fromString(strValues, values, ",");
}

template<typename T>
static inline void convertStringToValues(std::vector<std::vector<T> > &values, const std::string &strValues) {
    autil::StringUtil::fromString(strValues, values, ",", "#");
}

template<typename T>
void FakeAttributeReader<T>::setAttributeValues(const std::string &strValues) {
    convertStringToValues(mValues, strValues);
    setAttributeValues(mValues);
}

template <typename T>
void FakeAttributeReader<T>::setAttributeValues(const std::vector<T> &values) {
    mValues = values;

    FakeSegmentReader* reader = new FakeSegmentReader();
    typename index::SingleValueAttributeReader<T>::SegmentReaderPtr readerPtr(reader);
    reader->open((T*)&(mValues[0]), (uint32_t)mValues.size());

    this->mSegmentReaders.push_back(readerPtr);

    index_base::SegmentInfo segInfo;
    segInfo.docCount = (uint32_t)mValues.size();
    this->mSegmentInfos.push_back(segInfo);
}

IE_NAMESPACE_END(testlib);

#endif //__INDEXLIB_FAKE_ATTRIBUTE_READER_H
