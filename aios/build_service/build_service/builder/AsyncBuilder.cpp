#include "build_service/builder/AsyncBuilder.h"
#include "build_service/util/Monitor.h"
#include <autil/Thread.h>

using namespace std;
using namespace autil;
using namespace build_service::config;

IE_NAMESPACE_USE(document);
IE_NAMESPACE_USE(index_base);
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(partition);

namespace build_service {
namespace builder {
BS_LOG_SETUP(builder, AsyncBuilder);

#define LOG_PREFIX _buildId.ShortDebugString().c_str()

AsyncBuilder::AsyncBuilder(const IndexBuilderPtr &indexBuilder,
                           fslib::fs::FileLock *fileLock,
                           const proto::BuildId &buildId)
    : Builder(indexBuilder, fileLock, buildId)
    , _running(false)
    , _asyncQueueSizeMetric(nullptr)
    , _asyncQueueMemMetric(nullptr)
{
}
AsyncBuilder::~AsyncBuilder()
{
    stop();
}

bool AsyncBuilder::init(const BuilderConfig &builderConfig,
                        IE_NAMESPACE(misc)::MetricProviderPtr metricProvider)
{
    if (!Builder::init(builderConfig, metricProvider)) {
        return false;
    }
    assert(builderConfig.asyncBuild);
    _docQueue.reset(new DocQueue(builderConfig.asyncQueueSize,
                                 builderConfig.asyncQueueMem * 1024 * 1024));

    //+2 to avoid block build thread
    _releaseDocQueue.reset(new Queue(builderConfig.asyncQueueSize + 2));

    _asyncQueueSizeMetric =
        DECLARE_METRIC(metricProvider, perf/asyncQueueSize, kmonitor::STATUS, "count");
    _asyncQueueMemMetric = DECLARE_METRIC(metricProvider, perf/asyncQueueMemUse, kmonitor::STATUS, "MB");

    _running = true;
    _asyncBuildThreadPtr = Thread::createThread(
            std::tr1::bind(&AsyncBuilder::buildThread, this), "BsAsyncB");

    if (!_asyncBuildThreadPtr) {
        _running = false;
        string errorMsg = "start async build thread failed";
        BS_PREFIX_LOG(ERROR, "%s", errorMsg.c_str());
        return false;
    }
    return true;
}

void AsyncBuilder::clearQueue() {
    while (!_docQueue->empty()) {
        DocumentPtr doc;
        _docQueue->pop(doc);
        doc.reset();
    }
}

void AsyncBuilder::buildThread() {
    while (_running) {
        DocumentPtr doc;
        //fix bug: 8214135
        if (!_docQueue->top(doc))
        {
            BS_PREFIX_LOG(TRACE1, "doc queue get doc return false");
            continue;
        }

        if (!Builder::build(doc)) {
            string errorMsg = "build failed";
            BS_PREFIX_LOG(ERROR, "%s", errorMsg.c_str());
            setFatalError();
            clearQueue();
            //TODO: need to think
            _running = false;
            break;
        }
        _docQueue->pop(doc);
        _releaseDocQueue->push(doc);
    }
    BS_PREFIX_LOG(INFO, "async builder thread exit");
}

void AsyncBuilder::releaseDocs() {
    while (!_releaseDocQueue->empty()) {
        DocumentPtr doc;
        _releaseDocQueue->pop(doc);
        doc.reset();
    }
}

bool AsyncBuilder::build(const DocumentPtr &doc)
{
    if (!_running) {
        string errorMsg = "receive document after builder stop.";
        BS_PREFIX_LOG(WARN, "%s", errorMsg.c_str());
        return false;
    }
    if (hasFatalError()) {
        return false;
    }

    releaseDocs();
    _docQueue->push(doc, doc->EstimateMemory());
    REPORT_METRIC(_asyncQueueSizeMetric, _docQueue->size());
    REPORT_METRIC(_asyncQueueMemMetric, _docQueue->memoryUse() / 1024.0 / 1024.0);
    return true;
}

bool AsyncBuilder::merge(const IndexPartitionOptions &options)
{
    assert(_running == false);
    assert(_docQueue->empty());
    return Builder::merge(options);
}

void AsyncBuilder::stop(int64_t stopTimestamp)
{
    BS_PREFIX_LOG(INFO, "async builder stop begin");
    _docQueue->setFinish();

    BS_PREFIX_LOG(INFO, "wait all docs in queue built");
    if (!_running && !_docQueue->empty())
    {
        string errorMsg = "some docs in queue, but build thread is not running";
        BS_PREFIX_LOG(ERROR, "%s", errorMsg.c_str());
    }
    const uint32_t interval = 50000; //50ms
    while (!isFinished())
    {
        usleep(interval);
    }

    BS_PREFIX_LOG(INFO, "all docs in queue build finish");
    _running = false;
    _asyncBuildThreadPtr.reset();
    releaseDocs();
    Builder::stop(stopTimestamp);
    BS_PREFIX_LOG(INFO, "async builder stop end");
}

#undef LOG_PREFIX

}
}
