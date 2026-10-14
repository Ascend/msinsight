/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const { pathToFileURL } = require('node:url');

// Preserve the file URL in Jest's CommonJS runtime; Worker execution still needs a real browser or explicit fake.
module.exports = ({ types }) => ({
    visitor: {
        MemberExpression(nodePath, state) {
            const { object, property, computed } = nodePath.node;
            if (types.isMetaProperty(object) && object.meta.name === 'import' && object.property.name === 'meta' &&
                !computed && types.isIdentifier(property, { name: 'url' })) {
                nodePath.replaceWith(types.stringLiteral(pathToFileURL(state.filename).href));
            }
        },
    },
});
