#ifndef ISEARCH_PAGEDISTINCTPROCESSOR_H
#define ISEARCH_PAGEDISTINCTPROCESSOR_H

#include <ha3/common.h>
#include <ha3/isearch.h>
#include <ha3/util/Log.h>
#include <ha3/common/Request.h>
#include <ha3/common/Result.h>
#include <ha3/common/ErrorResult.h>
#include <ha3/qrs/QrsProcessor.h>
#include <ha3/qrs/PageDistinctSelector.h>

BEGIN_HA3_NAMESPACE(qrs);

struct PageDistParam {
    uint32_t pgHitNum;
    std::string distKey;
    uint32_t distCount;
    std::vector<double> gradeThresholds;
    std::string originalGradeStr; 
    std::vector<bool> sortFlags;
    uint32_t startOffset;
    uint32_t hitCount;
};

class PageDistinctProcessor : public QrsProcessor
{
public:
    static const std::string PAGE_DIST_PROCESSOR_NAME;
    static const std::string PAGE_DIST_PARAM_PAGE_SIZE;
    static const std::string PAGE_DIST_PARAM_DIST_COUNT;
    static const std::string PAGE_DIST_PARAM_DIST_KEY;
    static const std::string PAGE_DIST_PARAM_GRADE_THRESHOLDS;
    static const std::string GRADE_THRESHOLDS_SEP;
public:
    PageDistinctProcessor();
    virtual ~PageDistinctProcessor();
public:
    virtual void process(common::RequestPtr &requestPtr,
                         common::ResultPtr &resultPtr);
    virtual QrsProcessorPtr clone();
public:
    std::string getName() const {
        return PAGE_DIST_PROCESSOR_NAME;
    }
private:
    bool getPageDistParams(const common::RequestPtr &requestPtr,
                           PageDistParam &pageDistParam,
                           common::ErrorResult &errorResult) const;
    PageDistinctSelectorPtr createPageDistinctSelector(
            const PageDistParam &pageDistParam,
            const common::HitPtr &hitPtr) const;
    void getSortFlags(const common::RequestPtr &requestPtr,
                      std::vector<bool> &sortFlags) const;
    bool getPageHitNum(const common::ConfigClause *configClause,
                       uint32_t &pgHitNum, common::ErrorResult &errorResult) const;
    bool getDistCount(const common::ConfigClause *configClause,
                      uint32_t &distCount, common::ErrorResult &errorResult) const;
    bool getDistKey(const common::ConfigClause *configClause,
                    std::string &distKey, common::ErrorResult &errorResult) const;
    bool getGradeThresholds(const common::ConfigClause *configClause,
                            std::vector<double> &gradeThresholds,
                            std::string &originalGradeStr,
                            common::ErrorResult &errorResult) const;
    bool getKVPairValue(const common::ConfigClause *configClause,
                        const std::string &key, std::string &value, 
                        common::ErrorResult &errorResult) const;
    bool getKVPairUint32Value(const common::ConfigClause *configClause,
                              const std::string &key, uint32_t &value, 
                              common::ErrorResult &errorResult) const;

    bool resetRequest(common::RequestPtr requestPtr, 
                      const PageDistParam &pageDistParam, 
                      common::ErrorResult &errorResult) const;

    bool setDistinctClause(common::RequestPtr requestPtr, 
                           const PageDistParam &pageDistParam, 
                           common::ErrorResult &errorResult) const;
    void setAttributeClause(common::RequestPtr requestPtr,
                            const PageDistParam &pageDistParam) const;

    std::string getDistClauseString(
            const PageDistParam &pageDistParam) const;
    uint32_t calcWholePageCount(const PageDistParam &pageDistParam) const;
    void selectPages(const PageDistParam &pageDistParam, 
                     common::ResultPtr &resultPtr);
    void restoreRequest(common::RequestPtr requestPtr, 
                        const PageDistParam &pageDistParam, 
                        const std::set<std::string> &attrNames);

private:
    friend class PageDistinctProcessorTest;

private:
    HA3_LOG_DECLARE();
};

HA3_TYPEDEF_PTR(PageDistinctProcessor);

END_HA3_NAMESPACE(qrs);

#endif //ISEARCH_PAGEDISTINCTPROCESSOR_H
