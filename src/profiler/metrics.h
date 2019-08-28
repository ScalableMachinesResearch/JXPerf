#ifndef _METRICS_H
#define _METRICS_H

#include <assert.h>
#include <vector>
#include "xml.h"

namespace metrics {

typedef enum {
    METRIC_VAL_REAL,
    METRIC_VAL_INT,
} metric_val_enum_t;

typedef struct {
    std::string client_name;
    std::string name;
    uint32_t threshold = 0;
    metric_val_enum_t val_type = METRIC_VAL_INT;
} metric_info_t;

class MetricInfoManager {
public:
    static int registerMetric(const metric_info_t &metric_info);
    static metric_info_t *getMetricInfo(int metric_idx);
    static std::size_t getNumMetrics();
private:
    static std::vector<metric_info_t> _metric_info_list;
};


typedef struct {
    int64_t i = 0; // precise redundant bytes
    int64_t r = 0; // approximate redundant bytes

} metric_val_t;

inline metric_val_t operator+(const metric_val_t &v1, const metric_val_t &v2){
    metric_val_t sum;
    sum.r = v1.r + v2.r;
    sum.i = v1.i + v2.i;
    return sum;
}
inline metric_val_t &operator+=(metric_val_t &v1, const metric_val_t &v2){
    v1.r += v2.r;
    v1.i += v2.i;
    return v1;
}


class ContextMetrics {
public:
    bool increment(int metric_idx, const metric_val_t &val);
    metric_val_t *getMetricVal(int metric_idx);
private:
    std::vector<metric_val_t> _val_list;
    friend xml::XMLObj * xml::createXMLObj<ContextMetrics>(ContextMetrics *instance);
};

} /* namespace metric */

#endif
