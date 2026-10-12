/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    constants,
    createCipheriv,
    createDecipheriv,
    createPublicKey,
    generateKeyPairSync,
    privateDecrypt,
    publicEncrypt,
    randomBytes,
} from "node:crypto";
import { chmodSync, existsSync, linkSync, mkdirSync, readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";
import { homedir, tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { SECRET_PLACEHOLDER, RSA_TRANSIT_ALG, isSecretEnvKey, isTransitEnvelope } from "./secretProjection.mjs";
import { protectCurrentUser, unprotectCurrentUser } from "./windowsDpapi.mjs";

export { RSA_TRANSIT_ALG };
const AT_REST_PREFIX = "enc:v1:";
const DPAPI_MAGIC = Buffer.from("MS1D");
const RAW_KEY_BYTES = 32;

export class SecretCryptoError extends Error {
    constructor(code, message, status = 400) {
        super(message);
        this.name = "SecretCryptoError";
        this.code = code;
        this.status = status;
    }
}

export const resolveSecretsKeyPath = () => {
    if (process.env.MSINSIGHT_SECRETS_KEY_PATH) return process.env.MSINSIGHT_SECRETS_KEY_PATH;
    if (process.env.NODE_TEST_CONTEXT) return join(tmpdir(), `msinsight-insight-secrets-${process.pid}`, "secrets.key");
    if (process.platform === "win32") {
        return join(process.env.APPDATA || join(homedir(), "AppData", "Roaming"), "MindStudioInsight", "secrets.key");
    }
    if (process.platform === "darwin") {
        return join(homedir(), "Library", "Application Support", "MindStudioInsight", "secrets.key");
    }
    return join(process.env.XDG_CONFIG_HOME || join(homedir(), ".config"), "mindstudio-insight", "secrets.key");
};

export const createSecretCrypto = ({
    keyPath = resolveSecretsKeyPath(),
    protectKey = process.platform === "win32" && process.env.MSINSIGHT_SECRETS_DPAPI !== "0",
} = {}) => {
    const wrappingKey = loadOrCreateWrappingKey(keyPath, protectKey);
    const rsaTransit = generateKeyPairSync("rsa", { modulusLength: 2048 });
    const rsaPublicKey = rsaTransit.publicKey.export({ type: "spki", format: "der" });

    const publicCrypto = () => ({
        alg: RSA_TRANSIT_ALG,
        publicKey: rsaPublicKey.toString("base64"),
    });

    const sealAtRest = (plaintext, aad) => {
        if (!plaintext) return "";
        const iv = randomBytes(12);
        const cipher = createCipheriv("aes-256-gcm", wrappingKey, iv);
        cipher.setAAD(Buffer.from(String(aad), "utf8"));
        const ciphertext = Buffer.concat([cipher.update(String(plaintext), "utf8"), cipher.final()]);
        return `${AT_REST_PREFIX}${iv.toString("base64url")}:${cipher.getAuthTag().toString("base64url")}:${ciphertext.toString("base64url")}`;
    };

    const openAtRest = (value, aad) => {
        if (!value) return "";
        if (!isAtRestEnvelope(value)) {
            throw new SecretCryptoError("secret_ciphertext_invalid", "Stored secret is not a recognized ciphertext envelope", 500);
        }
        const parts = String(value).slice(AT_REST_PREFIX.length).split(":");
        if (parts.length !== 3) {
            throw new SecretCryptoError("secret_ciphertext_invalid", "Stored secret ciphertext is corrupt", 500);
        }
        try {
            const decipher = createDecipheriv("aes-256-gcm", wrappingKey, Buffer.from(parts[0], "base64url"));
            decipher.setAAD(Buffer.from(String(aad), "utf8"));
            decipher.setAuthTag(Buffer.from(parts[1], "base64url"));
            return Buffer.concat([
                decipher.update(Buffer.from(parts[2], "base64url")),
                decipher.final(),
            ]).toString("utf8");
        } catch (error) {
            throw new SecretCryptoError("secret_ciphertext_invalid", error.message, 500);
        }
    };

    const persistIfPlain = (value, aad) => {
        if (!value) return { value: "", changed: false };
        if (isAtRestEnvelope(value)) return { value, changed: false };
        return { value: sealAtRest(value, aad), changed: true };
    };

    const unwrapTransit = (envelope) => {
        if (!isTransitEnvelope(envelope)) {
            throw new SecretCryptoError("crypto_unwrap_failed", "Secret envelope is missing or invalid");
        }
        try {
            return unwrapRsaTransit(rsaTransit.privateKey, envelope);
        } catch {
            throw new SecretCryptoError("crypto_unwrap_failed", "Secret envelope could not be decrypted");
        }
    };

    const sealTransit = (plaintext) => sealRsaTransitForPublicKey(rsaPublicKey, plaintext);

    const resolveIncomingSecret = (input, currentAtRest, aad) => {
        if (input === undefined || input === null) return currentAtRest ?? "";
        if (input === SECRET_PLACEHOLDER) {
            if (currentAtRest) return currentAtRest;
            throw new SecretCryptoError("secret_placeholder_invalid", "A secret placeholder cannot create a new secret");
        }
        if (input === "") return "";
        if (isTransitEnvelope(input)) {
            const plaintext = unwrapTransit(input);
            return plaintext ? sealAtRest(plaintext, aad) : "";
        }
        throw new SecretCryptoError("plaintext_secret_rejected", "Plaintext secrets are not accepted");
    };

    const resolveIncomingEnv = (inputEnv = {}, currentEnv = {}, agentName) => {
        const resolved = {};
        for (const [key, value] of Object.entries(inputEnv)) {
            if (isTransitEnvelope(value) || isSecretEnvKey(key)) {
                resolved[key] = resolveIncomingSecret(value, currentEnv[key], envAad(agentName, key));
            } else {
                resolved[key] = String(value ?? "");
            }
        }
        return resolved;
    };

    return {
        publicCrypto,
        sealAtRest,
        openAtRest,
        persistIfPlain,
        unwrapTransit,
        sealTransit,
        resolveIncomingSecret,
        resolveIncomingEnv,
        keyPath,
    };
};

export const isAtRestEnvelope = (value) => String(value ?? "").startsWith(AT_REST_PREFIX);

export const nativeApiKeyAad = () => "msinsight-native.json|apiKey";

export const envAad = (agentName, key) => `agent-servers.json|${agentName}|env|${key}`;

export const decryptEnv = (env = {}, agentName, crypto) => Object.fromEntries(Object.entries(env).map(([key, value]) => {
    if (!isSecretEnvKey(key)) return [key, String(value ?? "")];
    return [key, crypto.openAtRest(value, envAad(agentName, key))];
}));

export const persistEnvIfPlain = (env = {}, agentName, crypto) => {
    let changed = false;
    const next = {};
    for (const [key, value] of Object.entries(env)) {
        if (!isSecretEnvKey(key)) {
            next[key] = String(value ?? "");
            continue;
        }
        const persisted = crypto.persistIfPlain(value, envAad(agentName, key));
        next[key] = persisted.value;
        if (persisted.changed) changed = true;
    }
    return { env: next, changed };
};

export const migratePersistedSecrets = ({ nativeConfig, agentServersConfig, nativeConfigPath, agentServersConfigPath, writeJson, crypto }) => {
    const apiKey = crypto.persistIfPlain(nativeConfig?.apiKey, nativeApiKeyAad());
    const nextNative = apiKey.changed ? { ...nativeConfig, apiKey: apiKey.value } : nativeConfig;
    if (apiKey.changed) writeJson(nativeConfigPath, nextNative);

    const servers = Array.isArray(agentServersConfig?.agentServers) ? agentServersConfig.agentServers : [];
    let serversChanged = false;
    const nextServers = servers.map((server) => {
        const persisted = persistEnvIfPlain(server.env, server.name, crypto);
        if (persisted.changed) serversChanged = true;
        return persisted.changed ? { ...server, env: persisted.env } : server;
    });
    const nextAgentConfig = serversChanged ? { ...agentServersConfig, agentServers: nextServers } : agentServersConfig;
    if (serversChanged) writeJson(agentServersConfigPath, nextAgentConfig);
    return { nativeConfig: nextNative, agentServersConfig: nextAgentConfig };
};

export const sealRsaTransitForPublicKey = (spkiPublicKey, plaintext) => {
    const aesKey = randomBytes(32);
    const rsaKey = createPublicKey({
        key: Buffer.isBuffer(spkiPublicKey) ? spkiPublicKey : Buffer.from(spkiPublicKey, "base64"),
        format: "der",
        type: "spki",
    });
    const { n, ct } = sealPackedAesGcm(aesKey, plaintext);
    return {
        v: 1,
        alg: RSA_TRANSIT_ALG,
        ek: publicEncrypt({
            key: rsaKey,
            padding: constants.RSA_PKCS1_OAEP_PADDING,
            oaepHash: "sha256",
        }, aesKey).toString("base64"),
        n,
        ct,
    };
};

const sealPackedAesGcm = (key, plaintext) => {
    const nonce = randomBytes(12);
    const cipher = createCipheriv("aes-256-gcm", key, nonce);
    const ciphertext = Buffer.concat([cipher.update(String(plaintext), "utf8"), cipher.final()]);
    return {
        n: nonce.toString("base64"),
        ct: Buffer.concat([ciphertext, cipher.getAuthTag()]).toString("base64"),
    };
};

const openPackedAesGcm = (key, envelope) => {
    const packed = Buffer.from(envelope.ct, "base64");
    const nonce = Buffer.from(envelope.n, "base64");
    const tag = packed.subarray(packed.length - 16);
    const ciphertext = packed.subarray(0, packed.length - 16);
    const decipher = createDecipheriv("aes-256-gcm", key, nonce);
    decipher.setAuthTag(tag);
    return Buffer.concat([decipher.update(ciphertext), decipher.final()]).toString("utf8");
};

const unwrapRsaTransit = (privateKey, envelope) => {
    const aesKey = privateDecrypt({
        key: privateKey,
        padding: constants.RSA_PKCS1_OAEP_PADDING,
        oaepHash: "sha256",
    }, Buffer.from(envelope.ek, "base64"));
    return openPackedAesGcm(aesKey, envelope);
};

const loadOrCreateWrappingKey = (keyPath, protectKey) => {
    mkdirSync(dirname(keyPath), { recursive: true, mode: 0o700 });
    if (existsSync(keyPath)) {
        const stored = readFileSync(keyPath);
        const key = decodeWrappingKey(stored, protectKey);
        if (protectKey && stored.length === RAW_KEY_BYTES) replaceWrappingKey(keyPath, key, true);
        return key;
    }
    const key = randomBytes(RAW_KEY_BYTES);
    const tempPath = `${keyPath}.${process.pid}.${randomBytes(6).toString("hex")}.tmp`;
    writeWrappingKey(tempPath, key, protectKey);
    try {
        // Linking a fully-written file publishes the key atomically without replacing a winner.
        linkSync(tempPath, keyPath);
        return key;
    } catch (error) {
        if (error.code === "EEXIST") return decodeWrappingKey(readFileSync(keyPath), protectKey);
        throw error;
    } finally {
        try {
            unlinkSync(tempPath);
        } catch {
            // The key has already been published or the original error is more useful.
        }
    }
};

const replaceWrappingKey = (keyPath, key, protectKey) => {
    const tempPath = `${keyPath}.${process.pid}.${randomBytes(6).toString("hex")}.tmp`;
    try {
        writeWrappingKey(tempPath, key, protectKey);
        renameSync(tempPath, keyPath);
    } finally {
        try {
            unlinkSync(tempPath);
        } catch {
            // The temporary file was renamed or never created.
        }
    }
};

const writeWrappingKey = (keyPath, key, protectKey) => {
    writeFileSync(keyPath, encodeWrappingKey(key, protectKey), { mode: 0o600 });
    try {
        chmodSync(keyPath, 0o600);
    } catch {
        // Windows ignores POSIX modes; the write still succeeds.
    }
};

const encodeWrappingKey = (key, protectKey) => {
    if (!protectKey) return key;
    try {
        return Buffer.concat([DPAPI_MAGIC, protectCurrentUser(key)]);
    } catch (error) {
        throw new SecretCryptoError("secrets_key_protection_failed", `Wrapping key could not be protected: ${error.message}`, 500);
    }
};

const decodeWrappingKey = (stored, protectKey) => {
    if (stored.length === RAW_KEY_BYTES) return stored;
    if (stored.subarray(0, DPAPI_MAGIC.length).equals(DPAPI_MAGIC)) {
        if (!protectKey) {
            throw new SecretCryptoError("secrets_key_invalid", "Wrapping key is DPAPI-protected but MSINSIGHT_SECRETS_DPAPI is not enabled", 500);
        }
        try {
            const key = unprotectCurrentUser(stored.subarray(DPAPI_MAGIC.length));
            if (key.length !== RAW_KEY_BYTES) throw new Error("unprotected key has an invalid length");
            return key;
        } catch (error) {
            throw new SecretCryptoError("secrets_key_invalid", `Wrapping key could not be unprotected: ${error.message}`, 500);
        }
    }
    throw new SecretCryptoError("secrets_key_invalid", "Wrapping key file is corrupt", 500);
};
