#pragma once
#include <autil/Log.h>
#include <tensorflow/core/framework/op_kernel.h>
#include <iostream>
#include <ha3/common/ha3_op_common.h>
#include <suez/turing/common/SessionResource.h>
#include <ha3/util/Log.h>
#include <ha3/turing/ops/SearcherQueryResource.h>
#include <ha3/turing/ops/SearcherSessionResource.h>
#include <ha3/turing/ops/QrsSessionResource.h>
#include <ha3/turing/ops/QrsQueryResource.h>
#include <ha3/turing/variant/Ha3RequestVariant.h>
#include <ha3/turing/variant/Ha3ResultVariant.h>
#include <ha3/rank/RankProfileManager.h>
#include <ha3/search/MatchDocSearcher.h>
#include <ha3/search/SearchCommonResource.h>
#include <suez/turing/expression/plugin/SorterWrapper.h>
#include <ha3/sorter/SorterResource.h>
#include <ha3/service/SearcherResource.h>
#include <indexlib/partition/index_partition.h>
#include <ha3/monitor/SessionMetricsCollector.h>
#include <ha3/common/Tracer.h>
#include <ha3/common/TimeoutTerminator.h>
#include <autil/mem_pool/Pool.h>
#include <suez/turing/expression/framework/SimpleAttributeExpressionCreator.h>

BEGIN_HA3_NAMESPACE(turing);

class Ha3SorterOp : public tensorflow::OpKernel
{
public:
    explicit Ha3SorterOp(tensorflow::OpKernelConstruction* ctx) : tensorflow::OpKernel(ctx) {
        std::string locStr;
        OP_REQUIRES_OK(ctx, ctx->GetAttr("attr_location", &locStr));
        _location = sorter::transSorterLocation(locStr);
        OP_REQUIRES(ctx, _location != sorter::SL_UNKNOWN,
                    tensorflow::errors::Unavailable("sorter location not set:", locStr));
    }
public:
    void Compute(tensorflow::OpKernelContext* ctx) override;

    bool sortInSearcher(common::RequestPtr &request,
                        tensorflow::SessionResource *sessionResource,
                        tensorflow::QueryResource *queryResource,
                        suez::turing::MatchDocsVariant *matchDocsVariant,
                        uint32_t extraRankCount,
                        uint32_t &totalMatchDocs,
                        uint32_t &actualMatchDocs);

    void doLazyEvaluate(autil::mem_pool::PoolVector<matchdoc::MatchDoc> &matchDocVect,
                        common::Ha3MatchDocAllocatorPtr &matchDocAllocator,
                        const rank::ComparatorCreator *comparatorCreator);

    suez::turing::SorterWrapper *createFinalSorterWrapper(const common::Request *request,
            const suez::turing::SorterManager *sorterManager,
            autil::mem_pool::Pool *pool) const;

    sorter::SorterResource createSorterResource(const common::Request *request,
            search::SearchCommonResource &commonResource,
            search::SearchPartitionResource &partitionResource,
            search::SearchRuntimeResource &runtimeResource);

    bool sortInQrs(common::RequestPtr &request,
                   tensorflow::SessionResource *sessionResource,
                   tensorflow::QueryResource *queryResource,
                   suez::turing::MatchDocsVariant *matchDocsVariant,
                   uint32_t extraRankCount,
                   uint32_t &totalMatchDocs,
                   uint32_t &actualMatchDocs,
                   uint32_t srcCount);

    void fillSorterResourceInfo(sorter::SorterResource& sorterResource,
                                const common::Request *request,
                                uint32_t *topK,
                                suez::turing::AttributeExpressionCreatorBase *exprCreator,
                                search::SortExpressionCreator *sortExprCreator,
                                const common::Ha3MatchDocAllocatorPtr &allocator,
                                common::DataProvider *dataProvider,
                                rank::ComparatorCreator *compCreator,
                                autil::mem_pool::Pool *pool,
                                uint32_t resultSourceNum,
                                suez::turing::Tracer *tracer);

    suez::turing::SorterWrapper *createSorterWrapper(const common::Request *request,
            suez::turing::SorterManager *sorterManager,
            autil::mem_pool::Pool *pool);

    suez::turing::SorterWrapper *createSorterWrapper(const common::Request *request,
            const suez::turing::ClusterSorterManagersPtr &clusterSorterManagersPtr,
            autil::mem_pool::Pool *pool);
private:
    sorter::SorterLocation _location;

private:
    HA3_LOG_DECLARE();

};


END_HA3_NAMESPACE(turing);
