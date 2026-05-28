/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_TRACKINFOMANAGER_H
#define PROFILER_SERVER_TRACKINFOMANAGER_H
#include <string>
#include <mutex>
#include <unordered_map>
#include <set>
#include <map>
#include <vector>
#include "ProjectParserFactory.h"
namespace Dic::Module::Timeline {
struct TrackInfo {
    /* *
     * host信息，代表具体的机器
     */
    std::string host;
    /* *
     * 数据库db文件路径唯一标识
     */
    std::string cardId;
    /* *
     * 显卡设备全局id
     */
    std::string rankId;
    /* *
     * 单机显卡设备id
     */
    std::string deviceId;
    /* *
     * 进程信息
     */
    std::string processId;
    /* *
     * 线程信息
     */
    std::string threadId;
    /* *
     * 线程泳道唯一标识
     */
    uint64_t trackId = 0;
};
class TrackInfoManager {
  public:
    static TrackInfoManager &Instance() {
        static TrackInfoManager instance;
        return instance;
    }
    TrackInfoManager(const TrackInfoManager &) = delete;
    TrackInfoManager &operator=(const TrackInfoManager &) = delete;
    TrackInfoManager(TrackInfoManager &&) = delete;
    TrackInfoManager &operator=(TrackInfoManager &&) = delete;
    uint64_t GetTrackId(const std::string &cardId, const std::string &pid, const std::string &tid);
    bool GetTrackInfo(uint64_t trackId, TrackInfo &trackInfo, const std::string &fileId);
    void UpdateHost(const std::string &cardId, const std::string &host);
    void UpdateHostCardId(const std::string &cardId, const std::string &hostCardId);
    void UpdateDeviceMap(const std::string &cardId, const std::unordered_map<std::string, std::string> deviceMap);
    void UpdateDeviceToRankIdMap(const std::string &cardId, const std::string &rankId);
    void UpdateTrackIdMap(
        const std::string &fileId, const std::map<uint64_t, std::pair<std::string, std::string>> &threadMap);
    std::string GetRankId(const std::string &host, const std::string &deviceId);
    void Reset();
    std::string GetDeviceId(const std::string &cardId);
    void UpdateClusterDbToFileIdMap(const std::string &clusterDb, const std::string &fileId);
    std::map<std::string, std::string> GetRankIdToFileIdByClusterDb(const std::string &clusterDb);

    std::vector<RankInfo> GetRankListByFileId(const std::string &fileId, const std::string &rankId);

    void SetRankListByFileId(const std::string &fileId, const RankInfo &rank);

    std::string GetClusterByFileId(const std::string &fileId);

    void SetClusterByFileId(const std::string &fileId, const std::string &cluster);

    std::string GetFileIdByClusterDbAndRankId(const std::string &clusterDb, const std::string &rankId);

    void AddRankToCluster(const std::string &clusterId, const std::string &rank);

    std::string GetRankInCluster(const std::string &clusterId, const std::string &rank);

  private:
    TrackInfoManager() = default;
    ~TrackInfoManager() = default;
    std::mutex trackMutex;
    /* *
     * 键是cardID,用于区分不同的显卡，值是进程线程以及虚拟的的trackId
     */
    std::unordered_map<std::string, std::map<std::pair<std::string, std::string>, uint64_t>> trackIdMap;
    /* *
     * 键是cardID,数据库db文件路径唯一标识，值是显卡对应的host信息
     */
    std::unordered_map<std::string, std::string> hostMap;
    /* *
     * 键是cardID,数据库db文件路径唯一标识，值是rankId和deviceId的对应关系
     */
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> deviceMap;
    /* *
     * 键是trackId
     */
    std::unordered_multimap<uint64_t, TrackInfo> trackInfoMap;
    /* *
     * 键是cardID,数据库db文件路径唯一标识，值是显卡对应的host的cardId,host的cardId和db文件路径相同
     */
    std::unordered_map<std::string, std::string> hostCardIdMap;
    /* *
     * 键是host + deviceId拼接，值是rankId，显卡全局唯一id；
     */
    std::unordered_map<std::string, std::string> deviceIdToRankIdMap;

    /**
     * 集群db到rankId的映射
     */
    std::unordered_map<std::string, std::set<std::string>> clusterDbToFileIdMap;

    /**
     * @brief 键是fileId, 值是rankList
     */
    std::unordered_map<std::string, std::vector<RankInfo>> fileIdToRankListMap;

    /**
     * @brief 键是fileId,值是cluster
     */
    std::unordered_map<std::string, std::string> fileIdToClusterMap;

    /**
     * @brief key: clusterPath   value: rankSet , used in communication and summary
     */
    std::unordered_map<std::string, std::set<std::string>> clusterRankSetMap;

    uint64_t maxTrackId = 0;
    std::string GetRankId(const std::string &cardId);
    std::string GetHost(const std::string &cardId);
};
}
#endif // PROFILER_SERVER_TRACKINFOMANAGER_H
