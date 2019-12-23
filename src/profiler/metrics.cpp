#include "metrics.h"

namespace metrics {

std::vector<metric_info_t> MetricInfoManager::_metric_info_list;
    
int MetricInfoManager::registerMetric(const metric_info_t &metric_info){
    _metric_info_list.push_back(metric_info);
    return static_cast<int>(_metric_info_list.size() - 1);
}

metric_info_t *MetricInfoManager::getMetricInfo(int metric_idx){
    if (metric_idx < 0 or metric_idx >= static_cast<int>(_metric_info_list.size())){
        return nullptr;
    }
    return &(_metric_info_list[metric_idx]);
}

std::size_t MetricInfoManager::getNumMetrics(){
    return  _metric_info_list.size();
}

bool ContextMetrics::increment(int metric_idx, const metric_val_t &val){
    metric_val_t *tmp_val = getMetricVal(metric_idx);
    if (tmp_val == nullptr) return false;
    (*tmp_val) += val; 
    return true;
}

metric_val_t *ContextMetrics::getMetricVal(int metric_idx) {
    if (metric_idx < 0 or metric_idx >= MetricInfoManager::getNumMetrics()){
        return nullptr;
    }
    if (metric_idx >= _val_list.size()) {
        _val_list.resize(metric_idx + 1);
    }
    return &(_val_list[metric_idx]);
}

} /* namespace metric */

namespace xml {
using namespace metrics;

template<>
XMLObj *createXMLObj(ContextMetrics * instance) {
    std::ostringstream os;
    XMLObj *obj = new(std::nothrow) XMLObj("metrics");   
    assert(obj);
    bool isLargeRedundancy = false;

#define SET_ATTR(xmlobj, key, value) os.str(""); os << value; xmlobj->addAttr(key, os.str());
    for (uint32_t i = 0; i < instance->_val_list.size(); i++) {
        metric_val_t &metric_val = (instance->_val_list)[i];
        
        /*int min_red_byte = MetricInfoManager::getMetricInfo(i)->threshold * 10;
        if (metric_val.i >= min_red_byte || metric_val.r >= min_red_byte)*/ isLargeRedundancy = true;
        
        XMLObj *child_obj = new(std::nothrow) XMLObj("metric");
        assert(child_obj);
        SET_ATTR(child_obj, "id", i);

        if (metric_val.i) {
            SET_ATTR(child_obj, "value1", metric_val.i);
            // assert(child_obj->hasAttr("type", "FP") == false);
            // SET_ATTR(child_obj, "type1", "PRECISE");
        }   
        if (metric_val.r) {
            SET_ATTR(child_obj, "value2", metric_val.r);
            // assert(child_obj->hasAttr("type", "INT") == false);
            // SET_ATTR(child_obj, "type2", "APPROXIMATE");
        }
        obj->addChild(i, child_obj);
    }
#undef SET_ATTR

    if (!isLargeRedundancy) {
        delete obj;
        obj = nullptr;
    }
    return obj;
}

}
