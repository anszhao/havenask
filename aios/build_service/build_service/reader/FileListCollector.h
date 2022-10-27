#ifndef ISEARCH_BS_FILELISTCOLLECTOR_H
#define ISEARCH_BS_FILELISTCOLLECTOR_H

#include "build_service/common_define.h"
#include "build_service/util/Log.h"
#include "build_service/proto/BasicDefs.pb.h"
#include "build_service/reader/CollectResult.h"
#include "build_service/config/ResourceReader.h"
#include <fslib/fslib.h>


namespace build_service {
namespace reader {
class FilePatternBase;
BS_TYPEDEF_PTR(FilePatternBase);
typedef std::vector<FilePatternBasePtr> FilePatterns;

struct CollectorDescription
{
    CollectorDescription()
        : rangeFrom(0)
        , rangeTo(65535)
    { }
    uint16_t rangeFrom;
    uint16_t rangeTo;
    std::vector<std::string> paths;
    std::string filePattern;
};

class FileListCollector
{
private:
    FileListCollector();
    ~FileListCollector();
    FileListCollector(const FileListCollector &);
    FileListCollector& operator=(const FileListCollector &);
public:
    static bool getBSRawFileMetaDir(
        config::ResourceReader* resourceReaderPtr,
        const std::string& dataTableName,
        uint32_t generationId,
        std::string& metaDir);
    static bool collect(const KeyValueMap &kvMap, 
                        const proto::Range &range, 
                        CollectResults &result);

private:
    static bool kvMap2Description(
            const KeyValueMap &kvMap, CollectorDescription &description);
    static bool parseRawDocumentPaths(
            std::vector<std::string> &paths, const std::string &rawDocumentDir);
    static bool collectAllResults(
        const CollectorDescription &desc, CollectResults &result);
    static bool getCollectResultsFromFile(
        const KeyValueMap & kvMap, CollectResults &result);
    static bool collect(const KeyValueMap & kvMap,
                        const CollectorDescription &desc, 
                        CollectResults &fileList);
    static bool parseFilePatterns(const std::string &filePatternStr,
                                  uint16_t rangeFrom, uint16_t rangeTo,
                                  FilePatterns &filePatterns);
    static bool getAllCandidateFiles(const std::string &path, fslib::FileList &result);
    static void calculateHashIds(const FilePatterns &filePatterns, CollectResults &result);
    static void calculateFileIds(const std::vector<FilePatterns> &filePatternsVec,
                                 const std::vector<CollectResults> &resultsVec,
                                 CollectResults &allResults);
    static bool needShuffleByHashId(const FilePatterns &filePatterns);
private:
    static const std::string RAW_DOCUMENT_PATH_SEPERATOR;
private:
    BS_LOG_DECLARE();
};

}
}

#endif //ISEARCH_BS_FILELISTCOLLECTOR_H
