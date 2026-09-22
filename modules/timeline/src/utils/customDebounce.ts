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
type Callback<T extends unknown[], K> = (...args: T) => Promise<K>;
export const customDebounce = function<T extends unknown[], K>(callback: Callback<T, K>): Callback<T, K> {
    let waitingTask: ReturnType<Callback<T, K>> | null = null;
    const taskQueue: Array<(value: K) => void> = [];
    const runCallback = (...args: T): ReturnType<Callback<T, K>> => {
        const task = callback(...args);
        task.catch(() => {
            if (waitingTask === task) {
                waitingTask = null;
            }
        });
        return task;
    };
    return (...args) => {
        if (waitingTask === null) {
            waitingTask = runCallback(...args);
            return waitingTask;
        }
        return new Promise((resolve, reject) => {
            taskQueue.push(resolve);
            waitingTask?.then((res) => {
                if (resolve !== taskQueue[taskQueue.length - 1]) {
                    const index = taskQueue.indexOf(resolve);
                    resolve(res);
                    taskQueue.splice(0, index + 1);
                    return;
                }
                waitingTask = runCallback(...args);
                return waitingTask.then((finalRes) => {
                    const index = taskQueue.indexOf(resolve);
                    resolve(finalRes);
                    taskQueue.splice(0, index + 1);
                });
            })
                .catch(e => {
                    const index = taskQueue.indexOf(resolve);
                    if (index >= 0) {
                        taskQueue.splice(index, 1);
                    }
                    reject(e);
                });
        });
    };
};
