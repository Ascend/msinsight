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
import assert from "node:assert/strict";
import { mkdtemp, mkdir, realpath, rm, symlink } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import test from "node:test";
import { createFilesystemPolicyService } from "../../native-agent/config/filesystemPolicy.mjs";
import { isAllowedFilesystemPath } from "../../native-agent/shared/utils.mjs";

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-fs-"));
    const workspace = join(root, "workspace");
    const resourceDir = join(root, "resources");
    const docs = join(resourceDir, "docs");
    const skills = join(resourceDir, "skills");
    const extra = join(root, "extra");
    const project = join(root, "project");
    const sibling = join(root, "project-sibling");
    await Promise.all([workspace, docs, skills, extra, project, sibling].map((path) => mkdir(path, { recursive: true })));
    const filesystem = createFilesystemPolicyService({
        cwd: workspace,
        env: {
            INSIGHT_WEB_AGENT_RESOURCE_DIR: resourceDir,
            INSIGHT_WEB_AGENT_FILESYSTEM_POLICY: JSON.stringify({
                includeDocsRoot: true,
                projectRoot: resourceDir,
                extraPaths: [extra],
            }),
        },
    });
    return { root, workspace, resourceDir, docs, skills, extra, project, sibling, filesystem };
};

test("initial roots contain workspace, docs, skills, and explicit extras but no default project root", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));

    assert.deepEqual(fixture.filesystem.createSessionFilesystemRoots(), [
        fixture.workspace,
        fixture.docs,
        fixture.skills,
        fixture.extra,
    ]);
    assert.equal(fixture.filesystem.policy.projectRoot, undefined);
    assert.equal(fixture.filesystem.createSessionFilesystemRoots().includes(fixture.resourceDir), false);
});

test("missing or malformed policy does not enable the raw docs root", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-fs-defaults-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const workspace = join(root, "workspace");
    const resourceDir = join(root, "resources");
    await Promise.all([
        mkdir(workspace, { recursive: true }),
        mkdir(join(resourceDir, "docs"), { recursive: true }),
        mkdir(join(resourceDir, "skills"), { recursive: true }),
    ]);

    for (const serializedPolicy of [undefined, "{"]) {
        const filesystem = createFilesystemPolicyService({
            cwd: workspace,
            env: {
                INSIGHT_WEB_AGENT_RESOURCE_DIR: resourceDir,
                ...(serializedPolicy === undefined ? {} : { INSIGHT_WEB_AGENT_FILESYSTEM_POLICY: serializedPolicy }),
            },
        });
        assert.equal(filesystem.policy.includeDocsRoot, false);
        assert.equal(filesystem.createSessionFilesystemRoots().includes(join(resourceDir, "docs")), false);
        assert.equal(filesystem.createSessionFilesystemRoots().includes(join(resourceDir, "skills")), true);
    }
});

test("project updates require an absolute existing directory and store its canonical path", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const session = {
        projectRoot: undefined,
        filesystemRoots: fixture.filesystem.createSessionFilesystemRoots(),
        canonicalFilesystemRoots: [],
    };

    assert.equal(await fixture.filesystem.updateSessionFilesystemRoots(session, fixture.project), true);
    const canonicalProject = await realpath(fixture.project);
    assert.equal(session.projectRoot, canonicalProject);
    assert.equal(session.filesystemRoots.includes(canonicalProject), true);

    assert.equal(await fixture.filesystem.updateSessionFilesystemRoots(session, "relative/project"), true);
    assert.equal(session.projectRoot, undefined);
    assert.deepEqual(session.filesystemRoots, fixture.filesystem.createSessionFilesystemRoots());

    assert.equal(await fixture.filesystem.updateSessionFilesystemRoots(session, join(fixture.root, "missing")), false);
    assert.equal(await fixture.filesystem.updateSessionFilesystemRoots(session, undefined), false);
});

test("project symlinks store the canonical target when the token has symlink capability", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const projectLink = join(fixture.root, "project-link");
    if (!await createDirectorySymlinkOrSkip(t, fixture.project, projectLink)) return;
    const session = {
        projectRoot: undefined,
        filesystemRoots: fixture.filesystem.createSessionFilesystemRoots(),
        canonicalFilesystemRoots: [],
    };

    assert.equal(await fixture.filesystem.updateSessionFilesystemRoots(session, projectLink), true);
    assert.equal(session.projectRoot, await realpath(fixture.project));
});

test("canonical roots reject sibling-prefix escapes", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const canonicalRoots = await fixture.filesystem.canonicalizeFilesystemRoots([fixture.project]);

    assert.equal(await isAllowedFilesystemPath(join(fixture.project, "inside.txt"), canonicalRoots), true);
    assert.equal(await isAllowedFilesystemPath(join(fixture.sibling, "outside.txt"), canonicalRoots), false);
    assert.equal(await isAllowedFilesystemPath(resolve(`${fixture.project}-sibling`, "outside.txt"), canonicalRoots), false);
});

test("canonical roots reject symlink escapes when the token has symlink capability", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const escapeLink = join(fixture.project, "escape");
    if (!await createDirectorySymlinkOrSkip(t, fixture.sibling, escapeLink)) return;
    const canonicalRoots = await fixture.filesystem.canonicalizeFilesystemRoots([fixture.project]);

    assert.equal(await isAllowedFilesystemPath(join(escapeLink, "outside.txt"), canonicalRoots), false);
});

const createDirectorySymlinkOrSkip = async (t, target, path) => {
    try {
        await symlink(target, path, "dir");
        return true;
    } catch (error) {
        if (!["EPERM", "EACCES"].includes(error?.code)) throw error;
        t.skip(`symlink capability unavailable: ${error.code}`);
        return false;
    }
};
