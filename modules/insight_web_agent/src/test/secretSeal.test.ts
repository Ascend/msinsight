/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { isSecretEnvKey, RSA_TRANSIT_ALG, sealSecret } from '../secretSeal';

const fromBase64 = (value: string): Uint8Array => {
    const binary = atob(value);
    return Uint8Array.from(binary, (char) => char.charCodeAt(0));
};

test('RSA transit envelopes decrypt with the matching private key', async () => {
    const pair = await crypto.subtle.generateKey({
        name: 'RSA-OAEP',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256',
    }, true, ['encrypt', 'decrypt']) as CryptoKeyPair;
    const publicKey = new Uint8Array(await crypto.subtle.exportKey('spki', pair.publicKey));
    const publicKeyB64 = btoa(String.fromCharCode(...publicKey));

    const envelope = await sealSecret({ alg: RSA_TRANSIT_ALG, publicKey: publicKeyB64 }, 'browser-secret');
    const aesKeyBytes = await crypto.subtle.decrypt({ name: 'RSA-OAEP' }, pair.privateKey, fromBase64(envelope.ek));
    const aesKey = await crypto.subtle.importKey('raw', aesKeyBytes, 'AES-GCM', false, ['decrypt']);
    const plaintext = await crypto.subtle.decrypt(
        { name: 'AES-GCM', iv: fromBase64(envelope.n) },
        aesKey,
        fromBase64(envelope.ct),
    );

    expect(new TextDecoder().decode(plaintext)).toBe('browser-secret');
});

test('recognizes common secret environment variable names without classifying auth URLs', () => {
    for (const key of ['OPENAI_API_KEY', 'APIKEY', 'AWS_ACCESS_KEY_ID', 'SSH_PRIVATE_KEY', 'GITHUB_PAT']) {
        expect(isSecretEnvKey(key)).toBe(true);
    }
    expect(isSecretEnvKey('AUTH_URL')).toBe(false);
    expect(isSecretEnvKey('ACP_DEBUG')).toBe(false);
});
