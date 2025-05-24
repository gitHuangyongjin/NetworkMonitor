#include <hi_em_typedef.h>

// 新增结构体定义
struct sta_info {
    char mac[18];
    int rssi;
    char vap[5]; // vap8或vap9
};

typedef struct {
    HI_EmMac apMac;
    unsigned short staCount;
    int apUplinkrate;
    int apDownlinkrate;
    int apRssi;
    struct {
        HI_EmMac staMac;
        int rssi;
        int uplinkrate;
        int downlinkrate;
    } staList[HI_EM_CLIENT_STA_OF_BSS_MAX_NUM];
} EmApTopology;

typedef struct {
    unsigned char apCount;
    EmApTopology apList[HI_EM_DEVNUM_MAX];
} EmNetworkTopology;

// 在宏定义部分新增
#define STA_LOG_FILE "/tmp/vs_diag_dir/sta_monitor.log"
#define MAX_STA_LOG_SIZE (512 * 1024) // 512KB
#define MAX_STA 128 // 最大客户端数量
#define TARGET_VAP_COUNT 2
#define TARGET_VAPS {"vap8", "vap9"}


#define STA_RSSI_LOG "/tmp/vs_diag_dir/sta_rssi.log"  // 新增RSSI日志文件定义
// 在宏定义部分新增
#define MAX_RSSI_LOG_SIZE (1024 * 512) // 512Kb

void write_sta_log(void);

int write_sta_rssi_log(void);
int collect_sta_stats(struct sta_info *sta_list, int *total);


