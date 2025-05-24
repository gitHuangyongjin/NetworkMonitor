#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "sta_diag.h"

#include "igdCmModulePub.h"
#include <hi_em_typedef.h>


static int cli_init = 0;

void write_sta_log() {
    static FILE *fp = NULL;
    static long current_pos = 0;
    struct sta_info sta_list[MAX_STA];
    int total_sta = 0;
    int ret = collect_sta_stats(sta_list, &total_sta);

    // 移除原来的 if (ret <= 0) return 判断
    
    // 修改文件打开逻辑
    if (!fp) {
        fp = fopen(STA_LOG_FILE, "a+");  // 改为追加模式
        if (!fp) fp = fopen(STA_LOG_FILE, "w+");
        if (!fp) {
            perror("Failed to open STA log");
            return;
        }
        fseek(fp, 0, SEEK_END);
        current_pos = ftell(fp);
    }

    // 替换原来的循环写入逻辑
    // 新增日志轮转检查
    if (current_pos >= MAX_STA_LOG_SIZE) {
        fclose(fp);
        rename(STA_LOG_FILE, STA_LOG_FILE ".0"); // 备份旧日志
        fp = fopen(STA_LOG_FILE, "w");           // 创建新日志文件
        current_pos = 0;
        if (!fp) {
            perror("Failed to create new STA log");
            return;
        }
    }

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

    // 写入统计信息
    char header[128];
    int header_len = snprintf(header, sizeof(header), "[%s] Total STA: %d\n", timestamp, total_sta);
    fwrite(header, 1, header_len, fp);
    current_pos += header_len;

    // 写入每个STA的详细信息
    for (int i = 0; i < ret; i++) {
        char line[128];
        int line_len = snprintf(line, sizeof(line), "[%s] %s MAC:%s RSSI:%d\n",
                              timestamp, sta_list[i].vap, sta_list[i].mac, sta_list[i].rssi);
        
        // 新增行写入前的检查
        if (current_pos + line_len > MAX_STA_LOG_SIZE) {
            fclose(fp);
            rename(STA_LOG_FILE, STA_LOG_FILE ".0");
            fp = fopen(STA_LOG_FILE, "w");
            current_pos = 0;
        }
        fwrite(line, 1, line_len, fp);
        current_pos += line_len;
    }

    fflush(fp);
    // 移除原来的ftruncate调用
}

// 修改collect_sta_stats函数
int collect_sta_stats(struct sta_info *sta_list, int *total) {
    const char *target_vaps[TARGET_VAP_COUNT] = TARGET_VAPS;
    int count = 0;
    *total = 0;

    for (int i = 0; i < TARGET_VAP_COUNT; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/sta_info", target_vaps[i]);
        
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char line[256];
        int vap_total = 0;
        int current_aid = -1;
        int has_sta = 0;  // 新增标记是否有STA记录
        
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "Total user nums:")) {
                sscanf(line, "Total user nums: %d", &vap_total);
                *total += vap_total;
            }
            else if (sscanf(line, "%d: aid: %d", &current_aid) == 1) {
                // 新的STA记录开始
                if (count < MAX_STA) {
                    strncpy(sta_list[count].vap, target_vaps[i], 5);
                    has_sta = 1;  // 标记存在STA记录
                }
            }
            else if (count < MAX_STA && current_aid != -1) {
                if (strstr(line, "MAC ADDR:")) {
                    sscanf(line, " MAC ADDR: %17s", sta_list[count].mac);
                }
                else if (strstr(line, "RSSI:")) {
                    sscanf(line, " RSSI: %d", &sta_list[count].rssi);
                    count++;
                    current_aid = -1;
                }
            }
        }
        fclose(fp);
        
        // 新增：处理有总数但无详细记录的情况
        if (vap_total > 0 && !has_sta) {
            *total += vap_total;  // 确保总数正确
        }
    }
    return count;
}

static int compare_sta_rssi(const void *a, const void *b) {
    const struct {
        HI_EmMac staMac;
        int rssi;
        int uplinkrate;
        int downlinkrate;
    } *sta_a = a, *sta_b = b;
    return sta_a->rssi - sta_b->rssi;
}

int GetNetworkTopology(EmNetworkTopology *topology)
{
    igdEmTopoDevList topoList = {0};
    igdEmApTopology topoInfo[HI_EM_DEVNUM_MAX] = {0};
    igdEmApWebTopology webTopo[HI_EM_DEVNUM_MAX] = {0};
    IgdWLANEasymeshAttrCfgTab emAttr = {0};
    int ret;
    unsigned int index;
    int i, j ;

    if(!topology )
    {
        return -1;
    }

    ret = igdCmConfGet(IGD_EM_CTRL_TOPOLOGY_QUERY_LIST, (unsigned char *)&topoList, sizeof(topoList));
    if (ret != 0) {
        printf("ret igdCmConfGet IGD_EM_CTRL_TOPOLOGY_QUERY_LIST %d \r\n", ret);
        return -1;
    }

    // printf("cm easymesh topologyTbl.ulApDevNum  : %d \r\n", topoList.ulDevNum);
    for (index = 0; index < topoList.ulDevNum; index++) {
        HI_OS_MEMCPY_S((void *)topoInfo[index].ucAlMacAddr, sizeof(topoInfo[index].ucAlMacAddr),
                       &topoList.ucAlMac[index * HI_EM_MAC_LEN], sizeof(char) * HI_EM_MAC_LEN);
        ret = igdCmConfGet(IGD_EM_CTRL_TOPOLOGY_QUERY_DEV, (unsigned char *)&topoInfo[index], sizeof(topoInfo[index]));
        if (ret != 0) {
            printf("ret igdCmConfGet IGD_EM_CTRL_TOPOLOGY_QUERY_DEV %d \r\n", ret);
            return -1;
        }
        //将数据复制到 topology中
        HI_OS_MEMCPY_S((void *)topology->apList[index].apMac.mac, sizeof(topology->apList[index].apMac.mac),
                       &topoInfo[index].ucAlMacAddr, sizeof(char) * HI_EM_MAC_LEN);
        topology->apList[index].apUplinkrate = topoInfo[index].ulUplinkRate;
        topology->apList[index].apDownlinkrate = topoInfo[index].ulDownlinkRate;
        
    }
    topology->apCount = topoList.ulDevNum;

    for (i = 0; i < topoList.ulDevNum; i++) {
        igdEmApClientInfo clientInfo = {0};
        HI_OS_MEMCPY_S((void *)clientInfo.ucAlMac, sizeof(clientInfo.ucAlMac),
                       &topoInfo[i].ucAlMacAddr, sizeof(char) * HI_EM_MAC_LEN);

        ret = igdCmConfGet(IGD_EM_CTRL_TOPOLOGY_QUERY_CLIENT, (unsigned char *)&clientInfo, sizeof(clientInfo));
        if (ret != 0) {
            printf("ret igdCmConfGet IGD_EM_CTRL_TOPOLOGY_QUERY_CLIENT %d \r\n", ret);
            return -1;
        }
       topology->apList[i].staCount = clientInfo.usClientNum;

       for(j = 0 ; j < clientInfo.usClientNum ; j++)
       {
           HI_OS_MEMCPY_S((void *)topology->apList[i].staList[j].staMac.mac, sizeof(topology->apList[i].staList[j].staMac.mac),
                       &clientInfo.stClientTbl[j].ucClientMac[0], sizeof(char) * HI_EM_MAC_LEN);
           topology->apList[i].staList[j].rssi = clientInfo.stClientTbl[j].slUplinkRssi;
           topology->apList[i].staList[j].uplinkrate = clientInfo.stClientTbl[j].ulUplinkRate;
           topology->apList[i].staList[j].downlinkrate = clientInfo.stClientTbl[j].ulDownlinkRate;
       }
    }

    // 排序sta, 依据rssi 从低到高
    for (i = 0; i < topology->apCount; i++) {
        // 对每个AP下的STA列表按RSSI排序
        qsort(topology->apList[i].staList, 
              topology->apList[i].staCount,
              sizeof(topology->apList[i].staList[0]),
              compare_sta_rssi);
    }
    return 0;

}
int write_sta_rssi_log()
{
    static FILE *fp = NULL;
    static long current_pos = 0;
    EmNetworkTopology topo = {0};
    int ret = GetNetworkTopology(&topo);
    if (ret != 0) {
        return ret;
    }

    // 初始化文件指针
    if (!fp) {
        fp = fopen(STA_RSSI_LOG, "a+");
        if (!fp) fp = fopen(STA_RSSI_LOG, "w+");
        if (!fp) {
            perror("Failed to open STA RSSI log");
            return -1;
        }
        fseek(fp, 0, SEEK_END);
        current_pos = ftell(fp);
    }

    // 日志轮转检查
    if (current_pos >= MAX_RSSI_LOG_SIZE) {
        fclose(fp);
        rename(STA_RSSI_LOG, STA_RSSI_LOG ".0");
        fp = fopen(STA_RSSI_LOG, "w");
        current_pos = 0;
        if (!fp) {
            perror("Failed to create new RSSI log");
            return -1;
        }
    }

    // 写入日志内容（保持原有输出逻辑）
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

    // 计算需要写入的内容长度
    char header[256];  // 增大header缓冲区大小
    int total_sta = 0;
    for (int i = 1; i < topo.apCount; i++) {
        total_sta += topo.apList[i].staCount;
    }
    
    int header_len = snprintf(header, sizeof(header), 
                            "\n[%s] STA RSSI Report: APs=%d, STAs=%d\n", 
                            timestamp, topo.apCount - 1, total_sta);
    
    // 写入前检查空间
    if (current_pos + header_len > MAX_RSSI_LOG_SIZE) {
        fclose(fp);
        rename(STA_RSSI_LOG, STA_RSSI_LOG ".0");
        fp = fopen(STA_RSSI_LOG, "w");
        current_pos = 0;
    }

    fwrite(header, 1, header_len, fp);
    current_pos += header_len;

    for (int i = 1; i < topo.apCount; i++) {
        char apHeader[128];
        int ap_len = snprintf(apHeader, sizeof(apHeader), "AP[%d] MAC: %02X:%02X:%02X:%02X:%02X:%02X STAs=%d, ULRate:%d, DLRate:%d\n",
                            i,
                            topo.apList[i].apMac.mac[0], topo.apList[i].apMac.mac[1],
                            topo.apList[i].apMac.mac[2], topo.apList[i].apMac.mac[3],
                            topo.apList[i].apMac.mac[4], topo.apList[i].apMac.mac[5],
                            topo.apList[i].staCount,
                            topo.apList[i].apUplinkrate,
                            topo.apList[i].apDownlinkrate);
        
        // 动态检查写入空间
        if (current_pos + ap_len > MAX_RSSI_LOG_SIZE) {
            fclose(fp);
            rename(STA_RSSI_LOG, STA_RSSI_LOG ".0");
            fp = fopen(STA_RSSI_LOG, "w");
            current_pos = 0;
        }
        
        fwrite(apHeader, 1, ap_len, fp);
        current_pos += ap_len;
        // printf("%s", apHeader);

        for (int j = 0; j < topo.apList[i].staCount; j++) {
            char staEntry[256];
            int sta_len = snprintf(staEntry, sizeof(staEntry),
                                 "  STA MAC: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, ULRate: %d, DLRate:%d \n",
                                 topo.apList[i].staList[j].staMac.mac[0],
                                 topo.apList[i].staList[j].staMac.mac[1],
                                 topo.apList[i].staList[j].staMac.mac[2],
                                 topo.apList[i].staList[j].staMac.mac[3],
                                 topo.apList[i].staList[j].staMac.mac[4],
                                 topo.apList[i].staList[j].staMac.mac[5],
                                 topo.apList[i].staList[j].rssi, 
                                 topo.apList[i].staList[j].uplinkrate, 
                                 topo.apList[i].staList[j].downlinkrate);
            
            // 动态检查写入空间
            if (current_pos + sta_len > MAX_RSSI_LOG_SIZE) {
                fclose(fp);
                rename(STA_RSSI_LOG, STA_RSSI_LOG ".0");
                fp = fopen(STA_RSSI_LOG, "w");
                current_pos = 0;
            }
            
            fwrite(staEntry, 1, sta_len, fp);
            current_pos += sta_len;
            // printf("%s", staEntry);
        }
    }

    fflush(fp);
    return 0;
}


