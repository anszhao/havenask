#include <ha3/qrs/QrsChainManager.h>
#include <ha3/qrs/QrsModuleFactory.h>
#include <ha3/qrs/RequestParserProcessor.h>
#include <ha3/qrs/QrsSearchProcessor.h>
#include <ha3/qrs/QrsChainConfigurator.h>
#include <ha3/qrs/RequestValidateProcessor.h>
#include <ha3/qrs/CheckSummaryProcessor.h>
#include <ha3/qrs/PageDistinctProcessor.h>
#include <ha3/qrs/MatchInfoProcessor.h>
#include <build_service/plugin/Module.h>

using namespace std;
using namespace build_service::plugin;
using namespace build_service::analyzer;
using namespace suez::turing;
using namespace kmonitor;
USE_HA3_NAMESPACE(config);
USE_HA3_NAMESPACE(service);
BEGIN_HA3_NAMESPACE(qrs);
HA3_LOG_SETUP(qrs, QrsChainManager);

QrsChainManager::QrsChainManager(PlugInManagerPtr &plugInManagerPtr,
                                 config::ResourceReaderPtr &resourceReaderPtr)
    : _plugInManagerPtr(plugInManagerPtr)
    , _requestCompressType(NO_COMPRESS)
    , _resourceReaderPtr(resourceReaderPtr)
    , _metricsReporter(NULL)
{
}

QrsChainManager::~QrsChainManager() {
}

bool QrsChainManager::init() {
    clear();
    map<string, QrsChainInfo> &chainInfoMap = _chainConfig.chainInfoMap;
    for (map<string, QrsChainInfo>::iterator it = chainInfoMap.begin();
         it != chainInfoMap.end(); ++it)
    {
        if (!constructChain(it->second)) {
            return false;
        }
    }
    return true;
}

bool QrsChainManager::constructChain(const QrsChainInfo &chainInfo) {
    QrsProcessorPtr chainHead;
    QrsProcessorPtr chainTail;

    QrsProcessorPtr checkSummaryProcessorPtr(new CheckSummaryProcessor);
    linkChain(checkSummaryProcessorPtr, chainHead, chainTail);

    if (!createProcessorsBeforeParserPoint(chainInfo, chainHead, chainTail)) {
        return false;
    }

    //create processors in PARSER_PROCESS point (built-in)
    QrsProcessorPtr parserProcessorPtr(new RequestParserProcessor(
                    _clusterConfigMapPtr));
    linkChain(parserProcessorPtr, chainHead, chainTail);

    if (!createProcessorsBeforeValidatePoint(chainInfo, chainHead, chainTail)) {
        return false;
    }

    //create processors in VALIDATE_PROCESS point (built-in)
    QrsProcessorPtr validateProcessor = createValidateProcessor();
    linkChain(validateProcessor, chainHead, chainTail);

    if (!createProcessorsBeforeSearchPoint(chainInfo, chainHead, chainTail)) {
        return false;
    }

    //create processors in SEARCH_PROCESS point (built-in)
    QrsProcessorPtr searchProcesserPtr(new QrsSearchProcessor(_qrsRule._connectionTimeout,
                    _clusterTableInfoMapPtr, _clusterConfigMapPtr, _clusterSorterManagersPtr,
                    _requestCompressType, _qrsRule._searchTaskQueueRule));
    linkChain(searchProcesserPtr, chainHead, chainTail);

    _chainMap.insert(make_pair(chainInfo.getChainName(), chainHead));
    HA3_LOG(TRACE3, "add chain name: %s", chainInfo.getChainName().c_str());
    return true;
}

bool QrsChainManager::createProcessorsBeforeParserPoint(
        const QrsChainInfo &chainInfo,
        QrsProcessorPtr &chainHead, QrsProcessorPtr &chainTail)
{
    //create processors in BEFORE_PARSER_POINT point
    ProcessorNameVec rawStringProcessor
        = chainInfo.getPluginPoint(BEFORE_PARSER_POINT);
    return createProcessors(rawStringProcessor, chainHead, chainTail);
}

bool QrsChainManager::createProcessorsBeforeValidatePoint(
        const QrsChainInfo &chainInfo,
        QrsProcessorPtr &chainHead, QrsProcessorPtr &chainTail)
{
    ProcessorNameVec beforeValidateProcessors
        = chainInfo.getPluginPoint(BEFORE_VALIDATE_POINT);
    ProcessorNameVec::iterator it = beforeValidateProcessors.begin();
    for (;it != beforeValidateProcessors.end(); ++it) {
        if (*it == PageDistinctProcessor::PAGE_DIST_PROCESSOR_NAME) {
            break;
        }
    }

    // create the processors before PAGE_DIST_PROCESSOR
    if (it != beforeValidateProcessors.begin()) {
        ProcessorNameVec beforePageDistProcessors(
                beforeValidateProcessors.begin(), it);
        if (!createProcessors(beforePageDistProcessors, chainHead, chainTail)) {
            return false;
        }
    }

    // create PageDistinctProcessor
    if (it != beforeValidateProcessors.end()) {
        PageDistinctProcessorPtr pageDistProcessorPtr(
                new PageDistinctProcessor);
        linkChain(pageDistProcessorPtr, chainHead, chainTail);
        it ++;
    }

    // create the processors after PAGE_DIST_PROCESSOR
    ProcessorNameVec afterPageDistProcessors(
            it, beforeValidateProcessors.end());
    if (!createProcessors(afterPageDistProcessors, chainHead, chainTail)) {
        return false;
    }

    return true;
}

bool QrsChainManager::createProcessorsBeforeSearchPoint(
        const QrsChainInfo &chainInfo,
        QrsProcessorPtr &chainHead, QrsProcessorPtr &chainTail)
{
    //create processors in BEFORE_SEARCH_POINT point
    ProcessorNameVec requestProcessor
        = chainInfo.getPluginPoint(BEFORE_SEARCH_POINT);
    return createProcessors(requestProcessor, chainHead, chainTail);
}

QrsProcessorPtr QrsChainManager::createValidateProcessor() {
    RequestValidatorPtr requestValidatorPtr(new RequestValidator(_clusterTableInfoMapPtr,
                    _qrsRule._retHitsLimit, _clusterConfigMapPtr, _clusterFuncMapPtr,
                    _clusterCavaPluginManagerMapPtr, _clusterSorterNamesPtr));
    QueryTokenizer queryTokenizer(_analyzerFactoryPtr.get());

    QrsProcessorPtr validateProcessorPtr(new RequestValidateProcessor(
                    _clusterTableInfoMapPtr, _clusterConfigMapPtr,
                    requestValidatorPtr, queryTokenizer));
    return validateProcessorPtr;
}

bool QrsChainManager::createProcessors(const ProcessorNameVec &processorNames,
                                       QrsProcessorPtr &chainHead,
                                       QrsProcessorPtr &chainTail)
{
    for (uint32_t i = 0; i < processorNames.size(); i++)
    {
        const map<string, ProcessorInfo> &processorInfoMap = _chainConfig.processorInfoMap;
        map<string, ProcessorInfo>::const_iterator it
            = processorInfoMap.find(processorNames[i]);
        if(it == processorInfoMap.end()) {
            HA3_LOG(ERROR, "no such Processor: %s",processorNames[i].c_str());
            return false;
        }

        const ProcessorInfo &processorInfo = it->second;
        string processorName = processorInfo.getProcessorName();
        string moduleName = processorInfo.getModuleName();
        const KeyValueMap &keyValues = processorInfo.getParams();
        QrsProcessorPtr processorPtr;

        if (PlugInManager::isBuildInModule(moduleName)) {
            processorPtr = createBuildInProcessor(processorName);
        } else {
            Module* module = _plugInManagerPtr->getModule(moduleName);
            if (module == NULL) {
                HA3_LOG(ERROR, "Get module [%s] failed", moduleName.c_str());
                return false;
            }
            QrsModuleFactory* factory
                = dynamic_cast<QrsModuleFactory*>(module->getModuleFactory());
            if (!factory) {
                HA3_LOG(ERROR, "Wrong QrsModuleFactory type:%s", moduleName.c_str());
                return false;
            }
            processorPtr.reset(factory->createQrsProcessor(processorName));
            HA3_LOG(TRACE3, "processor:%s", processorName.c_str());
        }
        if (!processorPtr) {
            HA3_LOG(ERROR, "Failed to create processor:%s", processorName.c_str());
            return false;
        }
        QrsProcessorInitParam initParam;
        initParam.keyValues = &keyValues;
        initParam.resourceReader = _resourceReaderPtr.get();
        initParam.metricsReporter = _metricsReporter;
        initParam.clusterSorterManagersPtr = _clusterSorterManagersPtr;

        if (!processorPtr->init(initParam)) {
            HA3_LOG(ERROR, "Failed to init processor:%s", processorName.c_str());
            return false;
        }

        linkChain(processorPtr, chainHead, chainTail);
    }
    return true;
}

QrsProcessorPtr QrsChainManager::createBuildInProcessor(const string &processorName){
    QrsProcessorPtr processorPtr;
    if (processorName == DEFAULT_DEBUG_PROCESSOR) {
        map<string, string> cluster2table;
        ClusterConfigMap::const_iterator iter = _clusterConfigMapPtr->begin();
        while(iter != _clusterConfigMapPtr->end()){
            cluster2table[iter->first] = iter->second.getTableName();
            iter++;
        }
        MatchInfoProcessor *matchInfoProcessor =
            new MatchInfoProcessor(_clusterTableInfoMapPtr);
        matchInfoProcessor->setAnalyzerFactory(_analyzerFactoryPtr);
        processorPtr.reset(matchInfoProcessor);
    } else {
        HA3_LOG(ERROR, "failed to create qrs processor [%s] with BuildInModule",
                processorName.c_str());
    }
    return processorPtr;
}

void QrsChainManager::linkChain(QrsProcessorPtr newProcessorPtr,
                                QrsProcessorPtr &chainHead,
                                QrsProcessorPtr &chainTail)
{
    if (!chainHead.get()) {
        chainHead = newProcessorPtr;
        chainTail = newProcessorPtr;
    } else {
        chainTail->setNextProcessor(newProcessorPtr);
        chainTail = chainTail->getNextProcessor();
    }
}

void QrsChainManager::addProcessorChain(const string &chainName, QrsProcessorPtr chain)
{
    if (_chainMap.find(chainName) == _chainMap.end()) {
        _chainMap[chainName] = chain;
    } else {
        HA3_LOG(WARN, "there is a same processor chain: %s", chainName.c_str());
    }
}

QrsProcessorPtr QrsChainManager::cloneProcessorChain(QrsProcessorPtr firstProcessor) {
    QrsProcessorPtr chainHead;
    QrsProcessorPtr chainTail;

    QrsProcessorPtr currentProcessor = firstProcessor;
    while(currentProcessor) {
        QrsProcessorPtr clonedProcessor = currentProcessor->clone();
        linkChain(clonedProcessor, chainHead, chainTail);
        currentProcessor = currentProcessor->getNextProcessor();
    }

    return chainHead;
}

QrsProcessorPtr QrsChainManager::getProcessorChain(const string &chainName)
{
    if (_chainMap.find(chainName) != _chainMap.end()) {
        HA3_LOG(TRACE3, "find chain: %s", chainName.c_str());
        QrsProcessorPtr retProcessor = cloneProcessorChain(_chainMap[chainName]);
        return retProcessor;
    } else {
        HA3_LOG(TRACE3, "not find chain: %s, chainmap.size: %zu",
                chainName.c_str(), _chainMap.size());
        QrsProcessorPtr nullProcessor;
        return nullProcessor;
    }
}

void QrsChainManager::setMetricsReporter(MetricsReporter *metricsReporter) {
    _metricsReporter = metricsReporter;
}

void QrsChainManager::setQrsChainConfig(const QrsChainConfig &qrsChainConfig) {
    _chainConfig = qrsChainConfig;
}

void QrsChainManager::setPlugInManagerPtr(PlugInManagerPtr &plugInManagerPtr) {
    _plugInManagerPtr = plugInManagerPtr;
}

void QrsChainManager::setClusterTableInfoMap(const ClusterTableInfoMapPtr &tableInfoMapPtr) {
    _clusterTableInfoMapPtr = tableInfoMapPtr;
}

void QrsChainManager::setClusterConfigMap(const config::ClusterConfigMapPtr &clusterConfigMapPtr) {
    _clusterConfigMapPtr = clusterConfigMapPtr;
}

void QrsChainManager::setFuncInfoMap(const ClusterFuncMapPtr &clusterFuncMapPtr) {
    _clusterFuncMapPtr = clusterFuncMapPtr;
}

void QrsChainManager::setCavaPluginManagerMap(const suez::turing::CavaPluginManagerMapPtr &clusterCavaPluginManagerMapPtr) {
    _clusterCavaPluginManagerMapPtr = clusterCavaPluginManagerMapPtr;
}

void QrsChainManager::setClusterSorterManagers(
        const ClusterSorterManagersPtr &clusterSorterManagersPtr)
{
    _clusterSorterManagersPtr = clusterSorterManagersPtr;
}

void QrsChainManager::setClusterSorterNames(
        const ClusterSorterNamesPtr &clusterSorterNamesPtr)
{
    _clusterSorterNamesPtr = clusterSorterNamesPtr;
}

void QrsChainManager::setAnalyzerFactory(const AnalyzerFactoryPtr &analyzerFactoryPtr) {
    _analyzerFactoryPtr = analyzerFactoryPtr;
}

void QrsChainManager::addProcessorInfo(const ProcessorInfo &processorInfo)
{
    map<string, ProcessorInfo> &processorInfoMap = _chainConfig.processorInfoMap;
    processorInfoMap[processorInfo.getProcessorName()] = processorInfo;
}

void QrsChainManager::addQrsChainInfo(const QrsChainInfo &chainInfo)
{
    map<string, QrsChainInfo> &chainInfoMap = _chainConfig.chainInfoMap;
    chainInfoMap[chainInfo.getChainName()] = chainInfo;
}

ProcessorInfo& QrsChainManager::getProcessorInfo(
        const string &processorName)
{
    map<string, ProcessorInfo> &processorInfoMap = _chainConfig.processorInfoMap;
    return processorInfoMap[processorName];
}

void QrsChainManager::clear() {
    _chainMap.clear();
}

void QrsChainManager::setQRSRule(const QRSRule &qrsRule)
{
    _qrsRule = qrsRule;
}

const QRSRule& QrsChainManager::getQRSRule()
{
    return _qrsRule;
}

END_HA3_NAMESPACE(qrs);
