/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import { appendFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { inspect } from "node:util";

let logFilePath;

const formatDate = (date) => {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, "0");
    const day = String(date.getDate()).padStart(2, "0");
    return `${year}${month}${day}`;
};

const formatMessage = (values) => values.map((value) => {
    if (typeof value === "string") return value;
    return inspect(value, { depth: 5, breakLength: Infinity });
}).join(" ");

const writeLog = (level, values) => {
    if (!logFilePath) return;
    const timestamp = new Date().toISOString();
    appendFileSync(logFilePath, `[${timestamp}] [${level}] ${formatMessage(values)}\n`, "utf8");
};

export const initLogger = ({ rootDir, port }) => {
    mkdirSync(rootDir, { recursive: true });
    logFilePath = join(rootDir, `insight_web_agent_${formatDate(new Date())}_${port}.log`);

    console.log = (...values) => writeLog("INFO", values);
    console.info = (...values) => writeLog("INFO", values);
    console.warn = (...values) => writeLog("WARN", values);
    console.error = (...values) => writeLog("ERROR", values);
    console.debug = (...values) => writeLog("DEBUG", values);
};

export const getLogFilePath = () => logFilePath;
