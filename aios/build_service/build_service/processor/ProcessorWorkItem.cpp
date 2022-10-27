#include "build_service/processor/ProcessorWorkItem.h"
#include <indexlib/misc/metric_provider.h>
#include "build_service/util/Monitor.h"

using namespace std;
using namespace build_service::document;
IE_NAMESPACE_USE(misc);

namespace build_service {
namespace processor {

BS_LOG_SETUP(processor, ProcessorWorkItem);

ProcessorWorkItem::ProcessorWorkItem(const DocumentProcessorChainVecPtr &chains,
                                     const ProcessorChainSelectorPtr &chainSelector,
                                     ProcessorMetricReporter *reporter)
    : _chains(chains)
    , _chainSelector(chainSelector)
    , _reporter(reporter)
    , _processErrorCount(0)
{
}

ProcessorWorkItem::~ProcessorWorkItem() {
}

void ProcessorWorkItem::process() {
    if (_batchRawDocsPtr) {
        batchProcess();
        return;
    }
    singleProcess();
}

void ProcessorWorkItem::batchProcess() {
    ScopeLatencyReporter scopeTime(_reporter->_processLatencyMetric.get());
    INCREASE_VALUE(_reporter->_processQpsMetric, _batchRawDocsPtr->size());
    
    if (_chainSelector->alwaysSelectAllChains()) {
        batchProcessWithAllChainSelector();
    } else {
        batchProcessWithCustomizeChainSelector();
    }
    if (_processedDocumentVecPtr->empty()) {
        _processedDocumentVecPtr.reset();
    }
}

void ProcessorWorkItem::singleProcess() {
    ScopeLatencyReporter scopeTime(_reporter->_processLatencyMetric.get());

    assert(_rawDocPtr);
    _processedDocumentVecPtr.reset(new ProcessedDocumentVec());
    _processedDocumentVecPtr->reserve(_chains->size());

    INCREASE_QPS(_reporter->_processQpsMetric);
    RawDocumentPtr rawDocPtr = _rawDocPtr;
    const ProcessorChainSelector::ChainIdVector* chainIds = _chainSelector->selectChain(_rawDocPtr);
    if (!chainIds)
    {
        BS_INTERVAL_LOG(300, ERROR, "select chain for document[%s] failed.",
                        rawDocPtr->toString().c_str());
        INCREASE_QPS(_reporter->_processErrorQpsMetric);
        _processErrorCount++;
        _processedDocumentVecPtr.reset();
        return;
    }
    {
        ScopeLatencyReporter chainScopeTime(_reporter->_chainProcessLatencyMetric.get());
        for (size_t i = 0; i < chainIds->size(); ++i) {
            RawDocumentPtr processRawDocPtr;
            size_t chainId = (*chainIds)[i];
            // optimize.
            // last chain use org raw document.
            if (i != chainIds->size() - 1) {
                processRawDocPtr.reset(rawDocPtr->clone());
            } else {
                processRawDocPtr = rawDocPtr;
            }
            assert(chainId < _chains->size());
            DocumentProcessorChainPtr chain((*_chains)[chainId]->clone());
            ProcessedDocumentPtr processedDoc = chain->process(processRawDocPtr);
            assembleProcessedDoc(chainId, processRawDocPtr, processedDoc);
            _processedDocumentVecPtr->push_back(processedDoc);
        }
    }
    
    if (_processedDocumentVecPtr->empty()) {
        _processedDocumentVecPtr.reset();
    }
}

void ProcessorWorkItem::assembleProcessedDoc(size_t chainId,
        const RawDocumentPtr &rawDocPtr, ProcessedDocumentPtr &processedDoc) {
    if (!processedDoc) {
        INCREASE_QPS(_reporter->_processErrorQpsMetric);
        BS_INTERVAL_LOG(300, INFO, "chain[%lu] process document[%s] failed.",
                        chainId, rawDocPtr->toString().c_str());
        ERROR_COLLECTOR_LOG(INFO, "chain[%lu] process document[%s] failed.",
                            chainId, rawDocPtr->toString().c_str());
        _processErrorCount++;
        processedDoc.reset(new ProcessedDocument());
    } else if (processedDoc->needSkip()) {
        processedDoc->setDocument(IE_NAMESPACE(document)::DocumentPtr());
    } else {
        DocOperateType docOperateType = processedDoc->getDocument()->GetDocOperateType();
        if (SKIP_DOC == docOperateType) {
            processedDoc->setDocument(IE_NAMESPACE(document)::DocumentPtr());
        } else if (UNKNOWN_OP == docOperateType) {
            INCREASE_QPS(_reporter->_processErrorQpsMetric);
            _processErrorCount++;
            string errorMsg = "unknown document cmd in document[" + rawDocPtr->toString() + "]";
            BS_LOG(ERROR, "%s", errorMsg.c_str());
            processedDoc->setDocument(IE_NAMESPACE(document)::DocumentPtr());
        }
    }
    // set locator to processed doc, for job mode
    processedDoc->setLocator(rawDocPtr->getLocator());
}

void ProcessorWorkItem::destroy() {
    delete this;
}

void ProcessorWorkItem::drop() {
    destroy();
}

size_t ProcessorWorkItem::getRawDocumentsForEachChain(
        const RawDocumentVecPtr& batchRawDocsPtr,
        vector<RawDocumentVecPtr> &rawDocsForChains,
        vector<const ProcessorChainSelector::ChainIdVector*> &chainIdQueue)
{
    ScopeLatencyReporter scopeTime(_reporter->_prepareBatchProcessLatencyMetric.get());
    assert(batchRawDocsPtr);
    rawDocsForChains.resize(_chains->size());
    chainIdQueue.reserve(batchRawDocsPtr->size());
    
    size_t count = 0;
    for (int32_t idx = 0; idx < (int32_t)batchRawDocsPtr->size(); idx++) {
        const RawDocumentPtr& rawDocPtr = (*batchRawDocsPtr)[idx];

        const ProcessorChainSelector::ChainIdVector* chainIds = _chainSelector->selectChain(rawDocPtr);
        if (!chainIds)
        {
            BS_INTERVAL_LOG(300, ERROR, "select chain for document[%s] failed.",
                            rawDocPtr->toString().c_str());
            INCREASE_QPS(_reporter->_processErrorQpsMetric);
            _processErrorCount++;
            continue;
        }
        RawDocumentPtr processRawDocPtr;
        for (size_t i = 0; i < chainIds->size(); ++i) {
            size_t chainId = (*chainIds)[i];
            if (i != chainIds->size() - 1) {
                processRawDocPtr.reset(rawDocPtr->clone());
            } else {
                processRawDocPtr = rawDocPtr;
            }
            if (!rawDocsForChains[chainId]) {
                rawDocsForChains[chainId].reset(new RawDocumentVec);
                rawDocsForChains[chainId]->reserve(batchRawDocsPtr->size());
            }
            rawDocsForChains[chainId]->push_back(processRawDocPtr);
            ++count;
        }
        chainIdQueue.push_back(chainIds);
    }
    return count;
}

void ProcessorWorkItem::batchProcessWithCustomizeChainSelector()
{
    assert(_batchRawDocsPtr);
    vector<RawDocumentVecPtr> rawDocsForChains;
    vector<const ProcessorChainSelector::ChainIdVector*> chainIdQueue;
    size_t count = getRawDocumentsForEachChain(
            _batchRawDocsPtr, rawDocsForChains, chainIdQueue);
    _processedDocumentVecPtr.reset(new ProcessedDocumentVec());
    _processedDocumentVecPtr->reserve(count);

    vector<ProcessedDocumentVecPtr> processDocsForChains;
    processDocsForChains.reserve(_chains->size());

    {
        ScopeLatencyReporter chainScopeTime(_reporter->_chainProcessLatencyMetric.get());
        for (size_t i = 0; i < _chains->size(); i++) {
            if (!rawDocsForChains[i] || rawDocsForChains[i]->size() == 0) {
                processDocsForChains.push_back(ProcessedDocumentVecPtr());
                continue;
            }
            DocumentProcessorChainPtr chain((*_chains)[i]->clone());
            assert(i < rawDocsForChains.size());

            ProcessedDocumentVecPtr processDocVec = chain->batchProcess(rawDocsForChains[i]);
            assert(processDocVec);
            assert(processDocVec->size() == rawDocsForChains[i]->size());
            processDocsForChains.push_back(processDocVec);
        }
    }

    ScopeLatencyReporter endBatchScopeTime(_reporter->_endBatchProcessLatencyMetric.get());
    // pushback to vector by batchIdx
    vector<size_t> idxForEachChain(_chains->size(), 0);
    for (auto chainIds : chainIdQueue) {
        for (auto chainId : *chainIds) {
            const ProcessedDocumentVec& pDocs = *processDocsForChains[chainId];
            size_t idx = idxForEachChain[chainId];
            ProcessedDocumentPtr pDoc = pDocs[idx];
            assembleProcessedDoc(chainId, (*rawDocsForChains[chainId])[idx], pDoc);
            _processedDocumentVecPtr->push_back(pDoc);
            ++idxForEachChain[chainId];
        }
    }
}

void ProcessorWorkItem::batchProcessWithAllChainSelector()
{
    assert(_batchRawDocsPtr);
    _processedDocumentVecPtr.reset(new ProcessedDocumentVec());
    _processedDocumentVecPtr->reserve(_chains->size() * _batchRawDocsPtr->size());
    vector<ProcessedDocumentVecPtr> processDocsForChains;
    processDocsForChains.reserve(_chains->size());

    {
        ScopeLatencyReporter chainScopeTime(_reporter->_chainProcessLatencyMetric.get());
        RawDocumentVecPtr rawDocsForChain;
        for (size_t i = 0; i < _chains->size(); i++) {
            if (i != _chains->size() - 1) {
                if (!rawDocsForChain) {
                    rawDocsForChain.reset(new RawDocumentVec);
                    rawDocsForChain->reserve(_batchRawDocsPtr->size());
                } else {
                    rawDocsForChain->clear();
                }
                for (size_t idx = 0; idx < _batchRawDocsPtr->size(); idx++) {
                    RawDocumentPtr processRawDocPtr((*_batchRawDocsPtr)[idx]->clone());
                    rawDocsForChain->push_back(processRawDocPtr);
                }
            } else {
                rawDocsForChain = _batchRawDocsPtr;
            }
            DocumentProcessorChainPtr chain((*_chains)[i]->clone());
            ProcessedDocumentVecPtr processDocVec = chain->batchProcess(rawDocsForChain);
            assert(processDocVec);
            assert(processDocVec->size() == rawDocsForChain->size());
            processDocsForChains.push_back(processDocVec);
        }
    }

    ScopeLatencyReporter endBatchScopeTime(
            _reporter->_endBatchProcessLatencyMetric.get());
    for (size_t i = 0; i < _batchRawDocsPtr->size(); i++) {
        for (size_t chainId = 0; chainId < processDocsForChains.size(); ++chainId) {
            const ProcessedDocumentVec& pDocs = *processDocsForChains[chainId];
            ProcessedDocumentPtr pDoc = pDocs[i];
            assembleProcessedDoc(chainId, (*_batchRawDocsPtr)[i], pDoc);
            _processedDocumentVecPtr->push_back(pDoc);
        }
    }
}

uint32_t ProcessorWorkItem::getRawDocCount() const
{
    if (_rawDocPtr)
    {
        return 1;
    }
    if (_batchRawDocsPtr)
    {
        return _batchRawDocsPtr->size();
    }
    return 0;
}

}
}
