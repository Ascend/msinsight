/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
export const SECRET_PLACEHOLDER = '******';
export const RSA_TRANSIT_ALG = 'rsa-oaep-aes-gcm-v1';
export const SECRET_ENV_RE = /(?:API_?KEY|ACCESS_?KEY|PRIVATE_?KEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIALS?|(?:^|_)PAT(?:_|$))/i;

export const isSecretEnvKey = (key: string): boolean => SECRET_ENV_RE.test(key);
