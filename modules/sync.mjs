import fs from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';

const SYNC_CONFIG = [
    { folder: 'reinforcement-learning', targetName: 'RL' },
    { folder: 'cluster', targetName: 'Cluster' },
    { folder: 'compute', targetName: 'Compute' },
    { folder: 'insight_web_agent', targetName: 'InsightWebAgent' },
    { folder: 'leaks', targetName: 'Leaks' },
    { folder: 'memory', targetName: 'Memory' },
    { folder: 'operator', targetName: 'Operator' },
    { folder: 'statistic', targetName: 'Statistic' },
    { folder: 'timeline', targetName: 'Timeline' },
];

const __dirname = path.resolve();
const FRAMEWORK_PLUGINS_ROOT = path.join(__dirname, 'framework/public/plugins');

async function sync() {
    console.log('开始分发构建产物...');

    for (const item of SYNC_CONFIG) {
        const sourceDir = path.join(__dirname, item.folder, 'build');
        const targetDir = path.join(FRAMEWORK_PLUGINS_ROOT, item.targetName);

        try {
            // 1. 检查源目录是否存在
            if (existsSync(sourceDir)) {

                // 2. 清空并创建目标目录
                await fs.rm(targetDir, { recursive: true, force: true });
                await fs.mkdir(targetDir, { recursive: true });

                // 3. 递归拷贝
                // Node 16.7.0+ 支持 fs.cp
                await fs.cp(sourceDir, targetDir, {
                    recursive: true,
                    force: true
                });

                console.log(`✅ 已同步: ${item.folder} -> ${item.targetName}`);
            } else {
                console.warn(`⚠️ 跳过: ${item.folder} (未找到 build 目录: ${sourceDir})`);
            }
        } catch (err) {
            console.error(`❌ 同步 ${item.folder} 失败:`, err);
        }
    }
}

sync();
