/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { RSA_TRANSIT_ALG, SECRET_PLACEHOLDER, isSecretEnvKey } from './secretFields';

export { RSA_TRANSIT_ALG, SECRET_PLACEHOLDER, isSecretEnvKey };

export interface TransitSecretEnvelope {
    v: 1;
    alg: typeof RSA_TRANSIT_ALG;
    ek: string;
    n: string;
    ct: string;
}

export interface AgentConfigCrypto {
    alg: string;
    publicKey: string;
}

export const isTransitEnvelope = (value: unknown): value is TransitSecretEnvelope => {
    if (!value || typeof value !== 'object') return false;
    const envelope = value as TransitSecretEnvelope;
    if (envelope.v !== 1 || typeof envelope.n !== 'string' || typeof envelope.ct !== 'string') return false;
    return envelope.alg === RSA_TRANSIT_ALG && typeof envelope.ek === 'string';
};

export const sealSecret = async (
    cryptoConfig: AgentConfigCrypto,
    plaintext: string,
): Promise<TransitSecretEnvelope> => {
    if (!globalThis.crypto?.subtle) {
        throw new Error('This page is not a secure context, so API keys cannot be encrypted. Use HTTPS or localhost.');
    }
    if (cryptoConfig.alg !== RSA_TRANSIT_ALG) {
        throw new Error(`Unsupported Agent secret encryption algorithm: ${cryptoConfig.alg}`);
    }
    return sealSecretRsa(cryptoConfig.publicKey, plaintext);
};

const sealSecretRsa = async (rsaPublicKeyB64: string, plaintext: string): Promise<TransitSecretEnvelope> => {
    const aesKeyBytes = crypto.getRandomValues(new Uint8Array(32));
    const aesKey = await crypto.subtle.importKey('raw', aesKeyBytes, 'AES-GCM', false, ['encrypt']);
    const rsaKey = await crypto.subtle.importKey(
        'spki',
        bytesFromB64(rsaPublicKeyB64),
        { name: 'RSA-OAEP', hash: 'SHA-256' },
        false,
        ['encrypt'],
    );
    const nonce = crypto.getRandomValues(new Uint8Array(12));
    const packed = new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-GCM', iv: nonce }, aesKey, new TextEncoder().encode(plaintext)));
    const wrapped = new Uint8Array(await crypto.subtle.encrypt({ name: 'RSA-OAEP' }, rsaKey, aesKeyBytes));
    return {
        v: 1,
        alg: RSA_TRANSIT_ALG,
        ek: bytesToB64(wrapped),
        n: bytesToB64(nonce),
        ct: bytesToB64(packed),
    };
};

export const sealSecretInput = async (
    value: string | TransitSecretEnvelope | undefined,
    cryptoConfig: AgentConfigCrypto | undefined,
): Promise<string | TransitSecretEnvelope | undefined> => {
    if (value === undefined || value === SECRET_PLACEHOLDER || value === '' || isTransitEnvelope(value)) {
        return value;
    }
    if (!cryptoConfig?.publicKey) {
        throw new Error('Agent secret public key is unavailable');
    }
    return sealSecret(cryptoConfig, value);
};

export const sealEnvSecrets = async (
    env: Record<string, string | TransitSecretEnvelope>,
    cryptoConfig: AgentConfigCrypto | undefined,
): Promise<Record<string, string | TransitSecretEnvelope>> => {
    const next: Record<string, string | TransitSecretEnvelope> = {};
    for (const [key, value] of Object.entries(env)) {
        next[key] = isSecretEnvKey(key)
            ? await sealSecretInput(value, cryptoConfig) ?? ''
            : value;
    }
    return next;
};

const bytesToB64 = (bytes: Uint8Array): string => {
    let binary = '';
    bytes.forEach((byte) => {
        binary += String.fromCharCode(byte);
    });
    return btoa(binary);
};

const bytesFromB64 = (value: string): Uint8Array => {
    const binary = atob(value);
    return Uint8Array.from(binary, (char) => char.charCodeAt(0));
};
