#ifndef ISEARCH_FILLATTRIBUTESOP_H
#define ISEARCH_FILLATTRIBUTESOP_H

#include <autil/Log.h>
#include <tensorflow/core/framework/op_kernel.h>
#include <iostream>
#include <ha3/common/ha3_op_common.h>
#include <suez/turing/common/SessionResource.h>
#include <ha3/util/Log.h>
#include <ha3/turing/ops/SearcherQueryResource.h>
#include <ha3/turing/ops/SearcherSessionResource.h>
#include <ha3/turing/variant/Ha3RequestVariant.h>
#include <ha3/turing/variant/Ha3ResultVariant.h>
#include <ha3/turing/variant/AggregateResultsVariant.h>
#include <ha3/rank/RankProfileManager.h>
#include <ha3/search/MatchDocSearcher.h>
#include <ha3/search/SearchCommonResource.h>
#include <ha3/search/InnerSearchResult.h>
#include <ha3/service/SearcherResource.h>
#include <indexlib/partition/index_partition.h>
#include <ha3/common/Tracer.h>
#include <ha3/common/TimeoutTerminator.h>
#include <autil/mem_pool/Pool.h>

BEGIN_HA3_NAMESPACE(turing);

class Ha3FillAttributesOp : public tensorflow::OpKernel
{
public:
    explicit Ha3FillAttributesOp(tensorflow::OpKernelConstruction* ctx) : tensorflow::OpKernel(ctx) {     
    }
public:

    void Compute(tensorflow::OpKernelContext* ctx) override;
    bool fillPk(const common::ConfigClause *configClause,
                std::vector<matchdoc::MatchDoc> &matchDocVec,
                common::Ha3MatchDocAllocatorPtr &matchDocAllocator,
                suez::turing::AttributeExpressionCreator &attributeExpressionCreator,
                search::SearchCommonResourcePtr &commonResource);

    
    bool getPKExprInfo(uint8_t phaseOneInfoMask,
                       std::string &exprName, std::string &refName,
                       search::SearchCommonResourcePtr &commonResource);


    bool fillAttribute(const common::AttributeClause *attributeClause,
                       std::vector<matchdoc::MatchDoc> &matchDocVec,
                       common::Ha3MatchDocAllocatorPtr &matchDocAllocator,
                       suez::turing::AttributeExpressionCreator &attributeExpressionCreator);
private:
    HA3_LOG_DECLARE();

};

END_HA3_NAMESPACE(turing);
#endif // ISEARCH_FILLATTRIBUTESOP_H
