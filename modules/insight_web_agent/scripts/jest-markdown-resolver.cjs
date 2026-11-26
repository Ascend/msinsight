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
const { createRequire } = require('node:module');
const { join } = require('node:path');

module.exports = (request, options) => {
    try {
        return options.defaultResolver(request, options);
    } catch (error) {
        // Jest 27 does not resolve these package imports and export subpaths.
        if (request.startsWith('#min') || request.startsWith('entities/') || request.startsWith('unist-util-visit-parents/')) {
            return createRequire(join(options.basedir, 'package.json')).resolve(request);
        }
        throw error;
    }
};
