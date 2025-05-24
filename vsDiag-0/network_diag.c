#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include "sta_diag.h"
#include "network_diag.h"

static int debug_enabled = 0;
static struct network_stats prev_net = {0};
static struct cpu_stats prev_cpu = {0};

int get_network_rate(const char *interface, double *rx_rate, double *tx_rate,
                     double *rx_drop_rate, double *tx_drop_rate)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp)
    {
        if (debug_enabled)
            perror("open /proc/net/dev failed");
        return -1;
    }

    char line[256];
    unsigned long long rx = 0, tx = 0, rx_drop = 0, tx_drop = 0;
    int found = 0;

    while (fgets(line, sizeof(line), fp))
    {
        char *iface = strtok(line, ":");
        if (!iface)
            continue;
        while (*iface == ' ')
            iface++;

        if (strcmp(iface, interface) == 0)
        {
            char *data = line + strlen(interface) + 2;
            // 修改解析逻辑以适应vap接口格式
            if (sscanf(data, "%llu %*u %*u %llu %*u %*u %*u %*u %llu %*u %*u %llu %*u %*u %*u %*u",
                       &rx, &rx_drop, &tx, &tx_drop) < 4)
            {
                // 尝试备用解析格式
                if (sscanf(data, "%llu %*u %*u %llu %*u %*u %*u %*u %llu",
                           &rx, &rx_drop, &tx) < 3)
                {
                    if (debug_enabled)
                        printf("[network] failed to parse %s data\n", interface);
                    fclose(fp);
                    return -1;
                }
                tx_drop = 0; // 如果无法解析tx_drop，设为0
            }
            found = 1;
            break;
        }
    }
    fclose(fp);

    if (!found)
    {
        if (debug_enabled)
            printf("[network] interface %s not found\n", interface);
        return -1;
    }

    time_t now = time(NULL);
    if (prev_net.last_time == 0)
    {
        if (debug_enabled)
            printf("[network] initializing %s stats\n", interface);
        prev_net.rx_bytes = rx;
        prev_net.tx_bytes = tx;
        prev_net.last_time = now;
        *rx_rate = *tx_rate = 0.0;
        return 0;
    }

    double time_diff = difftime(now, prev_net.last_time);
    if (time_diff <= 0)
    {
        if (debug_enabled)
            printf("[network] time difference invalid: %.2f\n", time_diff);
        return 0;
    }

    if (debug_enabled)
    {
        printf("[network] %s rate calculated (RX=%.2fB/s TX=%.2fB/s)\n",
               interface, *rx_rate, *tx_rate);
    }

    *rx_rate = (rx - prev_net.rx_bytes) / time_diff;
    *tx_rate = (tx - prev_net.tx_bytes) / time_diff;

    // 计算丢包率
    if (prev_net.rx_bytes > 0 && prev_net.rx_dropped > 0)
    {
        *rx_drop_rate = (double)(rx_drop - prev_net.rx_dropped) /
                        (double)(rx - prev_net.rx_bytes + rx_drop - prev_net.rx_dropped);
    }
    else
    {
        *rx_drop_rate = 0.0;
    }

    if (prev_net.tx_bytes > 0 && prev_net.tx_dropped > 0)
    {
        *tx_drop_rate = (double)(tx_drop - prev_net.tx_dropped) /
                        (double)(tx - prev_net.tx_bytes + tx_drop - prev_net.tx_dropped);
    }
    else
    {
        *tx_drop_rate = 0.0;
    }

    // 更新丢包统计
    prev_net.rx_dropped = rx_drop;
    prev_net.tx_dropped = tx_drop;
    prev_net.rx_bytes = rx;
    prev_net.tx_bytes = tx;
    prev_net.last_time = now;
    return 0;
}

double get_cpu_usage()
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1.0;

    char line[256];
    unsigned long long user, nice, system, idle, iowait;

    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "cpu ", 4) == 0)
        {
            sscanf(line + 5, "%llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait);
            break;
        }
    }
    fclose(fp);

    unsigned long long total = user + nice + system + idle + iowait;
    unsigned long long idle_total = idle + iowait;

    time_t now = time(NULL);
    if (prev_cpu.last_time == 0)
    {
        prev_cpu.total = total;
        prev_cpu.idle = idle_total;
        prev_cpu.last_time = now;
        return 0.0;
    }

    double time_diff = difftime(now, prev_cpu.last_time);
    if (time_diff <= 0)
        return 0.0;

    unsigned long long total_diff = total - prev_cpu.total;
    unsigned long long idle_diff = idle_total - prev_cpu.idle;

    if (total_diff == 0)
        return 0.0;

    double usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
    prev_cpu.total = total;
    prev_cpu.idle = idle_total;
    prev_cpu.last_time = now;

    return usage;
}

// MemAvailable
//  修改内存检测函数
int get_memory_usage(unsigned long *available, unsigned long *total)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;

    char line[256];
    *total = *available = 0;
    unsigned long cached = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "MemTotal: %lu kB", total) == 1)
            continue;
        if (sscanf(line, "MemAvailable: %lu kB", available) == 1)
            continue;
        if (sscanf(line, "Cached: %lu kB", &cached) == 1)
            continue;
    }
    fclose(fp);

    // 当MemAvailable不存在时（旧内核）使用备用计算方式
    if (*available == 0 && cached != 0)
    {
        *available = cached; // 简单回退方案
    }
    return (*total == 0) ? -1 : 0;
}

// 统一的时间检查函数
static int should_check(struct timer_ctx *ctx, time_t now)
{
    return difftime(now, ctx->last_check) >= (double)ctx->interval;
}

// 拆分网络状态检测模块
static int check_connection_impl(const char *server)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c %d -W %d %s > /dev/null 2>&1",
             PING_RETRY, PING_TIMEOUT, server);
    int status = system(cmd);
    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

// 重构后的互联网检测函数
int check_internet_connection()
{
    return check_connection_impl(PING_SERVER1) ||
           check_connection_impl(PING_SERVER2);
}

// 修改write_log函数以适应新格式
void write_log(const char *if_info, double cpu_usage, unsigned long mem_available, unsigned long mem_total,
               int internet_status, int gateway_status) {
    static FILE *fp = NULL;
    static long current_pos = 0;

    // 文件管理模块化
    if (!fp)
    {
        if (!(fp = fopen(LOG_FILE, "a+")))
            fp = fopen(LOG_FILE, "w+");
        if (!fp)
        {
            perror("Failed to open log file");
            return;
        }
        fseek(fp, 0, SEEK_END);
        current_pos = ftell(fp);
    }

    // 时间处理模块化
    time_t now = time(NULL);
    struct tm tm;
    char timestamp[20];
    localtime_r(&now, &tm);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    // 简化格式生成
    const char *prefix = internet_status ? "" : "!!!";

    // 添加内存预警逻辑
    if (mem_available < 25 * 1024)
    { // 25MB阈值（单位kB）
        prefix = "!!!";
    }

    char line[MAX_LINE_LENGTH];
    // 修改日志写入函数参数
    int len = snprintf(line, sizeof(line),
        "%s[%s] %s CPU=%.2f%% Mem=%.1f/%.1fMB NET=%d GW_NET=%d\n",
        prefix, timestamp, if_info,
        cpu_usage, 
        mem_available / 1024.0,
        mem_total / 1024.0,
        internet_status, gateway_status);

    // 边界检查标准化
    len = (len >= MAX_LINE_LENGTH) ? MAX_LINE_LENGTH - 1 : len;
    line[len] = '\0';

    // 新增日志轮转逻辑
    if (current_pos + len > MAX_LOG_SIZE)
    {
        if (fp)
        {
            fclose(fp);
            fp = NULL;
            rename(LOG_FILE, LOG_FILE ".0"); // 重命名旧日志
        }
        current_pos = 0;
    }

    if (!fp)
    {
        fp = fopen(LOG_FILE, "w"); // 创建新日志文件
        if (!fp)
        {
            perror("Failed to create log file");
            return;
        }
    }

    fwrite(line, 1, len, fp);
    fflush(fp);
    current_pos += len; // 更新当前文件位置
}

// 重构后的主循环逻辑
// 检查是否缺少main函数闭合大括号
// 新增备份和上传函数
// 修改备份上传函数签名
static void backup_and_upload(const char *ftp_ip, int upload_meminfo)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    // 生成时间戳字符串
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &tm);

    // 修改上传路径到服务器根目录并简化文件名
    char cmd[512];
    // 上传agent日志
    snprintf(cmd, sizeof(cmd),
             "find /tmp -maxdepth 1 -name 'em_agent.log.txt*' -type f "
             "-exec sh -c 'f={}; ftpput %s /$(basename $f)_%s $f' \\;", // 修正转义符
             ftp_ip, timestamp);
    system(cmd);

    // 上传controller日志
    snprintf(cmd, sizeof(cmd),
             "find /tmp -maxdepth 1 -name 'em_controller.log.txt*' -type f "
             "-exec sh -c 'f={}; ftpput %s /$(basename $f)_%s $f' \\;", // 修正转义符
             ftp_ip, timestamp);
    system(cmd);

    if (upload_meminfo)
    {
        // 新增内存信息上传逻辑
        char meminfo_path[256];
        snprintf(meminfo_path, sizeof(meminfo_path), "/tmp/vs_diag_dir/meminfo_%s.txt", timestamp);

        // 生成内存信息文件
        char copy_cmd[256];
        snprintf(copy_cmd, sizeof(copy_cmd), "cp /proc/meminfo %s", meminfo_path);
        system(copy_cmd);

        // 上传内存信息文件
        char meminfo_cmd[512];
        snprintf(meminfo_cmd, sizeof(meminfo_cmd),
                 "ftpput %s /meminfo_%s.txt %s",
                 ftp_ip, timestamp, meminfo_path);
        system(meminfo_cmd);

        // 清理临时文件
        remove(meminfo_path);
    }
}

// 在全局变量区新增
#include <signal.h>
volatile sig_atomic_t upload_flag = 0;

// 新增信号处理函数
static void handle_signal(int sig) {
    upload_flag = 1;
}

// 在main函数开始处添加信号处理注册
int main(int argc, char *argv[]) {
    // 添加信号处理
    signal(SIGUSR2, handle_signal);
    
    char *interfaces[MAX_INTERFACES] = {"eth0", NULL}; // 默认只监控eth0
    int interface_count = 1;
    int pid = -1;
    char *ftp_ip = NULL;
    int opt = 0;
    // 修改选项处理
    static struct option long_options[] = {
        {"debug", no_argument, &debug_enabled, 1},
        {"interface", required_argument, 0, 'i'},
        {"server", required_argument, 0, 's'},
        {"pid", required_argument, 0, 'p'},
        {"log-interval", required_argument, 0, 'l'},
        {"log-sta-rssi", no_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int internet_status = 0;
    int current_status = 0;
    int prev_internet_status = 1; // 新增状态跟踪变量
    int log_interval = SYSTEM_LOG_INTERVAL;
    int log_sta_rssi = 0;
    char *token = NULL;
    // 解析命令行选项
    while ((opt = getopt_long(argc, argv, "dh:i:s:p:l:r:", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'd':
            debug_enabled = 1;
            break;
        case 'i':
            token = strtok(optarg, ",");
            interface_count = 0;
            while (token && interface_count < MAX_INTERFACES)
            {
                interfaces[interface_count++] = token;
                token = strtok(NULL, ",");
            }
            break;
        case 's':
            ftp_ip = optarg;
            break;
        case 'p':
            pid = atoi(optarg);
            break;
        case 'l':
            log_interval = atoi(optarg);
            if (log_interval <= 0)
            {
                fprintf(stderr, "Invalid log interval: %d, using default %d\n",
                        log_interval, SYSTEM_LOG_INTERVAL);
                log_interval = SYSTEM_LOG_INTERVAL;
            }
            break;
        case 'r':
            log_sta_rssi = atoi(optarg);
            break;
        case 'h':
        default:
            // 修改选项帮助信息
            printf("Usage: %s [OPTIONS]\n"
                   "Options:\n"
                   "  -d, --debug         Enable debug mode\n"
                   "  -i, --interface ETH Network interface (default: eth0)\n"
                   "  -l, --log-interval SEC  System log interval (default: %d)\n"
                   "  -s, --server IP     FTP server IP (optional)\n"
                   "  -p, --pid PID       Monitor process PID (optional)\n" // 修改为optional
                   "  -r  --log sta rssi with interval \n"
                   "  -h, --help          Show this help\n",
                   argv[0], SYSTEM_LOG_INTERVAL);
            return 1;
        }
    }

    // 初始化时间上下文（新增每小时定时器）
    struct timer_ctx timers[] = {
        {0, PING_INTERVAL}, // 互联网检测
        {0, STA_INTERVAL},  // STA日志
        {0, log_interval},  // 系统日志
        {0, HOUR_INTERVAL}, // 新增每小时任务
        {0, log_sta_rssi}   // sta rssi 日志
    };

    // 确保备份目录存在
    system("mkdir -p /tmp/vs_diag_dir/log_backup");

    while (1)
    {
        time_t now = time(NULL);

        // 新增每小时任务处理
        if (should_check(&timers[3], now) && pid != -1)
        {
            // 发送SIGUSR2信号
            if (kill(pid, SIGUSR2) == 0)
            {
                sleep(1); // 等待1秒

                // 生成带时间戳的文件名
                char timestamp[20];
                struct tm tm;
                localtime_r(&now, &tm);
                strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &tm);

                // 上传两个日志文件
                char cmd[512];
                // 上传 easymesh.log
                snprintf(cmd, sizeof(cmd),
                         "ftpput %s /easymesh_%s.log /var/log/dmalloc/easymesh.log",
                         ftp_ip, timestamp);
                system(cmd);

                // 新增/proc状态文件上传
                char status_path[256];
                snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
                snprintf(cmd, sizeof(cmd),
                         "ftpput %s /proc_status_%s.txt %s",
                         ftp_ip, timestamp, status_path);
                system(cmd);
            }
            timers[3].last_check = now;
        }

        if (should_check(&timers[0], now))
        {
            current_status = check_internet_connection();

            if (prev_internet_status == 1 && current_status == 0)
            {
                if (ftp_ip)
                {
                    backup_and_upload(ftp_ip, 0);
                }
            }

            internet_status = current_status;
            prev_internet_status = current_status; // 更新状态跟踪
            timers[0].last_check = now;
        }

        if (should_check(&timers[2], now))
        {
            double total_rx = 0, total_tx = 0;
            char if_info[256] = {0};
            char *pos = if_info;

            for (int i = 0; i < interface_count; i++)
            {
                double rx_rate, tx_rate, rx_drop_rate, tx_drop_rate;
                if (get_network_rate(interfaces[i], &rx_rate, &tx_rate,
                                     &rx_drop_rate, &tx_drop_rate) == 0)
                {
                    total_rx += rx_rate;
                    total_tx += tx_rate;

                    // 为每个接口生成独立信息
                    pos += snprintf(pos, sizeof(if_info) - (pos - if_info),
                                    "%s: RX=%.2fMB/s(%.2f%%) TX=%.2fMB/s(%.2f%%) ",
                                    interfaces[i],
                                    rx_rate / (1024 * 1024), rx_drop_rate * 100,
                                    tx_rate / (1024 * 1024), tx_drop_rate * 100);
                }
            }

            double cpu_usage = get_cpu_usage();
            unsigned long mem_available, mem_total;
            int gateway_status = check_connection_impl(GATEWAY_IP);


            if (!get_memory_usage(&mem_available, &mem_total))
            {
                // 添加内存预警逻辑
                if (mem_available < 25 * 1024)
                { // 25MB阈值（单位kB）
                    printf("!!! LOW MEMORY ALERT: %.1fMB available !!!\n", mem_available / 1024.0);
                    if (ftp_ip)
                    {
                        backup_and_upload(ftp_ip, 1);
                    }
                }

                write_log(if_info, cpu_usage,
                 mem_available, mem_total, internet_status,
                 gateway_status);
                 
                }
            
            timers[2].last_check = now;
        }

        // if (should_check(&timers[1], now)) {
        //     write_sta_log();
        //     timers[1].last_check = now;
        // }

        if (should_check(&timers[4], now) && log_sta_rssi > 0)
        {
            write_sta_rssi_log();
            timers[1].last_check = now;
        }

        // 新增信号触发上传检查
        if (upload_flag && ftp_ip)
        {
            backup_and_upload(ftp_ip, 0);
            upload_flag = 0; // 重置标志
        }

        sleep(1);
    }
    return 0;
}
