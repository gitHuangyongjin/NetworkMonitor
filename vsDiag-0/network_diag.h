#define MAX_LINE_LENGTH 256
#define LOG_FILE "/tmp/vs_diag_dir/system_monitor.log"
#define MAX_LOG_SIZE (512 * 1000) // 500kb
#define PING_SERVER1 "223.5.5.5"  // 阿里DNS
#define PING_SERVER2 "www.baidu.com" // 百度域名
#define PING_RETRY 3              // 重试次数
#define PING_TIMEOUT 2            // 单次超时时间（秒）
#define PING_INTERVAL 10       // 每10秒检测一次
#define STA_INTERVAL 10       // 每20秒检测一次
#define SYSTEM_LOG_INTERVAL 5       // 每5秒检测一次
#define HOUR_INTERVAL 3600        // 1小时定时
// 优化后的日志写入函数
// 在宏定义部分新增网关地址
#define GATEWAY_IP "192.168.131.1"  // 新增网关地址
// 修改为支持多接口的结构体
#define MAX_INTERFACES 2
struct network_stats {
    char ifname[16];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    unsigned long long rx_dropped;
    unsigned long long tx_dropped;
    time_t last_time;
};

struct cpu_stats {
    unsigned long long total;
    unsigned long long idle;
    time_t last_time;
};
// 新增时间管理结构体
struct timer_ctx {
    time_t last_check;
    int interval;
};


