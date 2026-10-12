/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
export const SECRET_PLACEHOLDER = "******";
export const RSA_TRANSIT_ALG = "rsa-oaep-aes-gcm-v1";
export const SECRET_ENV_RE = /(?:API_?KEY|ACCESS_?KEY|PRIVATE_?KEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIALS?|(?:^|_)PAT(?:_|$))/i;

export const isSecretEnvKey = (key) => SECRET_ENV_RE.test(String(key ?? ""));

export const isTransitEnvelope = (value) => Boolean(
    value
    && typeof value === "object"
    && !Array.isArray(value)
    && value.v === 1
    && typeof value.n === "string"
    && typeof value.ct === "string"
    && value.alg === RSA_TRANSIT_ALG
    && typeof value.ek === "string",
);

export const projectSecretValue = (value) => (value ? SECRET_PLACEHOLDER : "");

export const projectEnv = (env = {}) => {
    const projected = {};
    for (const [key, value] of Object.entries(env)) {
        if (isSecretEnvKey(key) && value) {
            projected[key] = SECRET_PLACEHOLDER;
        } else {
            projected[key] = String(value ?? "");
        }
    }
    return projected;
};

export const projectBuiltinAgent = (config) => ({
    ...config,
    apiKey: projectSecretValue(config.apiKey),
});

export const projectAgentServer = (server) => ({ ...server, env: projectEnv(server.env) });

export const projectSnapshot = (snapshot, crypto) => ({
    ...snapshot,
    builtinAgent: projectBuiltinAgent(snapshot.builtinAgent),
    agentServers: snapshot.agentServers.map(projectAgentServer),
    catalogAgents: Array.isArray(snapshot.catalogAgents)
        ? snapshot.catalogAgents.map(projectAgentServer)
        : snapshot.catalogAgents,
    crypto: crypto.publicCrypto(),
});
