/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { execFile } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { promisify } from "node:util";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { strict as assert } from "node:assert";
import test from "node:test";
import { createSecretCrypto, decryptEnv, envAad, isAtRestEnvelope, nativeApiKeyAad, sealRsaTransitForPublicKey } from "../../security/secretCrypto.mjs";
import { SECRET_PLACEHOLDER, projectBuiltinAgent, projectEnv } from "../../security/secretProjection.mjs";

const createCrypto = async () => {
    const dir = await mkdtemp(join(tmpdir(), "insight-secrets-"));
    return {
        dir,
        crypto: createSecretCrypto({ keyPath: join(dir, "secrets.key"), protectKey: false }),
    };
};
const execFileAsync = promisify(execFile);

test("seals api keys at rest and opens them only with matching AAD", async (t) => {
    const fixture = await createCrypto();
    t.after(() => rm(fixture.dir, { recursive: true, force: true }));

    const sealed = fixture.crypto.sealAtRest("sk-secret", nativeApiKeyAad());
    assert.equal(isAtRestEnvelope(sealed), true);
    assert.equal(fixture.crypto.openAtRest(sealed, nativeApiKeyAad()), "sk-secret");
    assert.throws(() => fixture.crypto.openAtRest(sealed, envAad("OpenCode", "TOKEN")), /corrupt|unable|Unsupported|bad decrypt|secret_ciphertext_invalid/i);
});

test("migrates plaintext secrets and rejects plaintext incoming values", async (t) => {
    const fixture = await createCrypto();
    t.after(() => rm(fixture.dir, { recursive: true, force: true }));

    const migrated = fixture.crypto.persistIfPlain("plain-key", nativeApiKeyAad());
    assert.equal(migrated.changed, true);
    assert.equal(fixture.crypto.openAtRest(migrated.value, nativeApiKeyAad()), "plain-key");
    assert.throws(
        () => fixture.crypto.resolveIncomingSecret("plain-key", migrated.value, nativeApiKeyAad()),
        { code: "plaintext_secret_rejected" },
    );
    assert.equal(
        fixture.crypto.resolveIncomingSecret(SECRET_PLACEHOLDER, migrated.value, nativeApiKeyAad()),
        migrated.value,
    );
    assert.throws(
        () => fixture.crypto.resolveIncomingSecret(SECRET_PLACEHOLDER, "", nativeApiKeyAad()),
        { code: "secret_placeholder_invalid" },
    );
    assert.equal(fixture.crypto.resolveIncomingSecret("", migrated.value, nativeApiKeyAad()), "");
});

test("unwraps an RSA-OAEP transit envelope and re-seals at rest", async (t) => {
    const fixture = await createCrypto();
    t.after(() => rm(fixture.dir, { recursive: true, force: true }));

    const envelope = sealRsaTransitForPublicKey(fixture.crypto.publicCrypto().publicKey, "rsa-secret");
    const stored = fixture.crypto.resolveIncomingSecret(envelope, "", nativeApiKeyAad());
    assert.equal(isAtRestEnvelope(stored), true);
    assert.equal(fixture.crypto.openAtRest(stored, nativeApiKeyAad()), "rsa-secret");
});

test("decryptEnv rejects ciphertext that cannot be opened", async (t) => {
    const fixture = await createCrypto();
    t.after(() => rm(fixture.dir, { recursive: true, force: true }));

    assert.throws(
        () => decryptEnv({ TOKEN: "enc:v1:bad:tag:ct", ACP_DEBUG: "1" }, "OpenCode", fixture.crypto),
        { code: "secret_ciphertext_invalid" },
    );
});

test("rejects transit envelopes sealed for a different host key", async (t) => {
    const fixture = await createCrypto();
    t.after(() => rm(fixture.dir, { recursive: true, force: true }));
    const other = createSecretCrypto({ keyPath: join(fixture.dir, "other.key"), protectKey: false });

    const envelope = sealRsaTransitForPublicKey(other.publicCrypto().publicKey, "stolen");
    assert.throws(() => fixture.crypto.unwrapTransit(envelope), { code: "crypto_unwrap_failed" });
});

test("projects configured secrets as placeholders", () => {
    assert.deepEqual(projectBuiltinAgent({ provider: "openai", apiKey: "enc:v1:x" }), {
        provider: "openai",
        apiKey: SECRET_PLACEHOLDER,
    });
    assert.deepEqual(projectEnv({ TOKEN: "enc:v1:x", PRIVATE_KEY: "enc:v1:y", ACP_DEBUG: "1" }), {
        TOKEN: SECRET_PLACEHOLDER,
        PRIVATE_KEY: SECRET_PLACEHOLDER,
        ACP_DEBUG: "1",
    });
});

test("reuses the wrapping key file across crypto instances", async (t) => {
    const dir = await mkdtemp(join(tmpdir(), "insight-secrets-reuse-"));
    t.after(() => rm(dir, { recursive: true, force: true }));
    const keyPath = join(dir, "secrets.key");
    const first = createSecretCrypto({ keyPath, protectKey: false });
    const sealed = first.sealAtRest("keep-me", nativeApiKeyAad());
    const stored = await readFile(keyPath);
    assert.equal(stored.length, 32);

    const second = createSecretCrypto({ keyPath, protectKey: false });
    assert.equal(second.openAtRest(sealed, nativeApiKeyAad()), "keep-me");
});

test("uses the explicit protection setting when reopening a protected key", async (t) => {
    if (process.platform !== "win32") return t.skip("DPAPI is only available on Windows");
    const dir = await mkdtemp(join(tmpdir(), "insight-secrets-dpapi-"));
    t.after(() => rm(dir, { recursive: true, force: true }));
    const keyPath = join(dir, "secrets.key");
    const first = createSecretCrypto({ keyPath, protectKey: true });
    const sealed = first.sealAtRest("keep-protected", nativeApiKeyAad());

    const second = createSecretCrypto({ keyPath, protectKey: true });
    assert.equal(second.openAtRest(sealed, nativeApiKeyAad()), "keep-protected");
    assert.throws(
        () => createSecretCrypto({ keyPath, protectKey: false }),
        { code: "secrets_key_invalid" },
    );
});

test("migrates an existing raw wrapping key when DPAPI protection is enabled", async (t) => {
    if (process.platform !== "win32") return t.skip("DPAPI is only available on Windows");
    const dir = await mkdtemp(join(tmpdir(), "insight-secrets-dpapi-migrate-"));
    t.after(() => rm(dir, { recursive: true, force: true }));
    const keyPath = join(dir, "secrets.key");
    const first = createSecretCrypto({ keyPath, protectKey: false });
    const sealed = first.sealAtRest("keep-after-migration", nativeApiKeyAad());

    const migrated = createSecretCrypto({ keyPath, protectKey: true });
    assert.equal(migrated.openAtRest(sealed, nativeApiKeyAad()), "keep-after-migration");
    assert.equal((await readFile(keyPath)).subarray(0, 4).toString("ascii"), "MS1D");
});

test("concurrent processes publish only one wrapping key", async (t) => {
    const dir = await mkdtemp(join(tmpdir(), "insight-secrets-race-"));
    t.after(() => rm(dir, { recursive: true, force: true }));
    const keyPath = join(dir, "secrets.key");
    const moduleUrl = pathToFileURL(join(process.cwd(), "server", "security", "secretCrypto.mjs")).href;
    const script = `
        import { createSecretCrypto, nativeApiKeyAad } from ${JSON.stringify(moduleUrl)};
        const crypto = createSecretCrypto({ keyPath: process.argv[1], protectKey: false });
        process.stdout.write(crypto.sealAtRest("shared-secret", nativeApiKeyAad()));
    `;

    const results = await Promise.all(Array.from({ length: 8 }, () => execFileAsync(
        process.execPath,
        ["--input-type=module", "--eval", script, keyPath],
    )));
    const verifier = createSecretCrypto({ keyPath, protectKey: false });
    for (const { stdout } of results) {
        assert.equal(verifier.openAtRest(stdout, nativeApiKeyAad()), "shared-secret");
    }
});
